#ifndef OFFSETS_HOO
#define OFFSETS_HOO

#include "types.h"
#include <type_traits>

class offset_base
{
    public:
        inline constexpr offset_base(uintptr_t val_) : address(val_) {}

        /// Mostly for old code which was written to get raw pointers
        inline void* raw_pointer() const { return (void *)address; }

    protected:
        uintptr_t address;
};

template <typename Type>
class offset : public offset_base
{
    public:
        inline constexpr offset(uintptr_t addr) : offset_base(addr) {}

        inline constexpr Type &operator* () const { return *(Type *)(address); }
        inline constexpr Type *operator-> () const { return (Type *)(address); }
};

// Slightly verbose variadic template, so that offsets that are arrays containing arrays,
// can simply be declared as `array_offset<type, outer_size, inner_size>` instead of having to do
// `offset<bound_checked_array<bound_checked_array<type, inner_size>, outer_size>` or something.
template <typename, uintptr_t...> class array_offset;

template <class Type>
class array_offset_iterator {
    public:
        array_offset_iterator(uintptr_t a, uintptr_t e) : address(a), end(e) { }
        array_offset_iterator<Type> operator+(int amount) {
            return array_offset_iterator<Type>(address + sizeof(Type) * amount, end);
        }
        std::ptrdiff_t operator-(const array_offset_iterator<Type> &other) {
            return (address - other.address) / sizeof(Type);
        }
        array_offset_iterator<Type> &operator+=(int amount) {
            address += sizeof(Type) * amount;
            return *this;
        }
        array_offset_iterator<Type> &operator++() {
            return *this += 1;
        }
        Type &operator*() {
            Assert(address + sizeof(Type) <= end);
            return *(Type *)(address);
        }
        bool operator!=(const array_offset_iterator<Type> &other) const {
            return address != other.address;
        }

    private:
        uintptr_t address;
        uintptr_t end;
};

template <typename Type, uintptr_t Size>
class array_offset<Type, Size> : public offset_base
{
    public:
        inline constexpr array_offset(uintptr_t addr) : offset_base(addr) {}

        constexpr Type &operator[](uintptr_t pos) const
        {
            Assert(pos < Size);
            return index_overflowing(pos);
        }

        constexpr Type &index_overflowing(uintptr_t pos) const
        {
            return ((Type *)(address))[pos];
        }

        /// Returns size in entries, not in bytes
        static uintptr_t size()
        {
            return Size;
        }

        array_offset_iterator<Type> begin() const
        {
            return array_offset_iterator<Type>(address, address + size() * sizeof(Type));
        }

        array_offset_iterator<Type> end() const
        {
            uintptr_t end = address + size() * sizeof(Type);
            return array_offset_iterator<Type>(end, end);
        }
};

template <typename Type, uintptr_t Size, uintptr_t... More>
class array_offset<Type, Size, More...> : public offset_base
{
    public:
        inline constexpr array_offset(uintptr_t addr) : offset_base(addr) {}

        const array_offset<Type, More...> operator[](uintptr_t pos) const
        {
            Assert(pos < Size);
            return index_overflowing(pos);
        }

        const array_offset<Type, More...> index_overflowing(uintptr_t pos) const
        {
            uintptr_t offset = array_offset<Type, More...>::size() * pos;
            return array_offset<Type, More...>(address + offset * sizeof(Type));
        }

        /// Returns size in entries of Type in all child arrays
        static uintptr_t size()
        {
            return Size * array_offset<Type, More...>::size();
        }

        class iterator {
            public:
                iterator(uintptr_t a, uintptr_t e) : address(a), end(e) { }
                iterator operator+(int amount) {
                    return iterator(address + array_offset<Type, More...>::size() * sizeof(Type) * amount, end);
                }
                std::ptrdiff_t operator-(const iterator &other) {
                    uintptr_t entry_size = array_offset<Type, More...>::size() * sizeof(Type);
                    return (address - other.address) / entry_size;
                }
                iterator &operator+=(int amount) {
                    address += array_offset<Type, More...>::size() * sizeof(Type) * amount;
                    return *this;
                }
                iterator &operator++() {
                    return *this += 1;
                }
                const array_offset<Type, More...> operator*() {
                    uintptr_t deref_end = address + array_offset<Type, More...>::size() * sizeof(Type);
                    // Avoid unused var warning ._.
                    if (deref_end > end) { Assert(deref_end <= end); }
                    return array_offset<Type, More...>(address);
                }
                bool operator!=(const iterator &other) const {
                    return address != other.address;
                }

            private:
                uintptr_t address;
                uintptr_t end;
        };

        iterator begin() const
        {
            return iterator(address, address + size() * sizeof(Type));
        }

        iterator end() const
        {
            uintptr_t end = address + size() * sizeof(Type);
            return iterator(end, end);
        }
};

namespace std {
template <typename Type>
struct iterator_traits<array_offset_iterator<Type>>
{
    typedef std::ptrdiff_t difference_type;
    typedef Type &reference;
    typedef Type value_type;
    typedef std::random_access_iterator_tag iterator_category;
};
}

namespace bw
{
    const offset<void *> main_window_hwnd = 0x0051BFB0;
    const offset<uint32_t> window_active = 0x0051BFA8;

    const offset<Surface> game_screen = 0x006CEFF0;
    const offset<uint32_t> needs_full_redraw = 0x006D5E1C;
    const array_offset<DrawLayer, 8> draw_layers = 0x006CEF50;
    const offset<void *> trans_list = 0x006D5E14;
    const offset<void *> game_screen_redraw_trans = 0x006D5E18;
    const offset<uint8_t> no_draw = 0x0051A0E9;
    const array_offset<uint8_t, 0x28 * 0x1e> screen_redraw_tiles = 0x006CEFF8;

    const offset<uint32_t> unk_frame_state = 0x006D11F0;
    const offset<uint32_t> frames_progressed_at_once = 0x005124D4;
    const offset<uint8_t> draw_sprites = 0x006D11EC;
    const offset<uint32_t> is_paused = 0x006509C4;
    const offset<uint32_t> next_frame_tick = 0x0051CE94;
    const offset<uint32_t> unk_6D11E8 = 0x006D11E8;
    const offset<uint32_t> ai_interceptor_target_switch = 0x006957D0;
    const offset<uint32_t> lurker_hits_used = 0x0064DEA8;
    const offset<uint32_t> lurker_hits_pos = 0x0064EEC8;
    const array_offset<Unit *, 0x20, 0x10, 0x2> lurker_hits = 0x0064DEC8;

    const offset<uint32_t> game_speed = 0x006CDFD4;
    const array_offset<uint32_t, 7> game_speed_waits = 0x005124D8;

    const array_offset<Ai::Region *, 8> player_ai_regions = 0x0069A604;
    const array_offset<Ai::PlayerData, 8> player_ai = 0x0068FEE8;

    const offset<uint8_t *> aiscript_bin = 0x0068C104;
    const offset<uint8_t *> bwscript_bin = 0x0068C108;

    const offset<Ai::DataList<Ai::Script>> first_active_ai_script = 0x0068C0FC;
    const array_offset<Ai::DataList<Ai::Town>, 8> active_ai_towns = 0x006AA050;

    const offset<Ai::ResourceAreaArray> resource_areas = 0x00692688;

    const offset<uint32_t> elapsed_seconds = 0x0058D6F8;
    const offset<uint32_t> frame_count = 0x0057F23C;

    const offset<uint32_t> keep_alive_count = 0x006556E8;
    const offset<uint32_t> desync_happened = 0x006552A8;
    const array_offset<uint32_t, 8> net_player_flags = 0x0057F0B8;
    const array_offset<NetPlayer, 8> net_players = 0x0066FE20;
    const offset<uint32_t> self_net_player = 0x0051268C;
    const offset<uint8_t> sync_data = 0x0065EB30;
    const offset<uint8_t> sync_frame = 0x0065EB2E;
    const offset<uint32_t> sync_hash = 0x0063FD28;

    const array_offset<Ai::DataList<Ai::GuardAi>, 8> first_guard_ai = 0x00685108;

    const offset<uint8_t> is_ingame = 0x006556E0;
    const offset<uint8_t> is_bw = 0x0058F440;
    const offset<uint8_t> is_multiplayer = 0x0057F0B4;
    const offset<uint8_t> team_game = 0x00596875;
    const offset<uint32_t> last_error = 0x0066FF60;

    const offset<uint32_t> is_replay = 0x006D0F14;
    const offset<uint32_t> replay_show_whole_map = 0x006D0F1C;
    const offset<uint32_t> replay_paused = 0x006D11B0;

    const offset<uint32_t> player_exploration_visions = 0x0057EEB8;
    const offset<uint32_t> player_visions = 0x0057F0B0;
    const offset<uint32_t> replay_visions = 0x006D0F18;

    const offset<x32> mouse_clickpos_x = 0x006CDDC4;
    const offset<y32> mouse_clickpos_y = 0x006CDDC8;

    const offset<uint8_t> shift_down = 0x00596A28;
    const offset<uint8_t> ctrl_down = 0x00596A29;
    const offset<uint8_t> alt_down = 0x00596A2A;

    const offset<uint8_t> redraw_transport_ui = 0x006CA9F0;
    const array_offset<int32_t, 8> ui_transported_unit_hps = 0x006CA950;
    const offset<uint32_t> ui_hitpoints = 0x006CA94C;
    const offset<uint32_t> ui_shields = 0x006CA9EC;
    const offset<uint32_t> ui_energy = 0x006CAC0C;

    const offset<uint32_t> ignore_unit_flag80_clear_subcount = 0x006D5BD0;

    const offset<uint32_t> order_wait_reassign = 0x0059CCA4;
    const offset<uint32_t> secondary_order_wait_reassign = 0x006283E8;
    const offset<uint32_t> dying_unit_creep_disappearance_count = 0x00658AE4;

    const offset<x32> original_tile_width = 0x006D0F08;
    const offset<y32> original_tile_height = 0x006D0C6C;
    const offset<uint16_t *> original_tiles = 0x006D0C68;
    const offset<uint8_t *> creep_tile_borders = 0x006D0E80;
    const offset<uint16_t *> map_tile_ids = 0x005993C4;
    const offset<uint32_t *> megatiles = 0x00628494; // Scenario.chk MTXM
    const offset<void *> scenario_chk_STR = 0x005993D4;
    const offset<uint32_t> scenario_chk_STR_size = 0x005994D8;

    const offset<Surface *> current_canvas = 0x006CF4A8;
    const offset<uint32_t> minimap_dot_count = 0x0059C2B8;
    const offset<uint32_t> minimap_dot_checksum = 0x0059C1A8;
    const offset<uint8_t> minimap_resource_color = 0x006CEB39;
    const offset<uint8_t> minimap_color_mode = 0x06D5BBE;
    const array_offset<uint8_t, 12> player_minimap_color = 0x00581DD6;
    const offset<uint8_t> enemy_minimap_color = 0x006CEB34;
    const offset<uint8_t> ally_minimap_color = 0x006CEB31;

    const offset<uint16_t> LeaderBoardUpdateCount_Patch = 0x00489D2D;
    const offset<uint8_t> trigger_pause = 0x006509B4;
    const offset<uint32_t> dont_progress_frame = 0x006509C4;
    const array_offset<uint8_t, 8> player_victory_status = 0x00650974;
    const offset<uint8_t> leaderboard_needs_update = 0x00685180; // Not sure
    const array_offset<uint8_t, 8> victory_status = 0x0058D700;
    const offset<uint32_t> trigger_current_player = 0x006509B0;
    const offset<uint32_t> trigger_cycle_count = 0x006509A0;
    const array_offset<TriggerList, 8> triggers = 0x0051A280;
    const offset<uint32_t> countdown_timer = 0x006509C0;
    const offset<uint32_t> trigger_portrait_active = 0x0068AC4C;
    const array_offset<uint32_t, 8> player_waits = 0x00650980;
    const array_offset<uint8_t, 8> player_wait_active = 0x006509B8;
    const offset<Trigger *> current_trigger = 0x006509AC;
    const offset<uint32_t> options = 0x006CDFEC;

    const array_offset<int (__fastcall *)(TriggerAction *), 0x3c> trigger_actions = 0x00512800;

    const array_offset<Location, 0xff> locations = 0x0058DC60;

    const offset<uint32_t> trig_kill_unit_count = 0x005971E0;
    const offset<uint32_t> trig_remove_unit_active = 0x005971DC;

    const array_offset<Unit *, 12> client_selection_group = 0x00597208;
    const array_offset<Unit *, 12> client_selection_group2 = 0x0059724C;
    const array_offset<Unit *, 12> client_selection_group3 = 0x006284B8;
    const offset<uint32_t> client_selection_count = 0x0059723D;
    const offset<uint8_t> client_selection_changed = 0x0059723C;
    const array_offset<Unit *, 8, 12> selection_groups = 0x006284E8;
    const offset<uint8_t> selection_iterator = 0x006284B6;
    const array_offset<Unit *, 8, 18, 12> selection_hotkeys = 0x0057FE60;
    const array_offset<uint16_t, 8, 8> recent_selection_times = 0x0063FE40;
    const offset<Unit *> primary_selected = 0x00597248;

    const array_offset<uint8_t, 0xc> self_alliance_colors = 0x00581D6A;
    const array_offset<uint8_t, 0xc, 0xc> alliances = 0x0058D634;
    const offset<uint32_t> frame_counter = 0x0057EEBC;

    const offset<uint8_t> is_targeting = 0x00641694;
    const offset<uint8_t> is_queuing_command = 0x00596A28;

    const offset<uint32_t> is_placing_building = 0x00640880;
    const offset<uint16_t> placement_unit_id = 0x0064088A;
    const offset<uint8_t> placement_order = 0x0064088D;

    const offset<uint32_t> lobby_command_user = 0x00512680;
    const offset<uint8_t> command_user = 0x00512678;
    const offset<uint8_t> select_command_user = 0x0051267C;
    const offset<uint32_t> local_player_id = 0x00512684;
    const offset<uint32_t> local_unique_player_id = 0x00512688;
    const array_offset<uint8_t, 4> team_game_main_player = 0x0057F1CC;

    const array_offset<uint32_t, 8> player_turn_size = 0x00654A80;
    const array_offset<uint8_t *, 8> player_turns = 0x006554B4;
    const array_offset<uint32_t, 8> player_download = 0x0068F4FC;
    const offset<uint32_t> public_game = 0x005999CC;
    const array_offset<uint8_t, 8> net_players_watched_briefing = 0x006556D8;
    const array_offset<uint32_t, 8> player_objectives_string_id = 0x0058D6C4;
    const offset<uint8_t> briefing_state = 0x006554B0;

    const offset<uint8_t> force_button_refresh = 0x0068C1B0;
    const offset<uint8_t> force_portrait_refresh = 0x0068AC74;
    const offset<uint8_t> ui_refresh = 0x0068C1F8;
    const offset<Control *> unk_control = 0x0068C1E8;
    const offset<void *> unk_control_value = 0x0068C1EC;

    const offset<Dialog *> load_screen = 0x0057F0DC;

    const offset<uint8_t> ground_order_id = 0x00641691;
    const offset<uint8_t> unit_order_id = 0x00641692;
    const offset<uint8_t> obscured_unit_order_id = 0x00641693;

    const offset<x16u> map_width = 0x00628450;
    const offset<y16u> map_height = 0x006284B4;
    const offset<x16u> map_width_tiles = 0x0057F1D4;
    const offset<y16u> map_height_tiles = 0x0057F1D6;
    const offset<uint32_t *> map_tile_flags = 0x006D1260;

    const array_offset<uint8_t, 12, 0x2e> upgrade_level_sc = 0x0058D2B0;
    const array_offset<uint8_t, 12, 0xf> upgrade_level_bw = 0x0058F32C;
    const array_offset<uint8_t, 12, 0x18> tech_level_sc = 0x0058CF44;
    const array_offset<uint8_t, 12, 0x14> tech_level_bw = 0x0058F140;

    const array_offset<uint32_t, 2, 0xe4> unit_strength = 0x006BB210;

    const offset<uint32_t> cheat_flags = 0x006D5A6C;

    const offset<uint8_t> image_flags = 0x006CEFB5;

    const array_offset<ImgRenderFuncs, 0x11> image_renderfuncs = 0x005125A0;
    const array_offset<ImgUpdateFunc, 0x11> image_updatefuncs = 0x00512510;

    const offset<uint8_t *> iscript = 0x006D1200;
    const offset<Unit *> active_iscript_unit = 0x006D11FC;
    const offset<void *> active_iscript_flingy = 0x006D11F4;
    const offset<Bullet *> active_iscript_bullet = 0x006D11F8;

    const offset<RevListHead<Unit, 0x0>> first_active_unit = 0x00628430;
    const offset<RevListHead<Unit, 0x0>> first_hidden_unit = 0x006283EC;
    const offset<RevListHead<Unit, 0x0>> first_revealer = 0x006283F4;
    const offset<RevListHead<Unit, 0x0>> first_dying_unit = 0x0062842C;
    const array_offset<RevListHead<Unit, 0x68>, 12> first_player_unit = 0x006283F8;
    const offset<RevListHead<Unit, 0xf8>> first_pylon = 0x0063FF54;
    const offset<RevListHead<Unit, 0xf0>> first_invisible_unit = 0x0063FF5C;
    const offset<uint32_t> pylon_refresh = 0x0063FF44;

    const offset<RevListHead<Flingy, 0x0>> first_active_flingy = 0x0063FF34;
    const offset<RevListHead<Flingy, 0x0>> last_active_flingy = 0x0063FEC8;

    const offset<uint8_t> previous_flingy_flags = 0x0063FEC0;
    const offset<uint8_t> current_flingy_flags = 0x0063FEC2;
    const offset<x16u> new_flingy_x = 0x0063FED4;
    const offset<y16u> new_flingy_y = 0x0063FECC;
    const offset<x16u> old_flingy_x = 0x0063FEC4;
    const offset<y16u> old_flingy_y = 0x0063FED0;

    const offset<uint8_t> new_flingy_flags = 0x0063FEC0;
    const offset<x32> new_exact_x = 0x0063FED8;
    const offset<y32> new_exact_y = 0x0063FF40;
    const offset<uint8_t> show_startwalk_anim = 0x0063FF30;
    const offset<uint8_t> show_endwalk_anim = 0x0063FEC1;

    const offset<uint32_t> reveal_unit_area = 0x0066FF70;

    const offset<Unit *> last_active_unit = 0x0059CC9C;
    const offset<Unit *> last_hidden_unit = 0x00628428;
    const offset<Unit *> last_revealer = 0x00628434;
    const offset<Unit *> last_dying_unit = 0x0059CC98;

    const offset<Unit *> dodge_unit_from_path = 0x006BEE88;
    const offset<Pathing::PathingSystem *> pathing = 0x006D5BFC;

    const offset<int32_t> position_search_units_count = 0x006BEE64;
    const offset<uint32_t> position_search_results_count = 0x006BEE6C;
    const array_offset<uint32_t, 0x4> position_search_results_offsets = 0x006BEE70;
    const offset<x32> unit_max_width = 0x006BEE68;
    const offset<y32> unit_max_height = 0x006BB930;
    const offset<uint8_t> unit_positions_x = 0x0066FF78;

    const offset<uint8_t> units = 0x0059CCA8; // Repurposed
    const offset<x16u> screen_pos_x_tiles = 0x0057F1D0;
    const offset<y16u> screen_pos_y_tiles = 0x0057F1D2;
    const offset<x16u> screen_x = 0x0062848C;
    const offset<y16u> screen_y = 0x006284A8;
    const array_offset<RevListHead<Sprite, 0x0>, 0x100> horizontal_sprite_lines = 0x00629688;
    const array_offset<ListHead<Sprite, 0x0>, 0x100> horizontal_sprite_lines_rev = 0x00629288;
    const offset<Sprite *> first_active_lone_sprite = 0x00654874;
    const offset<Sprite *> first_active_fow_sprite = 0x00654868;
    const offset<Surface> minimap_surface = 0x0059C194;

    const offset<uint8_t> fog_variance_amount = 0x00657A9C;
    const offset<uint8_t *> fog_arr1 = 0x006D5C14;
    const offset<uint8_t *> fog_arr2 = 0x006D5C0C;

    const offset<Sprite *> cursor_marker = 0x00652918;
    const offset<uint8_t> draw_cursor_marker = 0x00652920;

    const offset<ListHead<Bullet, 0x0>> last_active_bullet = 0x0064DEAC;
    const offset<RevListHead<Bullet, 0x0>> first_active_bullet = 0x0064DEC4;

    const array_offset<uint32_t, 5, 5> damage_multiplier = 0x00515B88;

    const array_offset<uint32_t, 0xe4, 12> all_units_count = 0x00582324;
    const array_offset<uint32_t, 12> player_men_deaths = 0x00581E74;
    const array_offset<uint32_t, 12> player_men_kills = 0x00581EA4;
    const array_offset<uint32_t, 12> player_men_kill_score = 0x00581F04;
    const array_offset<uint32_t, 12> player_building_deaths = 0x00581FC4;
    const array_offset<uint32_t, 12> player_building_kills = 0x00581FF4;
    const array_offset<uint32_t, 12> player_building_kill_score = 0x00582054;
    const array_offset<uint32_t, 12> player_factory_deaths = 0x005820E4;
    const array_offset<uint32_t, 12> player_factory_kills = 0x00582114;
    const array_offset<uint32_t, 0xe4, 12> unit_deaths = 0x0058A364;
    const array_offset<uint32_t, 0xe4, 12> unit_kills = 0x005878A4;
    const array_offset<uint32_t, 12> building_count = 0x00581E44;
    const array_offset<uint32_t, 12> men_score = 0x00581ED4;
    const array_offset<uint32_t, 0xe4, 12> completed_units_count = 0x00584DE4;

    const array_offset<uint32_t, 0xc> zerg_supply_used = 0x00582174;
    const array_offset<uint32_t, 0xc> terran_supply_used = 0x00582204;
    const array_offset<uint32_t, 0xc> protoss_supply_used = 0x00582294;
    const array_offset<uint32_t, 0xc> zerg_supply_available = 0x00582144;
    const array_offset<uint32_t, 0xc> terran_supply_available = 0x005821D4;
    const array_offset<uint32_t, 0xc> protoss_supply_available = 0x00582264;
    const array_offset<uint32_t, 0xc> zerg_supply_max = 0x005821A4;
    const array_offset<uint32_t, 0xc> terran_supply_max = 0x00582234;
    const array_offset<uint32_t, 0xc> protoss_supply_max = 0x005822C4;
    const array_offset<uint32_t, 0xc> minerals = 0x0057F0F0;
    const array_offset<uint32_t, 0xc> gas = 0x0057F120;

    const offset<uint16_t> tileset = 0x0057F1DC;

    const offset<uint8_t> current_energy_req = 0x006563AA;

    const offset<Tbl *> stat_txt_tbl = 0x006D1238;
    const offset<Tbl *> network_tbl = 0x006D1220;

    const offset<uint32_t> use_rng = 0x006D11C8;
    const offset<uint32_t> rng_seed = 0x051CA14;

    const array_offset<Player, 12> players = 0x0057EEE0;

    const offset<uint32_t> vision_update_count = 0x0051CE98;
    const offset<uint8_t> vision_updated = 0x0051CE9C;
    const array_offset<uint32_t, 12> visions = 0x0057F1EC;

    const array_offset<int32_t, 0x100, 2> circle = 0x00512D28;

    const array_offset<int (__fastcall *) (Unit *, int, int, Unit **target, int unit_id), 7> GetRightClickOrder = 0x005153FC;
    const offset<int (__stdcall *) (int, Rect32 *, uint8_t **, int *, int)> SDrawLockSurface_Import = 0x004FE5A0;
    const offset<int (__stdcall *) (int, uint8_t *, int, int)> SDrawUnlockSurface_Import = 0x004FE59C;

    const offset<ReplayData *> replay_data = 0x00596BBC;

    const offset<uint32_t> unk_57F240 = 0x0057F240;
    const offset<uint32_t> unk_59CC7C = 0x0059CC7C;
    const offset<uint32_t> unk_6D5BCC = 0x006D5BCC;
    const offset<char> map_path = 0x0057FD3C;
    const offset<uint16_t> campaign_mission = 0x0057F244;
    const offset<GameData> game_data = 0x005967F8;
    const offset<File *> loaded_save = 0x006D1218;
    const offset<uint8_t> load_succeeded = 0x006D121C;
    const offset<uint32_t> unk_51CA1C = 0x0051CA1C;
    const offset<uint8_t> unk_57F1E3 = 0x0057F1E3;

    const array_offset<uint8_t, 3, 0x14> unk_6CEF8C = 0x006CEF8C;
    const offset<uint8_t *> pylon_power_mask = 0x006D5BD8;
    const offset<void *> map_mpq_handle = 0x00596BC4;
    const array_offset<PlacementBox, 2> placement_boxes = 0x00640960;
    const offset<uint32_t> has_effects_scode = 0x00596CCC;
    const offset<uint32_t> gameid = 0x006D5A64;
    const offset<void *> playback_commands = 0x006D0F24;
    const offset<uint32_t> menu_screen_id = 0x006D11BC;
    const offset<uint8_t> scmain_state = 0x00596904;

    const offset<void *> unk_6D120C = 0x006D120C;
    const offset<void *> unk_596898 = 0x00596898;
    const offset<void *> unk_5968E0 = 0x005968E0;
    const offset<void *> unk_5968FC = 0x005968FC;
    const offset<uint32_t> popup_dialog_active = 0x006D1214;
    const offset<Control *> popup_dialog = 0x006D5BF4;
    const offset<uint32_t> is_ingame2 = 0x006D5BC8;
    const offset<uint32_t> leave_game_tick = 0x0059CC88;
    const offset<uint32_t> snp_id = 0x0059688C;
    const offset<uint32_t> unk_sound = 0x0051A208; // Actually unk struct
    const array_offset<CycleStruct, 8> cycle_colors = 0x006CE2A0; // Also unk struct
    const array_offset<GrpSprite *, 0x3e7> image_grps = 0x0051CED0;
    const array_offset<BlendPalette, 0x8> blend_palettes = 0x005128F8;

    const offset<uint32_t> scenario_chk_length = 0x006D0F20;
    const offset<void *> scenario_chk = 0x006D0F24;
    const offset<ReplayHeader> replay_header = 0x006D0F30;
    const array_offset<char *, 65> campaign_map_names = 0x0059C080;

    const offset<Unit *> ai_building_placement_hack = 0x006D5DCC;
    const offset<uint8_t> building_placement_state = 0x006408F8;

    const offset<Unit *> last_bullet_spawner = 0x0064DEB0;

    const array_offset<uint32_t, 8> player_build_minecost = 0x006CA51C;
    const array_offset<uint32_t, 8> player_build_gascost = 0x006CA4EC;
    const offset<uint8_t> player_race = 0x0057F1E2;

    const array_offset<uint8_t, 0x100> cloak_distortion = 0x005993D8;
    const array_offset<uint8_t, 0x100> cloak_remap_palette = 0x005993D8;
    const array_offset<uint8_t, 0x100> default_grp_remap = 0x0050CDC1;
    const array_offset<uint8_t, 0x100> shadow_remap = 0x005985A0;

    const offset<uint8_t> download_percentage = 0x0066FBF9;
    const offset<uint32_t> update_lobby_glue = 0x005999D4;
    const offset<uint8_t *> joined_game_packet = 0x006D5C78;
    const array_offset<uint8_t, 12> save_races = 0x0057F1C0;
    const array_offset<uint8_t, 8> save_player_to_original = 0x0066FF34;
    const offset<uint32_t> saved_seed = 0x00596BB4;
    const offset<uint32_t> lobby_state = 0x0066FBFA;
    const offset<uint32_t> in_lobby = 0x00596888;
    const offset<MapDl *> map_download = 0x00596890;
    const offset<uint32_t> local_net_player = 0x0051268C;
    const offset<uint32_t> loaded_local_player_id = 0x0057F1B0;
    const offset<uint32_t> loaded_local_unique_player_id = 0x00596BAC;
    const offset<uint16_t> own_net_player_flags = 0x0066FF30;
    const offset<uint8_t> user_select_slots = 0x0059BDA8;
    const offset<uint8_t> replay_user_select_slots = 0x006D11A1;
    const offset<uint8_t> force_layout = 0x0058D5B0;
    const array_offset<uint8_t, 12> player_types_unused = 0x0066FF3C;
    const array_offset<Player, 12> temp_players = 0x0059BDB0;

    const array_offset<uint32_t, 100> current_palette_rgba = 0x005994E0;
    const offset<void *> FindClosestIndex = 0x004BDB30; // Actually a function
    const offset<uint16_t> next_scmain_state = 0x0051CE90;
    const offset<uint8_t> error_happened = 0x006D5A10;
    const offset<uint32_t> nooks_and_crannies_error = 0x006D5BF8;
    const array_offset<char, 13, 218> chat_messages = 0x00640B60;
    const array_offset<uint8_t, 12> starting_player_types = 0x0057F1B4;

    // Some kind of league thing? Most likely those 0xc00 bytes contain
    // other related data
    const array_offset<char, 0xc00> validation_replay_path = 0x00628668;

    namespace dat
    {
        template <typename T> using units_dat = array_offset<T, 0xe4>;
        template <typename T> using weapons_dat = array_offset<T, 0x82>;
        template <typename T> using flingy_dat = array_offset<T, 0xd1>;
        template <typename T> using sprites_dat = array_offset<T, 0x205>;
        template <typename T> using images_dat = array_offset<T, 0x3e7>;
        template <typename T> using upgrades_dat = array_offset<T, 0x3d>;
        template <typename T> using techdata_dat = array_offset<T, 0x2c>;
        template <typename T> using orders_dat = array_offset<T, 0xbd>;

        const units_dat<uint32_t> units_dat_flags = 0x00664080;
        const units_dat<uint8_t> units_dat_group_flags = 0x006637A0;
        const units_dat<uint8_t> units_dat_rclick_action = 0x00662098;
        const units_dat<Rect16> units_dat_dimensionbox = 0x006617C8;
        const array_offset<uint16_t, 0xe4, 2> units_dat_placement_box = 0x00662860;
        const units_dat<uint8_t> units_dat_attack_unit_order = 0x00663320;
        const units_dat<uint8_t> units_dat_attack_move_order = 0x00663A50;
        const units_dat<uint8_t> units_dat_return_to_idle_order = 0x00664898;
        const units_dat<uint8_t> units_dat_ai_idle_order = 0x00662EA0;
        const units_dat<uint8_t> units_dat_human_idle_order = 0x00662268;
        const units_dat<uint8_t> units_dat_space_required = 0x00664410;
        const units_dat<uint8_t> units_dat_space_provided = 0x00660988;
        const units_dat<uint8_t> units_dat_air_weapon = 0x006616E0;
        const units_dat<uint8_t> units_dat_ground_weapon = 0x006636B8;
        const units_dat<int32_t> units_dat_hitpoints = 0x00662350;
        const units_dat<uint16_t> units_dat_shields = 0x00660E00;
        const units_dat<uint8_t> units_dat_has_shields = 0x006647B0;
        const units_dat<uint8_t> units_dat_armor_upgrade = 0x006635D0;
        const units_dat<uint8_t> units_dat_armor = 0x0065FEC8;
        const units_dat<uint8_t> units_dat_armor_type = 0x00662180;
        const units_dat<uint16_t> units_dat_kill_score = 0x00663EB8;
        const units_dat<uint16_t> units_dat_build_score = 0x00663408;
        const units_dat<uint16_t> units_dat_build_time = 0x00660428;
        const units_dat<uint8_t> units_dat_ai_flags = 0x00660178;
        const units_dat<uint8_t> units_dat_sight_range = 0x00663238;
        const units_dat<uint8_t> units_dat_target_acquisition_range = 0x00662DB8;
        const units_dat<uint16_t> units_dat_subunit = 0x006607C0;
        const units_dat<uint16_t> units_dat_mine_cost = 0x00663888;
        const units_dat<uint16_t> units_dat_gas_cost = 0x0065FD00;
        const units_dat<uint8_t> units_dat_elevation_level = 0x00663150;
        const units_dat<uint8_t> units_dat_flingy = 0x006644F8;
        const units_dat<uint8_t> units_dat_direction = 0x006605F0;

        const weapons_dat<uint16_t> weapons_dat_outer_splash = 0x00657780;
        const weapons_dat<uint16_t> weapons_dat_middle_splash = 0x006570C8;
        const weapons_dat<uint16_t> weapons_dat_inner_splash = 0x00656888;
        const weapons_dat<uint8_t> weapons_dat_effect = 0x006566F8;
        const weapons_dat<uint8_t> weapons_dat_upgrade = 0x006571D0;
        const weapons_dat<uint16_t> weapons_dat_damage = 0x00656EB0;
        const weapons_dat<uint16_t> weapons_dat_upgrade_bonus = 0x00657678;
        const weapons_dat<uint8_t> weapons_dat_damage_type = 0x00657258;
        const weapons_dat<uint8_t> weapons_dat_cooldown = 0x00656FB8;
        const weapons_dat<uint32_t> weapons_dat_min_range = 0x00656A18;
        const weapons_dat<uint32_t> weapons_dat_max_range = 0x00657470;
        const weapons_dat<uint8_t> weapons_dat_behaviour = 0x00656670;
        const weapons_dat<uint16_t> weapons_dat_error_msg = 0x00656568;
        const weapons_dat<uint8_t> weapons_dat_x_offset = 0x00657910;
        const weapons_dat<uint32_t> weapons_dat_flingy = 0x00656CA8;
        const weapons_dat<uint8_t> weapons_dat_death_time = 0x00657040;
        const weapons_dat<uint8_t> weapons_dat_launch_spin = 0x00657888;
        const weapons_dat<uint8_t> weapons_dat_attack_angle = 0x00656990;

        const flingy_dat<int32_t> flingy_dat_top_speed = 0x006C9EF8;
        const flingy_dat<uint32_t> flingy_dat_halt_distance = 0x006C9930;
        const flingy_dat<uint16_t> flingy_dat_acceleration = 0x006C9C78;
        const flingy_dat<uint16_t> flingy_dat_sprite = 0x006CA318;
        const flingy_dat<uint8_t> flingy_dat_movement_type = 0x006C9858;

        const sprites_dat<uint16_t> sprites_dat_image = 0x00666160;
        const sprites_dat<uint8_t> sprites_dat_start_as_visible = 0x00665C48;

        const images_dat<uint8_t> images_dat_drawfunc = 0x00669E28;
        const images_dat<int8_t *> images_dat_shield_overlay = 0x0052E5C8;
        const images_dat<uint32_t> images_dat_damage_overlay = 0x0066A210;
        const images_dat<uint8_t> images_dat_draw_if_cloaked = 0x00667718;
        const images_dat<uint8_t> images_dat_turning_graphic = 0x0066E860;
        const images_dat<uint8_t> images_dat_clickable = 0x0066C150;
        const images_dat<uint8_t> images_dat_remapping = 0x00669A40;
        const images_dat<uint8_t> images_dat_use_full_iscript = 0x0066D4D8;
        const images_dat<uint32_t> images_dat_iscript_header = 0x0066EC48;
        const images_dat<uint32_t> images_dat_grp = 0x00668AA0;
        const array_offset<void *, 6, 0x3e7> images_dat_overlays = 0x0051F2A8;

        const techdata_dat<uint16_t> techdata_dat_energy_cost = 0x00656380;
        const techdata_dat<uint16_t> techdata_dat_label = 0x006562A0;

        const upgrades_dat<uint16_t> upgrades_dat_label = 0x00655A40;

        const orders_dat<uint8_t> orders_dat_targeting_weapon = 0x00665880;
        const orders_dat<uint8_t> orders_dat_use_weapon_targeting = 0x00664B00;
        const orders_dat<uint8_t> orders_dat_energy_tech = 0x00664E00;
        const orders_dat<uint8_t> orders_dat_obscured = 0x00665400;
        const orders_dat<uint8_t> orders_dat_interruptable = 0x00665040;
        const orders_dat<uint16_t> orders_dat_highlight = 0x00664EC0;
        const orders_dat<uint8_t> orders_dat_terrain_clip = 0x006654C0;
        const orders_dat<uint8_t> orders_dat_fleeable = 0x00664C80;
        const orders_dat<uint8_t> orders_dat_unknown7 = 0x00665100;
        const orders_dat<uint8_t> orders_dat_subunit_inheritance = 0x00664A40;
    }
    namespace storm
    {
        extern intptr_t base_diff;
    }
    namespace funcs
    {
        namespace storm
        {
            using bw::storm::base_diff;
        }
        #include "funcs.autogen"
    }
    namespace base
    {
        const uintptr_t starcraft = 0x00400000;
        const uintptr_t storm = 0x15000000;
        const uintptr_t battle = 0x19000000;
        const uintptr_t standard = 0x1D000000;
    }
}

using namespace bw::dat;
using namespace bw::funcs;

void SetStormOffsets(int diff);

#endif // OFFSETS_HOO

