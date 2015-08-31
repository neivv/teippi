#ifndef OFFSETS_HOO
#define OFFSETS_HOO

#include "types.h"
#include <type_traits>

#define Offset offset<uint32_t>
template <typename type>
class offset
{
    public:
        inline offset() : val(0) {}
        inline constexpr offset(uintptr_t val_) : val(val_) {}
        inline offset(void *val_) : val((uintptr_t)val_) {}

        //inline operator uintptr_t() const { return val; }
        inline constexpr void* v() const { return (void *)val; }
        inline constexpr operator void*() const { return v(); }
        inline constexpr void *operator+ (uintptr_t num) const { return (void *)(val + num); }

        inline constexpr type &as() const { return *(type *)(val); }
        inline constexpr type &operator* () const { return as(); }
        inline constexpr type *operator-> () const { return (type *)(val); }
        inline constexpr type& operator[](int pos) const { return ((type *)(val))[pos]; }

        uintptr_t val;
};

namespace bw
{
    const Offset WinMain = 0x004E0AE0;
    const Offset WindowCreated = 0x004E0842;
    const Offset WndProc = 0x004D1D70;
    const offset<void *> main_window_hwnd = 0x0051BFB0;
    const offset<uint32_t> window_active = 0x0051BFA8;

    const Offset Sc_fwrite = 0x00411931;
    const Offset Sc_fread = 0x004117DE;
    const Offset Sc_fseek = 0x00411B6E;
    const Offset Sc_fclose = 0x0040D483;
    const Offset Sc_fopen = 0x0040D424;
    const Offset Sc_fgetc = 0x00411BB7;
    const Offset Sc_setvbuf = 0x00411619;

    const Offset Wm_KeyDownHook = 0x004D2197;
    const Offset Wm_CharHook = 0x004D1F5B;
    const Offset DrawScreen = 0x0041E280;
    const offset<Surface> game_screen = 0x006CEFF0;
    const offset<uint32_t> needs_full_redraw = 0x006D5E1C;
    const offset<DrawLayer> draw_layers = 0x006CEF50;
    const offset<void *> trans_list = 0x006D5E14;
    const offset<void *> game_screen_redraw_trans = 0x006D5E18;
    const offset<uint8_t> no_draw = 0x0051A0E9;
    const offset<uint8_t> screen_redraw_tiles = 0x006CEFF8;

    const Offset ProgressObjects = 0x004D94B0;
    const Offset GameFunc = 0x004D9670;
    const offset<uint32_t> unk_frame_state = 0x006D11F0;
    const offset<uint32_t> frames_progressed_at_once = 0x005124D4;
    const offset<uint8_t> draw_sprites = 0x006D11EC;
    const offset<uint32_t> is_paused = 0x006509C4;
    const offset<uint32_t> next_frame_tick = 0x0051CE94;
    const offset<uint32_t> unk_6D11E8 = 0x006D11E8;
    const offset<uint32_t> ai_interceptor_target_switch = 0x006957D0;
    const offset<uint32_t> lurker_hits_used = 0x0064DEA8;
    const offset<uint32_t> lurker_hits_pos = 0x0064EEC8;
    const offset<Unit *> lurker_hits = 0x0064DEC8;

    const offset<uint32_t> game_speed = 0x006CDFD4;
    const offset<uint32_t> game_speed_waits = 0x005124D8;

    const offset<Ai::Region *> player_ai_regions = 0x0069A604;
    const offset<Ai::PlayerData> player_ai = 0x0068FEE8;

    const Offset KillSingleUnit = 0x00475710;
    const Offset Unit_Die = 0x004A0740;
    const Offset Order_Die = 0x00479480;

    const offset<uint8_t *> aiscript_bin = 0x0068C104;
    const offset<uint8_t *> bwscript_bin = 0x0068C108;
    const Offset AiScript_InvalidOpcode = 0x0045C9B3;
    const Offset AiScript_Stop = 0x00403380;
    const Offset AiScript_MoveDt = 0x004A2380;
    const Offset AddMilitaryAi = 0x0043DA20;

    const offset<Ai::DataList<Ai::Script>> first_active_ai_script = 0x0068C0FC;
    const offset<Ai::DataList<Ai::Town>> active_ai_towns = 0x006AA050;

    const Offset CreateAiScript = 0x0045AEF0;
    const Offset RemoveAiTownGasReferences = 0x00432760;
    const Offset DeleteGuardAi = 0x00403030;
    const Offset DeleteWorkerAi = 0x00404470;
    const Offset DeleteBuildingAi = 0x00404500;
    const Offset DeleteMilitaryAi = 0x004371D0;
    const Offset CreateAiTown = 0x00432020;
    const Offset AddUnitAi = 0x00433DD0;
    const offset<Ai::ResourceAreaArray> resource_areas = 0x00692688;

    const offset<uint32_t> elapsed_seconds = 0x0058D6F8;
    const offset<uint32_t> frame_count = 0x0057F23C;

    const offset<uint32_t> keep_alive_count = 0x006556E8;
    const offset<uint32_t> desync_happened = 0x006552A8;
    const offset<uint32_t> net_player_flags = 0x0057F0B8;
    const offset<NetPlayer> net_players = 0x0066FE20;
    const offset<uint32_t> self_net_player = 0x0051268C;
    const offset<uint8_t> sync_data = 0x0065EB30;
    const offset<uint8_t> sync_frame = 0x0065EB2E;
    const offset<uint32_t> sync_hash = 0x0063FD28;

    const offset<Ai::DataList<Ai::GuardAi>> first_guard_ai = 0x00685108;
    const Offset PreCreateGuardAi = 0x00462670;
    const Offset AddGuardAiToUnit = 0x00462960;
    const Offset RemoveUnitAi = 0x004A1E50;
    const Offset Ai_UpdateGuardNeeds = 0x004630C0;
    const Offset Ai_DeleteGuardNeeds = 0x004626E0;
    const Offset Ai_SuicideMission = 0x0043E050;
    const Offset Ai_SetFinishedUnitAi = 0x00435DB0;
    const Offset Ai_AddToRegionMilitary = 0x0043E2E0;
    const Offset DoesNeedGuardAi = 0x00462880;
    const Offset ForceGuardAiRefresh = 0x00462760;
    const Offset AiSpendReq_TrainUnit = 0x00435F10;

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

    const Offset CreateBunkerShootOverlay = 0x00477FD0;

    const Offset CancelZergBuilding = 0x0045DA40;

    const Offset StatusScreen_DrawKills = 0x00425DD0;
    const Offset StatusScreenButton = 0x00458220;

    const Offset DrawStatusScreen_LoadedUnits = 0x00424BA0;
    const Offset TransportStatus_DoesNeedRedraw = 0x00424F10;
    const Offset TransportStatus_UpdateDrawnValues = 0x00424FC0;

    const offset<uint8_t> redraw_transport_ui = 0x006CA9F0;
    const offset<int32_t> ui_transported_unit_hps = 0x006CA950;
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
    const Offset minimap_canvas = 0x0059C194;
    const offset<uint32_t> minimap_dot_count = 0x0059C2B8;
    const offset<uint32_t> minimap_dot_checksum = 0x0059C1A8;
    const Offset DrawAllMinimapUnits = 0x004A4AC0;
    const offset<uint8_t> minimap_resource_color = 0x006CEB39;
    const offset<uint8_t> minimap_color_mode = 0x06D5BBE;
    const offset<uint8_t> player_minimap_color = 0x00581DD6;
    const offset<uint8_t> enemy_minimap_color = 0x006CEB34;
    const offset<uint8_t> ally_minimap_color = 0x006CEB31;

    const offset<uint16_t> LeaderBoardUpdateCount_Patch = 0x00489D2D;
    const offset<uint8_t> trigger_pause = 0x006509B4;
    const offset<uint32_t> dont_progress_frame = 0x006509C4;
    const offset<uint8_t> player_unk_650974 = 0x00650974;
    const offset<uint8_t> leaderboard_needs_update = 0x00685180; // Not sure
    const offset<uint8_t> victory_status = 0x0058D700;
    const offset<uint32_t> trigger_current_player = 0x006509B0;
    const offset<uint32_t> trigger_cycle_count = 0x006509A0;
    const offset<TriggerList> triggers = 0x0051A280;
    const offset<uint32_t> countdown_timer = 0x006509C0;
    const offset<uint32_t> trigger_portrait_active = 0x0068AC4C;
    const offset<uint32_t> player_waits = 0x00650980;
    const offset<uint8_t> player_wait_active = 0x006509B8;
    const offset<Trigger *> current_trigger = 0x006509AC;
    const offset<uint32_t> options = 0x006CDFEC;

    const offset<int (__fastcall *)(TriggerAction *)> trigger_actions = 0x00512800;

    const Offset TriggerPortraitFinished = 0x0045E610;

    const offset<Location> locations = 0x0058DC60;

    const offset<uint32_t> trig_kill_unit_count = 0x005971E0;
    const offset<uint32_t> trig_remove_unit_active = 0x005971DC;
    const Offset Trig_KillUnitGeneric = 0x004C7E20;

    const Offset FindUnitInLocation_Check = 0x004C6F70;
    const Offset ChangeInvincibility = 0x004C6B00;
    const Offset TransportDeath = 0x0049FDD0;
    const Offset CanLoadUnit = 0x004E6E00;
    const Offset LoadUnit = 0x004E78E0;
    const Offset UnloadUnit = 0x004E7F70;
    const Offset SendUnloadCommand = 0x004BFDB0;
    const Offset HasLoadedUnits = 0x004E7110;
    const Offset GetFirstLoadedUnit = 0x004E6C90;
    const Offset IsCarryingFlag = 0x004F3A80;
    const Offset ForEachLoadedUnit = 0x004E6D00;
    const Offset AddLoadedUnitsToCompletedUnitLbScore = 0x0045F870;
    const Offset GetUsedSpace = 0x004E7170;

    const Offset CmdBtnEvtHandlerSwitch = 0x004598E5;
    const Offset CmdBtnActionHook = 0x00459912;

    const Offset Action_HangarTrain = 0x00423390;
    const offset<Unit *> client_selection_group = 0x00597208;
    const offset<Unit *> client_selection_group2 = 0x0059724C;
    const offset<Unit *> client_selection_group3 = 0x006284B8;
    const offset<uint32_t> client_selection_count = 0x0059723D;
    const offset<uint8_t> client_selection_changed = 0x0059723C;
    const offset<Unit *> selection_groups = 0x006284E8;
    const offset<uint8_t> selection_iterator = 0x006284B6;
    const offset<Unit *> selection_hotkeys = 0x0057FE60;
    const offset<uint16_t> recent_selection_times = 0x0063FE40;
    const offset<Unit *> selection_rank_order = 0x00597248;

    const offset<uint8_t> self_alliance_colors = 0x00581D6A;
    const offset<uint8_t> alliances = 0x0058D634;
    const offset<uint32_t> frame_counter = 0x0057EEBC;

    const Offset SendChangeSelectionCommand = 0x004C0860;
    const offset<void *> SelectHotkeySwitch = 0x00484B64;
    const Offset CenterOnSelectionGroup = 0x004967E0;
    const Offset SelectHotkeyGroup = 0x00496B40;
    const Offset TrySelectRecentHotkeyGroup = 0x00496D30;
    const Offset Command_SaveHotkeyGroup = 0x004965D0;
    const Offset Command_SelectHotkeyGroup = 0x00496940;

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
    const offset<uint8_t> team_game_main_player = 0x0057F1CC;

    const offset<uint32_t> player_turn_size = 0x00654A80;
    const offset<uint8_t *> player_turns = 0x006554B4;
    const offset<uint32_t> player_download = 0x0068F4FC;
    const offset<uint32_t> public_game = 0x005999CC;
    const offset<uint8_t> net_players_watched_briefing = 0x006556D8;
    const offset<uint32_t> player_objectives_string_id = 0x0058D6C4;
    const offset<uint8_t> briefing_state = 0x006554B0;
    const Offset BriefingOk = 0x0046D090;
    const Offset ProcessLobbyCommands = 0x00486530;

    const offset<uint8_t> force_button_refresh = 0x0068C1B0;
    const offset<uint8_t> force_portrait_refresh = 0x0068AC74;
    const offset<uint8_t> ui_refresh = 0x0068C1F8;
    const offset<Control *> unk_control = 0x0068C1E8;
    const offset<void *> unk_control_value = 0x0068C1EC;

    const offset<Dialog *> load_screen = 0x0057F0DC;

    const Offset GameScreenRClickEvent = 0x004564E0;
    const Offset GameScreenLClickEvent_Targeting = 0x004BD500;

    const Offset SendRightClickCommand = 0x004C0380;
    const Offset DoTargetedCommand = 0x0046F5B0;

    const offset<uint8_t> ground_order_id = 0x00641691;
    const offset<uint8_t> unit_order_id = 0x00641692;
    const offset<uint8_t> obscured_unit_order_id = 0x00641693;

    const Offset CreateOrder = 0x0048C510;
    const Offset DeleteOrder = 0x004742D0;
    const Offset DeleteSpecificOrder = 0x00474400;
    const Offset DoNextQueuedOrder = 0x00475000;

    const offset<x16u> map_width = 0x00628450;
    const offset<y16u> map_height = 0x006284B4;
    const offset<x16u> map_width_tiles = 0x0057F1D4;
    const offset<y16u> map_height_tiles = 0x0057F1D6;
    const offset<uint32_t *> map_tile_flags = 0x006D1260;

    const Offset GetHangarCapacity = 0x004653D0;

    const offset<uint8_t> upgrade_level_sc = 0x0058D2B0;
    const offset<uint8_t> upgrade_level_bw = 0x0058F2FE;
    const offset<uint8_t> tech_level_sc = 0x0058CF44;
    const offset<uint8_t> tech_level_bw = 0x0058F140;

    const offset<uint32_t> unit_strength = 0x006BB210;

    const offset<uint32_t> cheat_flags = 0x006D5A6C;

    const Offset GetEmptyImage = 0x004D4E30;
    const Offset DeleteImage = 0x004D4CE0;
    const Offset IscriptEndRetn = 0x004D7BD2;
    const Offset CreateSprite = 0x004990F0;
    const Offset ProgressSpriteFrame = 0x00497920;
    const Offset ProgressLoneSpriteDeleteHook = 0x0048805C;
    const Offset DeleteSprite = 0x00497B40;
    const Offset SpawnSprite = 0x004D7120;

    const Offset InitSprites_JmpSrc = 0x00499940;
    const Offset InitSprites_JmpDest = 0x00499A05;

    const Offset AddMultipleOverlaySprites = 0x00499660;

    const Offset GetEmptyUnitHook = 0x004A06F5;
    const Offset GetEmptyUnitNop = 0x004A071B;
    const Offset UnitLimitJump = 0x004A06CD;
    const Offset InitUnitSystem = 0x0049F380;
    const Offset InitSpriteSystem = 0x00499900;

    const offset<uint8_t> image_flags = 0x006CEFB5;

    const offset<ImgRenderFuncs> image_renderfuncs = 0x005125A0;
    const offset<ImgUpdateFunc> image_updatefuncs = 0x00512510;

    const offset<uint8_t *> iscript = 0x006D1200;
    const offset<Unit *> active_iscript_unit = 0x006D11FC;
    const offset<void *> active_iscript_flingy = 0x006D11F4;
    const offset<Bullet *> active_iscript_bullet = 0x006D11F8;

    const Offset SetIscriptAnimation = 0x004D8470;

    const Offset Order_AttackMove_ReactToAttack = 0x00478370;
    const Offset Order_AttackMove_TryPickTarget = 0x00477820;

    const offset<RevListHead<Unit, 0x0>> first_active_unit = 0x00628430;
    const offset<RevListHead<Unit, 0x0>> first_hidden_unit = 0x006283EC;
    const offset<RevListHead<Unit, 0x0>> first_revealer = 0x006283F4;
    const offset<RevListHead<Unit, 0x0>> first_dying_unit = 0x0062842C;
    const offset<RevListHead<Unit, 0x68>> first_player_unit = 0x006283F8;
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

    const Offset ProgressFlingyTurning = 0x00494FE0;
    const Offset SetMovementDirectionToTarget = 0x00496140;

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

    const Offset SinglePlayerSpeed = 0x004D9A26;
    const Offset GameEnd = 0x004EE8C0;

    const Offset ZeroOldPosSearch = 0x0049F46B;
    const Offset AddToPositionSearch = 0x0046A3A0;
    const Offset FindUnitPosition = 0x00469B00;
    const Offset unit_positions_x = 0x0066FF78;
    const Offset unit_positions_y = 0x006769B8;
    const Offset FindNearbyUnits = 0x00430190;
    const Offset ClearPosSearchResults_Patch = 0x0046AC1E;
    const Offset FindUnitsRect = 0x004308A0;
    const Offset DoUnitsCollide = 0x00469B60;
    const Offset CheckMovementCollision = 0x004304D0;
    const Offset FindUnitBordersRect = 0x0042FF80;
    const Offset ClearPositionSearch = 0x0042FEE0;
    const Offset ChangeUnitPosition = 0x0046A000;
    const Offset FindNearestUnit = 0x004E8320;
    const Offset GetNearbyBlockingUnits = 0x00422160;
    const Offset RemoveFromPosSearch = 0x0046A300;
    const Offset GetDodgingDirection = 0x004F2A70;
    const Offset DoesBlockArea = 0x0042E0E0;
    const Offset FindUnitsPoint = 0x004300E0;
    const Offset IsTileBlockedBy = 0x00473300;
    const Offset DoesBuildingBlock = 0x00473410;

    const Offset UnitToIndex = 0x0047B1D0;
    const Offset IndexToUnit = 0x0047B210;
    const Offset PathingUnitIndexPatch2 = 0x0046BFDC;

    const Offset PathingInited = 0x0048433E;

    const offset<Unit *> dodge_unit_from_path = 0x006BEE88;

    const offset<Pathing::PathingSystem *> pathing = 0x006D5BFC;

    const offset<int32_t> position_search_units_count = 0x006BEE64;
    const offset<uint32_t> position_search_results_count = 0x006BEE6C;
    const offset<uint32_t> position_search_results_offsets = 0x006BEE70;
    const offset<x32> unit_max_width = 0x006BEE68;
    const offset<y32> unit_max_height = 0x006BB930;

    const Offset MakeDrawnSpriteList_Call = 0x0041CA06;
    const Offset PrepareDrawSprites = 0x00498CB0;
    const Offset DrawSprites = 0x00498D40;
    const offset<uint8_t> units = 0x0059CCA8; // Repurposed
    const offset<x16u> screen_pos_x_tiles = 0x0057F1D0;
    const offset<y16u> screen_pos_y_tiles = 0x0057F1D2;
    const offset<x16u> screen_x = 0x0062848C;
    const offset<y16u> screen_y = 0x006284A8;
    const offset<RevListHead<Sprite, 0x0>> horizontal_sprite_lines = 0x00629688;
    const offset<ListHead<Sprite, 0x0>> horizontal_sprite_lines_rev = 0x00629288;
    const offset<Sprite *> first_active_lone_sprite = 0x00654874;
    const offset<Sprite *> first_active_fow_sprite = 0x00654868;

    const Offset CreateLoneSprite = 0x00488210;
    const Offset CreateFowSprite = 0x00488410;
    const Offset InitLoneSprites = 0x00488550;
    const Offset ProgressLoneSpriteFrames = 0x00488510;
    const Offset DisableVisionSync = 0x0047CDE5;
    const Offset FullRedraw = 0x004BD595;
    const Offset SetSpriteDirection = 0x00401140;
    const Offset FindBlockingFowResource = 0x00487B00;

    const offset<uint8_t> fog_variance_amount = 0x00657A9C;
    const offset<uint8_t *> fog_arr1 = 0x006D5C14;
    const offset<uint8_t *> fog_arr2 = 0x006D5C0C;
    const Offset GenerateFog = 0x0047FC50;

    const offset<Sprite *> cursor_marker = 0x00652918;
    const offset<uint8_t> draw_cursor_marker = 0x00652920;
    const Offset DrawCursorMarker = 0x00488180;
    const Offset ShowRallyTarget = 0x00468670;
    const Offset ShowCursorMarker = 0x00488660;

    const Offset CreateBullet = 0x0048C260;
    const offset<ListHead<Bullet, 0x0>> last_active_bullet = 0x0064DEAC;
    const offset<RevListHead<Bullet, 0x0>> first_active_bullet = 0x0064DEC4;

    const offset<uint32_t> damage_multiplier = 0x00515B88;

    const Offset DamageUnit = 0x004797B0;

    const Offset RemoveUnitFromBulletTargets = 0x0048AAC0;

    const offset<uint32_t> all_units_count = 0x00582324;
    const offset<uint32_t> player_men_deaths = 0x00581E74;
    const offset<uint32_t> player_men_kills = 0x00581EA4;
    const offset<uint32_t> player_men_kill_score = 0x00581F04;
    const offset<uint32_t> player_building_deaths = 0x00581FC4;
    const offset<uint32_t> player_building_kills = 0x00581FF4;
    const offset<uint32_t> player_building_kill_score = 0x00582054;
    const offset<uint32_t> player_factory_deaths = 0x005820E4;
    const offset<uint32_t> player_factory_kills = 0x00582114;
    const offset<uint32_t> unit_deaths = 0x0058A364;
    const offset<uint32_t> unit_kills = 0x005878A4;
    const offset<uint32_t> building_count = 0x00581E44;
    const offset<uint32_t> men_score = 0x00581ED4;
    const offset<uint32_t> completed_units_count = 0x00584DE4;

    const offset<uint32_t> zerg_supply_used = 0x00582174;
    const offset<uint32_t> terran_supply_used = 0x00582204;
    const offset<uint32_t> protoss_supply_used = 0x00582294;
    const offset<uint32_t> zerg_supply_available = 0x00582144;
    const offset<uint32_t> terran_supply_available = 0x005821D4;
    const offset<uint32_t> protoss_supply_available = 0x00582264;
    const offset<uint32_t> zerg_supply_max = 0x005821A4;
    const offset<uint32_t> terran_supply_max = 0x00582234;
    const offset<uint32_t> protoss_supply_max = 0x005822C4;
    const offset<uint32_t> minerals = 0x0057F0F0;
    const offset<uint32_t> gas = 0x0057F120;

    const offset<uint16_t> tileset = 0x0057F1DC;

    const Offset ProgressBulletState = 0x0048BCF0;

    const Offset Order_Recall_SpritePatch = 0x00494608;
    const Offset RecallUnit_SpritePatch = 0x00494397;
    const offset<uint8_t> current_energy_req = 0x006563AA;

    const offset<Tbl *> stat_txt_tbl = 0x006D1238;
    const offset<Tbl *> network_tbl = 0x006D1220;

    const offset<uint32_t> use_rng = 0x006D11C8;
    const offset<uint32_t> rng_calls = 0x0051C610;
    const offset<uint32_t> all_rng_calls = 0x0051CA18;
    const offset<uint32_t> rng_seed = 0x051CA14;
    const Offset RngSeedPatch = 0x004EF192;

    const offset<Player> players = 0x0057EEE0;

    const offset<uint32_t> vision_update_count = 0x0051CE98;
    const offset<uint8_t> vision_updated = 0x0051CE9C;
    const offset<uint32_t> visions = 0x0057F1EC;

    const Offset ProgressUnstackMovement = 0x004F2160;
    const Offset MovementState13 = 0x0046BCC0;
    const Offset MovementState17 = 0x0046BC30;
    const Offset MovementState20 = 0x0046B000;
    const Offset MovementState1c = 0x0046BF60;
    const Offset MovementState_Flyer = 0x0046B400;
    const offset<int32_t> circle = 0x00512D28;

    const Offset ChangeMovementTarget = 0x004EB820;
    const Offset ChangeMovementTargetToUnit = 0x004EB720;

    const offset<int (__fastcall *) (Unit *, int, int, Unit **target, int unit_id)> GetRightClickOrder = 0x005153FC;
    const Offset ReplayCommands_Nothing = 0x004CDCE0;
    const Offset SDrawLockSurface = 0x00411E4E;
    const Offset SDrawUnlockSurface = 0x00411E48;
    const Offset ProcessCommands = 0x004865D0;

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
    const Offset LoadGameObjects = 0x004CFEF0;

    const offset<uint8_t> unk_6CEF8C = 0x006CEF8C;
    const offset<uint8_t *> pylon_power_mask = 0x006D5BD8;
    const offset<void *> map_mpq_handle = 0x00596BC4;
    const offset<PlacementBox> placement_boxes = 0x00640960;
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
    const offset<uint32_t> unk_006CE2A0 = 0x006CE2A0; // Also unk struct
    const offset<GrpSprite *> image_grps = 0x0051CED0;
    const offset<BlendPalette> blend_palettes = 0x005128F8;

    const Offset AiScript_SwitchRescue = 0x004A2130;

    const Offset AllocatePath = 0x004E42F0;
    const Offset DeletePath = 0x004E42A0;
    const Offset DeletePath2 = 0x0042F740;
    const Offset CreateSimplePath = 0x0042F830;
    const Offset InitPathArray = 0x0042F4F0;

    const Offset LoadReplayMapDirEntry = 0x004A7C30;
    const Offset LoadReplayData = 0x004DF570;
    const Offset ExtractNextReplayFrame = 0x004CDFF0;

    const offset<uint32_t> scenario_chk_length = 0x006D0F20;
    const offset<void *> scenario_chk = 0x006D0F24;
    const offset<ReplayHeader> replay_header = 0x006D0F30;
    const offset<char *> campaign_map_names = 0x0059C080;

    const Offset UpdateBuildingPlacementState = 0x00473FB0;

    const offset<Unit *> ai_building_placement_hack = 0x006D5DCC;
    const offset<uint8_t> building_placement_state = 0x006408F8;

    const offset<Unit *> last_bullet_spawner = 0x0064DEB0;

    const Offset MedicRemove = 0x004477C0;
    const Offset RemoveWorkerOrBuildingAi = 0x00434C90;

    const offset<uint32_t> player_build_minecost = 0x006CA51C;
    const offset<uint32_t> player_build_gascost = 0x006CA4EC;
    const offset<uint8_t> player_race = 0x0057F1E2;

    const Offset LoadGrp = 0x0047ABE0;
    const Offset IsDrawnPixel = 0x004D4DB0;
    const Offset LoadBlendPalettes = 0x004BDE60;
    const Offset FindUnitAtPoint = 0x0046F3A0;
    const Offset DrawImage_Detected = 0x0040B596;
    const Offset DrawImage_Detected_Flipped = 0x0040BCA3;
    const Offset DrawUncloakedPart = 0x0040AFD5;
    const Offset DrawUncloakedPart_Flipped = 0x0040B824;
    const Offset DrawImage_Cloaked = 0x0040B155;
    const Offset DrawImage_Cloaked_Flipped = 0x0040B9A9;
    const Offset MaskWarpTexture = 0x0040AD04;
    const Offset MaskWarpTexture_Flipped = 0x0040AE63;
    const Offset DrawGrp = 0x0040ABBE;
    const Offset DrawGrp_Flipped = 0x0040BF60;

    const offset<uint8_t> cloak_distortion = 0x005993D8;
    const offset<uint8_t> cloak_remap_palette = 0x005993D8;
    const offset<uint8_t> default_grp_remap = 0x0050CDC1;
    const offset<uint8_t> shadow_remap = 0x005985A0;

    const offset<uint8_t> download_percentage = 0x0066FBF9;
    const offset<uint32_t> update_lobby_glue = 0x005999D4;
    const offset<uint8_t *> joined_game_packet = 0x006D5C78;
    const offset<uint8_t> save_races = 0x0057F1C0;
    const offset<uint8_t> save_player_to_original = 0x0066FF34;
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
    const offset<uint8_t> player_types_unused = 0x0066FF3C;
    const offset<Player> temp_players = 0x0059BDB0;

    const Offset MakeJoinedGameCommand = 0x00471FB0;
    const Offset Command_GameData = 0x00470840;

    const Offset InitGame = 0x004EEE00;
    const Offset InitStartingRacesAndTypes = 0x0049CC40;
    const Offset FindClosestIndex = 0x004BDB30;

    const offset<uint32_t> current_palette_rgba = 0x005994E0;
    const offset<uint16_t> next_scmain_state = 0x0051CE90;
    const offset<uint8_t> error_happened = 0x006D5A10;
    const offset<uint32_t> nooks_and_crannies_error = 0x006D5BF8;
    const offset<char> chat_messages = 0x00640B60;
    const offset<uint8_t> starting_player_types = 0x0057F1B4;

    const Offset NeutralizePlayer = 0x00489FC0;
    const Offset MakeDetected = 0x00497ED0;

    const Offset AddDamageOverlay = 0x004993C0;

    // Some kind of league thing, size 0xc00?
    const offset<char> validation_replay_path = 0x00628668;

    namespace dat
    {
        const offset<uint32_t> units_dat_flags = 0x00664080;
        const offset<uint8_t> units_dat_group_flags = 0x006637A0;
        const offset<uint8_t> units_dat_rclick_action = 0x00662098;
        const offset<Rect16> units_dat_dimensionbox = 0x006617C8;
        const offset<uint16_t> units_dat_placement_box = 0x00662860;
        const offset<uint8_t> units_dat_attack_unit_order = 0x00663320;
        const offset<uint8_t> units_dat_attack_move_order = 0x00663A50;
        const offset<uint8_t> units_dat_return_to_idle_order = 0x00664898;
        const offset<uint8_t> units_dat_ai_idle_order = 0x00662EA0;
        const offset<uint8_t> units_dat_human_idle_order = 0x00662268;
        const offset<uint8_t> units_dat_space_required = 0x00664410;
        const offset<uint8_t> units_dat_space_provided = 0x00660988;
        const offset<uint8_t> units_dat_air_weapon = 0x006616E0;
        const offset<uint8_t> units_dat_ground_weapon = 0x006636B8;
        const offset<int32_t> units_dat_hitpoints = 0x00662350;
        const offset<uint16_t> units_dat_shields = 0x00660E00;
        const offset<uint8_t> units_dat_has_shields = 0x006647B0;
        const offset<uint8_t> units_dat_armor_upgrade = 0x006635D0;
        const offset<uint8_t> units_dat_armor = 0x0065FEC8;
        const offset<uint8_t> units_dat_armor_type = 0x00662180;
        const offset<uint16_t> units_dat_kill_score = 0x00663EB8;
        const offset<uint16_t> units_dat_build_score = 0x00663408;
        const offset<uint16_t> units_dat_build_time = 0x00660428;
        const offset<uint8_t> units_dat_ai_flags = 0x00660178;
        const offset<uint8_t> units_dat_sight_range = 0x00663238;
        const offset<uint8_t> units_dat_target_acquisition_range = 0x00662DB8;
        const offset<uint16_t> units_dat_subunit = 0x006607C0;
        const offset<uint16_t> units_dat_mine_cost = 0x00663888;
        const offset<uint16_t> units_dat_gas_cost = 0x0065FD00;
        const offset<uint8_t> units_dat_elevation_level = 0x00663150;
        const offset<uint8_t> units_dat_flingy = 0x006644F8;
        const offset<uint8_t> units_dat_direction = 0x006605F0;

        const offset<uint16_t> weapons_dat_outer_splash = 0x00657780;
        const offset<uint16_t> weapons_dat_middle_splash = 0x006570C8;
        const offset<uint16_t> weapons_dat_inner_splash = 0x00656888;
        const offset<uint8_t> weapons_dat_effect = 0x006566F8;
        const offset<uint8_t> weapons_dat_upgrade = 0x006571D0;
        const offset<uint16_t> weapons_dat_damage = 0x00656EB0;
        const offset<uint16_t> weapons_dat_upgrade_bonus = 0x00657678;
        const offset<uint8_t> weapons_dat_damage_type = 0x00657258;
        const offset<uint8_t> weapons_dat_cooldown = 0x00656FB8;
        const offset<uint32_t> weapons_dat_min_range = 0x00656A18;
        const offset<uint32_t> weapons_dat_max_range = 0x00657470;
        const offset<uint8_t> weapons_dat_behaviour = 0x00656670;
        const offset<uint16_t> weapons_dat_error_msg = 0x00656568;
        const offset<uint8_t> weapons_dat_x_offset = 0x00657910;
        const offset<uint32_t> weapons_dat_flingy = 0x00656CA8;
        const offset<uint8_t> weapons_dat_death_time = 0x00657040;
        const offset<uint8_t> weapons_dat_launch_spin = 0x00657888;
        const offset<uint8_t> weapons_dat_attack_angle = 0x00656990;

        const offset<int32_t> flingy_dat_top_speed = 0x006C9EF8;
        const offset<uint32_t> flingy_dat_halt_distance = 0x006C9930;
        const offset<uint16_t> flingy_dat_acceleration = 0x006C9C78;
        const offset<uint16_t> flingy_dat_sprite = 0x006CA318;
        const offset<uint8_t> flingy_dat_movement_type = 0x006C9858;

        const offset<uint16_t> sprites_dat_image = 0x00666160;

        const offset<uint8_t> images_dat_drawfunc = 0x00669E28;
        const offset<int8_t *> images_dat_shield_overlay = 0x0052E5C8;
        const offset<void *> images_dat_attack_overlay = 0x0051F2A8;
        const offset<uint32_t> images_dat_damage_overlay = 0x0066A210;
        const offset<uint8_t> images_dat_draw_if_cloaked = 0x00667718;

        const offset<uint16_t> techdata_dat_energy_cost = 0x00656380;
        const offset<uint16_t> techdata_dat_label = 0x006562A0;

        const offset<uint16_t> upgrades_dat_label = 0x00655A40;

        const offset<uint8_t> orders_dat_targeting_weapon = 0x00665880;
        const offset<uint8_t> orders_dat_use_weapon_targeting = 0x00664B00;
        const offset<uint8_t> orders_dat_energy_tech = 0x00664E00;
        const offset<uint8_t> orders_dat_obscured = 0x00665400;
        const offset<uint8_t> orders_dat_interruptable = 0x00665040;
        const offset<uint16_t> orders_dat_highlight = 0x00664EC0;
        const offset<uint8_t> orders_dat_terrain_clip = 0x006654C0;
        const offset<uint8_t> orders_dat_fleeable = 0x00664C80;
        const offset<uint8_t> orders_dat_unknown7 = 0x00665100;
        const offset<uint8_t> orders_dat_subunit_inheritance = 0x00664A40;
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

#undef Offset

#endif // OFFSETS_HOO

