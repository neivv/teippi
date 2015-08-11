#ifndef LOG_FREEZE_H
#define LOG_FREEZE_H

#include <windows.h>
#include <string>
#include <atomic>
#include <functional>
#include <stdio.h>

// Win32 only supported
class FreezeLogger {
    public:
        template <class ErrorHandler>
        FreezeLogger(HANDLE thread, std::string path, std::function<bool()> &&func, ErrorHandler error_handler)
            : watched_thread(thread), is_frozen(move(func)), dump_path(path)
        {
            watchdog_thread = CreateThread(NULL, 0, &FreezeLogger::ThreadProc, this, 0, NULL);
            if (watchdog_thread == NULL) {
                char buf[64];
                auto error = GetLastError();
                snprintf(buf, sizeof buf, "Createthread failed: %x %d", error, error);
                error_handler(buf);
            }
        }

        ~FreezeLogger() {
            CloseHandle(watched_thread);
        }

    private:
        HANDLE watchdog_thread;
        HANDLE watched_thread;
        std::function<bool()> is_frozen;
        std::string dump_path;

        void DumpThreadState() {
            FILE *dump = fopen(dump_path.c_str(), "a");
            if (!dump) {
                // What to do???
                MessageBoxA(0, "Could not open freeze log", ":(", 0);
                return;
            }
            char buf[64];
            time_t time_ = time(0);
            struct tm *time = localtime(&time_);
            strftime(buf, sizeof buf, "%Y-%m-%d %H:%M:%S\n", time);
            fputs(buf, dump);
            fputs("Main thread has frozen. Trying to log state...\n", dump);

            CONTEXT ctx;
            ctx.ContextFlags = CONTEXT_INTEGER | CONTEXT_CONTROL;
            SuspendThread(watched_thread);
            auto success = GetThreadContext(watched_thread, &ctx);
            if (success == 0) {
                auto error = GetLastError();
                fprintf(dump, "Could not query thread context: %x %d\n", error, error);
            } else {
                DumpRegisters(dump, &ctx);
                fputs("\nStack:\n", dump);
                DumpMemory(dump, ctx.Esp, 0x400);
            }
            ResumeThread(watched_thread);
            fputs("End of freeze dump\n\n", dump);
            fclose(dump);
        }

        void DumpRegisters(FILE *dump, CONTEXT *ctx) {
            fprintf(dump, "Eax: %08X\n", ctx->Eax);
            fprintf(dump, "Ecx: %08X\n", ctx->Ecx);
            fprintf(dump, "Edx: %08X\n", ctx->Edx);
            fprintf(dump, "Ebx: %08X\n", ctx->Ebx);
            fprintf(dump, "Esi: %08X\n", ctx->Esi);
            fprintf(dump, "Edi: %08X\n", ctx->Edi);
            fprintf(dump, "Ebp: %08X\n", ctx->Ebp);
            fprintf(dump, "Esp: %08X\n", ctx->Esp);
            fprintf(dump, "Eip: %08X\n", ctx->Eip);
        }

        void DumpMemory(FILE *dump, uintptr_t address, int bytes) {
            int pos = 0;
            while (pos < bytes) {
                fprintf(dump, "%08X ", address + pos);
                for (int i = 0; i < 4 && pos < bytes; i++) {
                    for (int j = 0; j < 4 && pos < bytes; j++) {
                        fprintf(dump, " %02X", *(uint8_t *)(address + pos));
                        pos += 1;
                    }
                    fputs(" ", dump);
                }
                fputs("\n", dump);
            }
        }

        static DWORD WINAPI ThreadProc(void *param) {
            FreezeLogger *self = (FreezeLogger *)param;
            while (true) {
                Sleep(3000);
                if (self->is_frozen()) {
                    self->DumpThreadState();
                }
            }
            return 0;
        }
};

#endif /* LOG_FREEZE_H */
