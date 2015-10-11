#ifndef LL_DEQUE_H
#define LL_DEQUE_H

#include <atomic>

namespace Common
{
// Hah, lockless queue onki vaa mitä tarten ja helpompi tehä/nopeempi
// One producer, multiple consumers
template <class C>
class LocklessQueue
{
    static const int ChunkSize = 16;
    static const int FreeChunkLimit = 4;
    class Chunk
    {
        public:
            Chunk()
            {
                used_entries.store(ChunkSize, std::memory_order_relaxed);
                next = nullptr;
            }

            bool Contains(C *entry)
            {
                char *entry_bytes = (char *)entry;
                char *arr_bytes = (char *)entries;
                if (arr_bytes <= entry_bytes && arr_bytes + sizeof(C) * ChunkSize > entry_bytes)
                    return true;
                return false;
            }

            C *GetNext(C *prev)
            {
                if (prev == entries + ChunkSize - 1)
                    return next->entries;
                return prev + 1;
            }

            C *GetNextIfInSelf(C *prev)
            {
                if (prev == entries + ChunkSize - 1)
                    return nullptr;
                return prev + 1;
            }

            int GetUsedEntriesFrom(C *entry)
            {
                return ChunkSize - (entry - entries);
            }

            Chunk *next;
            std::atomic<int> used_entries;
            C entries[ChunkSize];
    };

    public:
        LocklessQueue()
        {
            const auto relaxed = std::memory_order_relaxed;
            Chunk *chunk = new Chunk;
            head_chunk.store(chunk, relaxed);
            write_chunk = chunk;
            tail_chunk.store(chunk, relaxed);
            C *first = chunk->entries;
            read_pos.store(first, relaxed);
            read_end.store(first, relaxed);
            free_chunks.store(0, relaxed);
        }

        bool pop_front(C *out)
        {
            const auto relaxed = std::memory_order_relaxed;
            const auto release = std::memory_order_release;
            C *pos = read_pos.load(relaxed);
            Chunk *chunk = head_chunk.load(relaxed);
            while (true)
            {
                if (pos == read_end.load(relaxed))
                    return false;
                if (chunk->Contains(pos))
                {
                    C *next = chunk->GetNext(pos);
                    if (read_pos.compare_exchange_weak(pos, next, relaxed, relaxed) == true)
                        break;
                }
                else
                {
                    pos = read_pos.load(relaxed);
                }
                chunk = head_chunk.load(relaxed);
            }
            *out = *pos;
            // Decrement chunk used_entries, if free, maybe make tail
            if (chunk->used_entries.fetch_sub(1, release) == 1)
            {
                // This may cause small stall, as other threads will fail in chunk->Contains() line above
                // until this store is done
                head_chunk.store(chunk->next, relaxed);
                // FreeChunkLimit is not hard limit, multiple threads passing this check simultaneously
                // might increase free_chunks above FreeChunkLimit
                if (free_chunks.load(relaxed) < FreeChunkLimit)
                {
                    free_chunks.fetch_add(1, relaxed);
                    Chunk *old_tail = tail_chunk.load(relaxed);
                    chunk->used_entries.store(ChunkSize, relaxed);
                    chunk->next = nullptr;
                    do ; while (tail_chunk.compare_exchange_weak(old_tail, chunk, release, relaxed) == false);
                    old_tail->next = chunk;
                }
                else
                    delete chunk;
            }
            return true;
        }

        void push_back(const C &val)
        {
            const auto relaxed = std::memory_order_relaxed;
            const auto release = std::memory_order_release;
            C *pos = read_end.load(relaxed);
            Chunk *chunk = write_chunk;
            Chunk *allocated = nullptr;
            C *next = chunk->GetNextIfInSelf(pos);
            *pos = val;
            if (next)
            {
                read_end.store(next, release);
                return;
            }
            else
            {
                while (true)
                {
                    Chunk *next_chunk = chunk->next;
                    if (!next_chunk)
                    {
                        if (!allocated)
                            allocated = new Chunk;
                        if (tail_chunk.compare_exchange_strong(chunk, allocated, relaxed, relaxed) == false)
                            continue;
                        chunk->next = allocated;
                        next_chunk = allocated;
                        allocated = nullptr;
                    }
                    else
                    {
                        free_chunks.fetch_sub(1, relaxed);
                        next_chunk->used_entries.store(ChunkSize, release);
                    }
                    write_chunk = next_chunk;
                    read_end.store(next_chunk->entries, release);
                    break;
                }
                delete allocated; // If we couldn't use it after all
            }
        }

        void clear()
        {
            const auto relaxed = std::memory_order_relaxed;
            const auto release = std::memory_order_release;
            C *old_read_pos = read_pos.load(relaxed);
            C *end = read_end.load(relaxed);
            Chunk *chunk = head_chunk.load(relaxed);
            // Move read_pos to read_end, and if a consumer managed to pop something before, try again
            while (true)
            {
                if (chunk->Contains(old_read_pos))
                {
                    if (read_pos.compare_exchange_strong(old_read_pos, end, release, relaxed) == true)
                        break;
                }
                old_read_pos = read_pos.load(relaxed);
                chunk = head_chunk.load(relaxed);
            }
            // Now consumers cannot do anything anymore
            head_chunk.store(write_chunk, release);
            // Free chunks until we reach write_chunk
            Chunk *original_head = chunk;
            while (chunk != write_chunk)
            {
                if (chunk == original_head)
                {
                    int used_entries = chunk->GetUsedEntriesFrom(old_read_pos);
                    do ; while (chunk->used_entries.load(relaxed) != used_entries);
                }
                Chunk *next = chunk->next;
                delete chunk;
                chunk = next;
            }
            // If original_head == chunk (i.e write_chunk->Contains(old_read_pos)),
            // the consumer threads may do their respective fetch_sub if they are in midst of pop_front,
            // and we have to do ours as well. Otherwise just set used_entries to the correct value
            // The fetch_sub cannot cause chunk to be freed, as it is also the write_chunk and such
            // has at least one empty entry
            if (original_head != chunk)
                chunk->used_entries.store(chunk->GetUsedEntriesFrom(end), release);
            else
                chunk->used_entries.fetch_sub(end - old_read_pos, relaxed);
        }

        bool empty()
        {
            const auto acquire = std::memory_order_acquire;
            return read_pos.load(acquire) == read_end.load(acquire);
        }

    private:
        std::atomic<C *> read_pos;
        std::atomic<C *> read_end;
        std::atomic<Chunk *> head_chunk; // Contains read_pos
        Chunk *write_chunk;
        std::atomic<Chunk *> tail_chunk; // May not contain read_end, there may be empty chunks ready
        std::atomic<int> free_chunks;
};
}
#endif /* LL_DEQUE_H */
