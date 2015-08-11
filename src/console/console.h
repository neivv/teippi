#ifndef CONSOLE_H
#define CONSOLE_H

#include "types.h"
#include "surface.h"
#include "font.h"
#include "cmdargs.h"
#include <list>
#include <string>
#include <unordered_map>
#include <functional>

namespace Common
{

class Console;
class Line;
extern Console *console;
typedef std::function<bool(const CmdArgs &)> CmdFunc;
typedef std::list<Line> LogContainer;

class Line
{
    public:
        Line(const std::string &str_, int type_, unsigned char hlines);

        std::string str;
        int type;

        unsigned short horizontal_lines;
        unsigned short fast_horizontal_lines[3];
};

class LineIterator
{
    public:
        LineIterator();
        LineIterator(LogContainer::iterator base);

        void operator++();
        void operator--();
        Line &operator *();
        LineIterator &operator=(LogContainer::iterator it_);
        bool operator==(LogContainer::iterator other) { return it == other; }
        bool operator!=(LogContainer::iterator other) { return !(it == other); }
        bool operator==(const LineIterator &other) { return it == other.it && h_line == other.h_line; }
        bool operator!=(const LineIterator &other) { return !(*this == other); }

        int HLine() { return h_line; }

    private:
        LogContainer::iterator it;
        int h_line;
};

namespace Color
{
    namespace Default
    {
        static const int bg = 0xf;
        static const int border = 0x42;
        static const int text = 0x55;
        static const int own_cmd = 0x34;
        static const int typo = 0x21;
        static const int fail = 0x6f;
    }
    enum
    {
        own = 0,
        text,
        typo,
        fail,
        bg,
        border,

        last
    };
}

class Console
{
    public:
        Console();
        ~Console();

        void Show();
        virtual void Hide();
        bool IsOk() { return state != fail; }


        void HookWndProc(void *hwnd);
        void UnhookWndProc();
        void HookTranslateAccelerator(PatchContext *patch, uintptr_t expected_base);

        bool KeyHook(int key, int scan);
        bool CharHook(wchar_t chr);
        bool TranslateAcceleratorHook(void *msg);
        void Draw(uint8_t *fbuf, int w, int h);

        void Print(const std::string &line);
        void Printf(const char *format, ...);

        Common::Font *GetFont() { return &font; }

    protected:
        enum state_
        {
            fail,
            hidden,
            shown
        } state;

        std::unordered_map<std::string, CmdFunc> commands;
        uint8_t colors[Color::last];
        bool dirty;

        void ClearScreen();
        template <class C>
        void AddCommand(std::string cmd, bool(C::*fn)(const CmdArgs &)) {
            typedef bool(Console::*Func)(const CmdArgs &);
            using std::placeholders::_1;
            commands.emplace(move(cmd), std::bind((Func)fn, this, _1));
        }

        Common::Font font;

    private:
        void Render();
        void DrawBackground();

        std::string NextHorizontalLine(const std::string &line, int start_pos);
        std::string HorizontalLine(const Line &line, int pos, int *pos_hint);

        unsigned int HorizontalLineAmount(const std::string &line);
        int CharLength(char c);
        int CountLinesBetween(const LineIterator &a, const LineIterator &b);

        void ProcessCommand();
        int Command(const std::string &cmd);
        void PrintBase(const Line &line);

        Point32 pos;
        int history_end;
        uint16_t history_lines;
        uint16_t max_line_len;
        uint16_t history_size;

        bool ignore_next;
        bool clear;

        Surface surface;
        LogContainer lines;
        LineIterator line_pos;
        std::string current_cmd;
};

} // Namespace console


#endif // CONSOLE_H

