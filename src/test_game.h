#ifndef TEST_GAME_H
#define TEST_GAME_H

#include "types.h"
#include <string>

// The .cpp file is only compiled in debug builds, so generate empty stubs otherwise
// This allows any interface typos etc get caught by compiler in release builds as well
// (Doubt it increases compile times noticeably)
#ifdef DEBUG
#define DEBUG_MEM_FN(s) s;
#else
#define DEBUG_MEM_FN(s) s {}
#endif

class GameTest
{
    friend class GameTests;
    protected:
        GameTest() : state(0), frames_remaining(20000), status(Status::Running) {}
        virtual void Init() = 0;
        virtual void Done() {}
        virtual void NextFrame() = 0;

        void Fail(std::string reason) { status = Status::Failed; fail_reason = move(reason); }
        void Pass() { status = Status::Passed; }

        Unit *CreateUnitForTest(UnitType unit_id, int player);
        Unit *CreateUnitForTestAt(UnitType unit_id, int player, const Point &point);

        const char *name;
        int id;
        // Because nearly every test needs some kind of state tracking
        int state;

        int frames_remaining;
        enum class Status {
            Running,
            Failed,
            Passed
        } status;
        std::string fail_reason;
};

class GameTests
{
    public:
        DEBUG_MEM_FN(GameTests())
        DEBUG_MEM_FN(void RunTests(int first, int last, int repeat))
        DEBUG_MEM_FN(void NextFrame())

    private:
        void AddTest(const char *name, GameTest *test);
        void NextTest();
        void CheckTest();
        void StartTest();
        vector<ptr<GameTest>> tests;
        int current_test;
        int first_test;
        int last_test;
        int repeat;
        // Init waiting is always fast forwarded through, but if the test
        // was started without fast forward, revert it back once ready.
        int orig_render_wait;
};

#undef DEBUG_MEM_FN

#endif /* TEST_GAME_H */
