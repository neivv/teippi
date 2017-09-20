#ifndef SC_CONSOLE_H
#define SC_CONSOLE_H

#ifdef CONSOLE
#include "console/genericconsole.h"
#include "types.h"
#include "limits.h"

struct Grid
{
    Grid() {}
    Grid(int w, int h, uint8_t c) : width(w), height(h), color(c) {}
    int width;
    int height;
    uint8_t color;
};

class ScConsole : public Common::GenericConsole
{
    public:
        ScConsole();
        ~ScConsole();

        virtual void Hide();
        bool show_fps;
        bool show_frame;

        void DrawDebugInfo(uint8_t *framebuf, xuint w, yuint h);

    private:
        bool Heal(const CmdArgs &args);
        bool Kill(const CmdArgs &args);
        bool Give(const CmdArgs &args);
        bool Gsw(const CmdArgs &args);
        bool Tcr(const CmdArgs &args);
        bool SupplyMax(const CmdArgs &args);
        bool AiScript(const CmdArgs &args);
        bool AiRegion(const CmdArgs &args);
        bool Player(const CmdArgs &args);
        bool UnitCmd(const CmdArgs &args);
        bool Money(const CmdArgs &args);
        bool Supply(const CmdArgs &args);
        bool Self(const CmdArgs &args);
        bool Pause(const CmdArgs &args);
        bool Vis(const CmdArgs &args);
        bool Cmd_Grid(const CmdArgs &args);

        bool Frame(const CmdArgs &args);
        bool Show(const CmdArgs &args);
        bool Test(const CmdArgs &args);
        bool Spawn(const CmdArgs &args);
        bool AiscriptExec(const CmdArgs &args);

        bool Death(const CmdArgs &args, bool print, bool clear);

        vector<UnitType> ParseUnitId(const char *unit_str, int max_amt);

        void GetTownRequests(uint32_t *out, int len, uint32_t *in);
        void DrawAiRegions(int player, Common::Surface *text_surf, const Point32 &pos);

        void DrawLocations(uint8_t *framebuf, xuint w, yuint h);
        void DrawCrects(uint8_t *framebuf, xuint w, yuint h);
        void DrawAiInfo(uint8_t *textbuf, uint8_t *framebuf, xuint w, yuint h);
        void DrawOrders(uint8_t *framebuf, xuint w, yuint h);
        void DrawCoords(uint8_t *framebuf, xuint w, yuint h);
        void DrawDeaths(uint8_t *framebuf, xuint w, yuint h);
        void DrawRange(uint8_t *framebuf, xuint w, yuint h);
        void DrawGrids(uint8_t *framebuf, xuint w, yuint h);
        void DrawBullets(uint8_t *framebuf, xuint w, yuint h);
        void DrawResourceAreas(uint8_t *textbuf, uint8_t *framebuf, xuint w, yuint h);

        Unit *GetUnit();

        void ConstructInfoLines();

        bool draw_locations;
        bool draw_crects;
        bool draw_ai_towns;
        enum class OrderDrawMode {
            None,
            All,
            Selected,
        } draw_orders;
        bool draw_ai_data;
        bool draw_ai_full;
        bool draw_ai_named;
        int show_ai[Limits::Players]; // 0 no, 1 yes, 2 extra
        bool draw_coords;
        bool draw_range;
        bool draw_info;
        bool draw_bullets;
        bool draw_resource_areas;

        // player_mask, unit_id
        vector<tuple<uint16_t, UnitType>> death_counters;
        vector<std::string> info_lines;
        vector<Grid> grids;
};

void PatchConsole();

#endif

#endif // SC_CONSOLE_H

