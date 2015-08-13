#ifndef THREAD_H
#define THREAD_H

#include <queue>
#include <vector>
#include <thread>
#include <condition_variable>
#include "console/windows_wrap.h"
#include "common/ll-deque.h"
#include "types.h"

template <typename Tvar> class ThreadPool;
template <typename Tvar> class ThreadedTask;
template <typename Tvar> struct ThreadParams;

template <typename Tvar>
class Task
{
    public:
        Task() { }
        Task(const volatile Task<Tvar> &other) { *this = other; }
        Task(void (*a)(Tvar *, void *), void *b) { func = a; param = b; }
        void operator=(const Task &other) { func = other.func; param = other.param; }
        void operator=(const Task &other) volatile { func = other.func; param = other.param; }
        void operator=(const volatile Task &other) volatile { func = other.func; param = other.param; }

        void (*func)(Tvar *, void *);
        void *param;
};

template <typename Tvar>
class Thread
{
    public:
        Thread()
        {
            thread_variables = nullptr;
        }
        void Init(void *parent, unsigned long (*ThreadProc)(void *))
        {
            ThreadParams<Tvar> *params = new ThreadParams<Tvar>;
            cv.reset(new std::condition_variable);
            mutex.reset(new std::mutex);
            params->parent = parent;
            params->thread = this;
            params->vars = new Tvar;
            running.store(true, std::memory_order_release);
            thread = std::move(std::thread(ThreadProc, params));
        }

        ~Thread()
        {
            delete thread_variables;
        }

        Thread(const Thread &other) = delete;

        std::atomic<bool> running;
        std::thread thread;
        Task<Tvar> task;
        ptr<std::mutex> mutex;
        ptr<std::condition_variable> cv;
        Tvar *thread_variables;
};

template <typename Tvar>
struct ThreadParams
{
    void *parent;
    Thread<Tvar> *thread;
    Tvar *vars;
};


template <typename Tvar>
class PoolThread : public Thread<Tvar>
{
    public:
        PoolThread()
        {
            should_sleep.store(false, std::memory_order_relaxed);
        }
        void Init(ThreadPool<Tvar> *pool)
        {
            next_free = nullptr;
            Thread<Tvar>::Init(pool, &PoolThreadProc);
        }
        ~PoolThread() {}
        PoolThread(const PoolThread &other) = delete;

        void Wake()
        {
            {
                std::lock_guard<std::mutex> lock(*this->mutex);
                this->running.store(true, std::memory_order_release);
            }
            this->cv->notify_one();
        }

        bool IsRunning() const { return this->running.load(std::memory_order_acquire); }

    private:
        static unsigned long PoolThreadProc(void *param)
        {
            ThreadParams<Tvar> *params = (ThreadParams<Tvar> *)param;
            ThreadPool<Tvar> *pool = (ThreadPool<Tvar> *)params->parent;
            PoolThread<Tvar> *thread = (PoolThread<Tvar> *)params->thread;
            thread->thread_variables = params->vars;
            delete params;

            while (true)
            {
                while (!pool->RequestTask(thread))
                    thread->Sleep(); // Täs pitäs olla joku if (killed) break;
                auto param = thread->task.param;
                (*thread->task.func)(thread->thread_variables, param);
            }
            return 0;
        }

        void Sleep()
        {
            // RequestTask sets runnign to false as it has to be done before adding thread to idle list
            std::unique_lock<std::mutex> lock(*this->mutex);
            this->cv->wait(lock, [this](){ return this->running.load(std::memory_order_acquire) == true; });
            should_sleep.store(false, std::memory_order_relaxed);
        }

    public:
        std::atomic<bool> should_sleep;
        PoolThread *next_free;
};

template <typename Tvar>
class ThreadPool
{
    public:
        ThreadPool()
        {
            state.store(0, std::memory_order_relaxed);
            sleep_count.store(0, std::memory_order_relaxed);
            free_threads.store(nullptr, std::memory_order_relaxed);
        }
        ThreadPool(int size)
        {
            Init(size);
        }

        void Init(int size)
        {
            for (int i = 0; i < size; i++)
            {
                threads.emplace_back(new PoolThread<Tvar>);
            }
            for (auto &thread : threads)
            {
                thread->Init(this);
            }
        }
        ~ThreadPool() {}

        int GetThreadCount() { return threads.size(); }

        void ClearAll()
        {
            Task<Tvar> tmp;
            tasks.clear();
            for (auto &thread : threads)
            {
                thread->should_sleep.store(true, std::memory_order_relaxed);
            }
            for (auto &thread : threads)
            {
                while (thread->IsRunning())
                {
                    Sleep(0);
                    // The first should_sleep may get overwritten if thread had just awaken
                    thread->should_sleep.store(true, std::memory_order_relaxed);
                }
            }
        }

        // Ironically, this function can only be called from one thread at time
        template <typename Param>
        void AddTask(void (*func)(Tvar *, Param *), Param *param)
        {
            auto old_state = state.load(std::memory_order_acquire);
            tasks.push_back(Task<Tvar>((void (*)(Tvar *, void *))func, param));
            while (true)
            {
                PoolThread<Tvar> *thread = free_threads.load(std::memory_order_acquire);
                if (thread == ThreadsBusy)
                {
                    // Return if no workers have gone sleep during this
                    if (state.compare_exchange_strong(old_state, old_state + 1,
                            std::memory_order_release, std::memory_order_relaxed) == true)
                    {
                        return;
                    }
                    // However, the sleeping thread might not have yet added itself to the free_threads list
                    while (thread == ThreadsBusy)
                        thread = free_threads.load(std::memory_order_acquire);
                }
                // Now there is at least one sleeping thread, fight until we get one of them
                while (true)
                {
                    if (free_threads.compare_exchange_strong(thread, thread->next_free,
                            std::memory_order_release, std::memory_order_relaxed) == true)
                    {
                        thread->Wake();
                        return;
                    }
                }
            }
        }

        bool RequestTask(PoolThread<Tvar> *thread)
        {
            Task<Tvar> task;
            auto old_state = state.load(std::memory_order_acquire);
            while (true)
            {
                int spincount = 1000;
                while (spincount--)
                {
                    if (tasks.pop_front(&thread->task)) // Success
                    {
                        return true;
                    }
                    if (thread->should_sleep.load(std::memory_order_relaxed) == true || SwitchToThread() == 0)
                        break;
                }
                // Either new task has been added or false alarm (Other worker thread doing same thing)
                if (state.compare_exchange_strong(old_state, old_state + 1, std::memory_order_release, std::memory_order_acquire) == false)
                    continue;
                thread->running.store(false, std::memory_order_release);
                if (PerfTest)
                    sleep_count.fetch_add(1, std::memory_order_relaxed);
                PoolThread<Tvar> *free_threads_head = free_threads.load(std::memory_order_acquire);
                while (true)
                {
                    thread->next_free = free_threads_head;
                    if (free_threads.compare_exchange_strong(free_threads_head, thread, std::memory_order_release, std::memory_order_relaxed) == true)
                        return false;
                }
            }
        }

        template <typename Func>
        void ForEachThread(Func func)
        {
            for (auto &thread : threads)
                func(thread->thread_variables);
        }

        // Can be used to tell how many sleep calls have occured
        int GetSleepCount() { return sleep_count.load(std::memory_order_relaxed); }

    private:
        std::atomic<unsigned int> sleep_count;
        std::vector<ptr<PoolThread<Tvar>>> threads;
        Common::LocklessQueue<Task<Tvar>> tasks;

        std::atomic<PoolThread<Tvar> *> free_threads; // linked list head
        std::atomic<unsigned int> state;
        static constexpr PoolThread<Tvar> *const ThreadsBusy = (PoolThread<Tvar> *)0x0;
};

#endif // THREAD_H
