#include "console.h"

#include "../resolution.h"
#include "../patch/patchmanager.h"

#include <string.h>
#include <windows.h>
#undef DrawText // nyyh
#include <ctime>
#include <algorithm>

using std::string;
using std::move;

namespace Common {

Console *console = nullptr;

Line::Line(const std::string &str_, int type_, unsigned char hlines) : str(str_), type(type_)
{
    horizontal_lines = hlines;
}

LineIterator::LineIterator()
{
    h_line = 0;
}

LineIterator::LineIterator(LogContainer::iterator base)
{
    h_line = 0;
    it = base;
}

void LineIterator::operator++()
{
    if (++h_line >= it->horizontal_lines)
    {
        ++it;
        h_line = 0;
    }
}

void LineIterator::operator--()
{
    if (h_line == 0)
    {
        --it;
        h_line = it->horizontal_lines - 1;
    }
    else
        --h_line;
}

Line &LineIterator::operator *()
{
    return *it;
}

LineIterator &LineIterator::operator=(LogContainer::iterator it_)
{
    it = it_;
    return *this;
}

typedef LRESULT (CALLBACK WndProc)(HWND, UINT, WPARAM, LPARAM);
static WndProc *OldWndProc;
static HWND console_hwnd = NULL;
LRESULT CALLBACK ConsoleWndProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
    if (console)
    {
        switch (msg)
        {
            case WM_CHAR:
                if (console->CharHook(wparam))
                    return 0;
            break;
            case WM_KEYDOWN:
                if (console->KeyHook(wparam, (lparam >> 16) & 0xff))
                    return 0;
            break;
        }
    }
    return CallWindowProcA(OldWndProc, hwnd, msg, wparam, lparam);
}

void Console::HookWndProc(void *hwnd)
{
    console_hwnd = (HWND)hwnd;
    OldWndProc = (WndProc *)SetWindowLongPtr((HWND)hwnd, GWLP_WNDPROC, (LONG)&ConsoleWndProc);
}

void Console::UnhookWndProc()
{
    if (console_hwnd)
    {
        SetWindowLongPtr(console_hwnd, GWLP_WNDPROC, (LONG)OldWndProc);
        console_hwnd = NULL;
    }
}

static int __stdcall TaHook(void *hwnd, void *acctable, MSG *msg)
{
    if (console && console->TranslateAcceleratorHook(msg))
        return 0;
    return TranslateAccelerator((HWND)hwnd, (HACCEL)acctable, msg);
}

void Console::HookTranslateAccelerator(PatchContext *patch, uintptr_t expected_base)
{
    patch->ImportPatch(expected_base, "user32.dll", "TranslateAcceleratorA", (void *)&TaHook);
}

Console::Console()
{
    pos = { 20, 20 };
    int width = resolution::screen_width - 20 * 2;
    int height = resolution::screen_height / 3 - 20 * 2;
    history_end = height - 18;
    history_lines = (history_end - 3) / 11;
    max_line_len = width - 10;
    history_size = 256;

    colors[Color::own] = Color::Default::own_cmd;
    colors[Color::text] = Color::Default::text;
    colors[Color::typo] = Color::Default::typo;
    colors[Color::fail] = Color::Default::fail;
    colors[Color::bg] = Color::Default::bg;
    colors[Color::border] = Color::Default::border;

    surface.Init(width, height);

    line_pos = lines.begin();

    char actual_path[256];
    ExpandEnvironmentStrings("%systemroot%\\fonts\\lucon.ttf", actual_path, 250);
    font.Load(actual_path, 10, true);
    int prev_error = font.last_error;
    ExpandEnvironmentStrings("%systemroot%\\fonts\\arialuni.ttf", actual_path, 250);
    font.Load(actual_path, 10, true);

    char buf[256];
    sprintf(buf, "Init console: history lines = %d, max line len = %d", history_lines, max_line_len);
    Print(buf);

    ignore_next = false;
    clear = false;
    if (prev_error && font.last_error)
    {
        MessageBoxA(0, "Fonts could not be loaded", "Console error", 0);
        state = fail;
    }
    else
        state = hidden;
}

Console::~Console()
{
}

int Console::CharLength(char c)
{
    auto chara = font.GetChar(c);
    if (chara)
        return chara.take()->full_width;
    else
        return 0;
}

string Console::NextHorizontalLine(const string &line, int pos)
{
    int length = 0, orig_pos = pos;

    while (42)
    {
        int next_length = 0;
        auto next_space = line.find_first_of("\t\x20\v\f", pos);
        if (next_space == line.npos)
            std::for_each(line.begin() + pos, line.end(), [&](char c) { next_length += CharLength(c); });
        else
            std::for_each(line.begin() + pos, line.begin() + next_space + 1, [&](char c) { next_length += CharLength(c); });

        if (length + next_length > max_line_len)
        {
            if (length == 0)
            {
                int i = 0;
                for (char c = line[pos + i]; c != 0; i++)
                {
                    next_length = CharLength(c);
                    if (length + next_length >  max_line_len)
                        break;
                    length += next_length;
                }
                return line.substr(pos, i);
            }
            else
                return line.substr(orig_pos, pos - orig_pos);
        }
        if (next_space == line.npos)
            return line.substr(orig_pos);
        length += next_length;
        pos = next_space + 1;
    }
}

string Console::HorizontalLine(const Line &line, int entry, int *pos_hint)
{
    if (*pos_hint == -1)
    {
        int pos = 0;
        string str;
        while (entry-- >= 0)
        {
            str = NextHorizontalLine(line.str, pos);
            pos += str.length();
        }
        *pos_hint = pos;
        return str;
    }
    else
    {
        string str = NextHorizontalLine(line.str, *pos_hint);
        *pos_hint += str.length();
        return str;
    }
}

unsigned int Console::HorizontalLineAmount(const string &line)
{
    unsigned int pos = 0, count = 0;
    do
    {
        pos += NextHorizontalLine(line, pos).length();
        count++;
    } while (pos < line.length());
    return count;
}

void Console::DrawBackground()
{
    uint8_t *buf = surface.buf;
    int width = surface.width;
    memset(buf, colors[Color::border], width);
    for (int y = 1; y < surface.height - 1; y++)
    {
        buf[y * width] = colors[Color::border];
        memset(buf + y * width + 1, colors[Color::bg], width - 2);
        buf[y * width + width - 1] = colors[Color::border];
    }
    memset(buf + (surface.height - 1) * width, colors[Color::border], width);
    memset(buf + (history_end) * width, colors[Color::border], width);
}

void Console::Render()
{
    static time_t prev_time;
    time_t new_time = std::time(0);
    if (prev_time != new_time)
    {
        prev_time = new_time;
    }
    else if (!dirty)
    {
        return;
    }

    DrawBackground();
    int i = 0, pos_hint = -1;
    Point draw_pos(5, 10);
    for (auto it = line_pos; it != lines.end(); ++it)
    {
        if (i >= history_lines)
            break; // ehk vois assert?

        Line &line = *it;
        if (it.HLine() == 0)
            pos_hint = 0;
        string h_line = HorizontalLine(line, it.HLine(), &pos_hint);
        surface.DrawText(&font, h_line, draw_pos, colors[line.type]);
        draw_pos.y += 11;
        i++;
    }
    draw_pos = Point(5, surface.height - 5);
    if (new_time & 1)
        surface.DrawText(&font, current_cmd , draw_pos, colors[Color::own]);
    else
        surface.DrawText(&font, current_cmd + '_', draw_pos, colors[Color::own]);

    dirty = false;
}

void Console::Draw(uint8_t *framebuf, int w, int h)
{
    if (state == hidden)
        return;
    if (w != resolution::screen_width || h != resolution::screen_height)
        return; // vois panic?

    Render();

    for (int y = pos.y, i = 0; y < pos.y + surface.height; y++, i++)
    {
        memcpy(framebuf + y * w + pos.x, surface.buf + i * surface.width, surface.width);
    }
}

void Console::Show()
{
    if (state == fail)
        return;
    state = shown;
}

void Console::Hide()
{
    state = hidden;
}

void Console::ClearScreen()
{
    clear = true;
}

int Console::Command(const string &cmd)
{
    CmdArgs args(cmd.c_str());
    auto func = commands.find(args[0]);
    if (func != commands.end())
    {
        if ((func->second)(args))
            return Color::own;
        else
            return Color::fail;
    }
    return Color::typo;
}

void Console::ProcessCommand()
{
    if (current_cmd.empty())
        return;
    PrintBase(Line(current_cmd, 0, HorizontalLineAmount(current_cmd)));
    Line &cmd_line = *lines.rbegin();
    int color = Command(current_cmd);
    cmd_line.type = color;
    current_cmd.clear();
    if (clear)
    {
        lines.clear();
        line_pos = lines.end();
        clear = false;
    }
}

int Console::CountLinesBetween(const LineIterator &a_, const LineIterator &b)
{
    auto a = a_;
    int i = 0;
    while (a != b)
    {
        ++i;
        ++a;
    }
    return i;
}

void Console::PrintBase(const Line &line)
{
    dirty = true;
    lines.push_back(line);

    if (line_pos == lines.end())
        --line_pos;
    else if (CountLinesBetween(line_pos, lines.end()) > history_lines)
    {
        int count = line.horizontal_lines;
        while (count--)
            ++line_pos;
    }
    if (lines.size() > history_size)
        lines.pop_front();
}

void Console::Print(const std::string &line)
{
    PrintBase(Line(line, Color::text, HorizontalLineAmount(line)));
}

void Console::Printf(const char *format, ...)
{
    char buf[512];
    va_list varg;
    va_start(varg, format);
    vsnprintf(buf, sizeof buf, format, varg);
    va_end(varg);
    Print(buf);
}

bool Console::CharHook(wchar_t chr)
{
    if (ignore_next)
    {
        ignore_next = false;
        return false;
    }

    if (state != shown)
        return false;

    dirty = true;
    switch (chr)
    {
        case 0x8: // Backspace
            if (!current_cmd.empty())
                current_cmd.pop_back();
            while (!current_cmd.empty() && (current_cmd.back() & 0xc0) == 0xc0)
                current_cmd.pop_back();
        break;
        case 0x1b: // Esc
            current_cmd.clear();
        break;
        case 0xd: // Enter
            ProcessCommand();
        break;
        default:
        {
            char buf[8];
            int count = WideCharToMultiByte(CP_UTF8, 0, &chr, 1, buf, sizeof buf, NULL, NULL);
            for (int i = 0; i < count; i++)
                current_cmd.push_back(buf[i]);
        }
        break;
    }

    return true;
}

bool Console::KeyHook(int key, int scan)
{
    switch (scan)
    {
        case 0x29:
            if (state == shown)
                Hide();
            else
            {
                Show();
                ignore_next = true;
            }
            return true;
    }
    return false;
}

bool Console::TranslateAcceleratorHook(void *msg_)
{
    //MSG *msg = (MSG *)msg_;
    if (state == shown)
    {
        return true;
    }
    return false;
}


} // namespace Console

