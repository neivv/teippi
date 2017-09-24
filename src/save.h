#ifndef SAVE_H
#define SAVE_H

#include "types.h"
#include <stdio.h>
#include <string>
#include <unordered_map>

void Command_Save(const uint8_t *data);
int LoadGameObjects();
void SaveGame(const char *filename, uint32_t time);

class datastream;

template<class Parent>
class SaveBase
{
    public:
        SaveBase() {}
        ~SaveBase();
        void Close();
        bool IsOk() const { return file != 0; }

        template <bool saving> void ConvertSpritePtr(Sprite **ptr, const LoneSpriteSystem *sprites) const;
        template <bool saving> void ConvertBulletPtr(Bullet **ptr, const BulletSystem *bullets) const;

    protected:
        template <bool saving> void ConvertBullet(Bullet *bullet);
        template <bool saving> void ConvertUnit(Unit *unit);

        template <bool saving, class Ai_Type> void ConvertAiParent(Ai_Type *ai);
        template <bool saving> void ConvertBuildingAi(Ai::BuildingAi *ai, int player);
        template <bool saving> void ConvertGuardAiPtr(Ai::GuardAi **ai, int player);
        template <bool saving> void ConvertAiRegion(Ai::Region *region);
        template <bool saving> void ConvertAiTown(Ai::Town *town);
        template <bool saving> void ConvertAiRegionPtr(Ai::Region **script, int player);
        template <bool saving> void ConvertAiTownPtr(Ai::Town **town, int player);
        template <bool saving> void ConvertPlayerAiData(Ai::PlayerData *player_ai, int player);
        template <bool saving> void ConvertAiScript(Ai::Script *script);
        template <bool saving> void ConvertAiRequestValue(int type, void **value, int player);

        template <bool saving> void ConvertPathing(Pathing::PathingSystem *pathing, Pathing::PathingSystem *offset = 0);

        FILE *file;

        // Well, lone sprite as owned sprites are only pointed
        // from the parent
        std::unordered_map<Sprite *, uintptr_t> sprite_to_id;
        std::unordered_map<Bullet *, uintptr_t> bullet_to_id;
        std::unordered_map<uintptr_t, Bullet *> id_to_bullet;
        std::unordered_map<uintptr_t, Sprite *> id_to_sprite;
};

class Save : public SaveBase<Save>
{
    public:
        Save(const char *filename);
        ~Save() {}
        void SaveGame(uint32_t time);

        Sprite *FindSpriteById(uint32_t id) { return 0; }
        Bullet *FindBulletById(uint32_t id) { return 0; }

        void BeginCompression(int chunk_size = 0x100000);
        void EndCompression();
        void AddData(const void *data, int len);

    private:
        void WriteCompressedChunk();
        template <class C>
        void BeginBufWrite(C **out, C *in = 0);

        void RestorePointers();

        void CreateSpriteSave(Sprite *sprite_);
        void CreateBulletSave(Bullet *bullet_);
        void CreateUnitSave(Unit *unit_);

        template <class C, class L> void SaveObjectChunk(void (Save::*CreateSave)(C *object), const L &list_head);
        void SaveUnitPtr(Unit *ptr);
        void SaveBulletPtr(Bullet *ptr);

        void SaveAiChunk();
        void SavePlayerAiData(int player);
        void SaveAiTowns(int player);
        void CreateAiTownSave(Ai::Town *ai_);
        void CreateWorkerAiSave(Ai::WorkerAi *ai_);
        void CreateBuildingAiSave(Ai::BuildingAi *ai_, int player);
        void CreateMilitaryAiSave(Ai::MilitaryAi *ai_);
        void CreateAiRegionSave(Ai::Region *region_);
        void CreateAiScriptSave(Ai::Script *script_);
        void SaveAiRegions(int player);
        template <bool active_ais> void CreateGuardAiSave(Ai::GuardAi *ai_);
        template <bool active_ais> void SaveGuardAis(const ListHead<Ai::GuardAi, 0x0> &list_head);

        void SavePathingChunk();

        datastream *buf;
        std::string filename;
        bool compressing;
        int compressed_chunk_size;
};

class Load : public SaveBase<Load>
{
    public:
        Load(File *file);
        ~Load();
        void LoadGame();

        Sprite *FindSpriteById(uint32_t id);
        Bullet *FindBulletById(uint32_t id);

        void Read(void *buf, int size);
        void ReadCompressed(void *out, int size);

    private:
        int ReadCompressedChunk();
        void ReadCompressed(FILE *file, void *out, int size);
        void LoadUnitPtr(Unit **ptr);
        void LoadBulletPtr(Bullet **ptr);
        void LoadAiChunk();
        void LoadAiRegions(int player, int region_count);
        template <bool active_ais> void LoadGuardAis(ListHead<Ai::GuardAi, 0x0> &list_head);
        void LoadAiTowns(int player);
        void LoadPlayerAiData(int player);

        template <class C, bool uses_temp_ids, class L>
        void LoadObjectChunk(std::pair<int, C*> (*LoadSave)(uint8_t *, uint32_t, L *, uint32_t *), L *list_head, std::unordered_map<uint32_t, C *> *temp_id_map);

        int LoadAiRegion(Ai::Region *region, uint32_t size);
        int LoadAiTown(Ai::Town *out, uint32_t size);
        Ai::MilitaryAi *LoadMilitaryAi();
        Ai::Script *LoadAiScript();

        void LoadPathingChunk();

        template <class C>
        void BufRead(C *out);

        uint8_t *buf_beg;
        uint8_t *buf;
        uint8_t *buf_end;
        uint32_t buf_size;
        uint32_t version;
};

#endif // SAVE_H
