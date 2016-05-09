#include "save.h"

#include "unit.h"
#include "sprite.h"
#include "image.h"
#include "bullet.h"
#include "datastream.h"
#include "limits.h"
#include "pathing.h"
#include "player.h"
#include "yms.h"
#include "strings.h"
#include "offsets.h"
#include "order.h"
#include "ai.h"
#include "game.h"
#include "log.h"
#include "unitsearch.h"
#include "warn.h"
#include "init.h"

#include "console/assert.h"

#include <windows.h>
#include <stdio.h>
#include <unordered_map>
#include <vector>
#include <exception>
#include <memory>

#ifndef SEEK_SET
#define SEEK_CUR 1
#define SEEK_END 2
#define SEEK_SET 0
#endif

const int buf_defaultmax = 0x110000;
const int buf_defaultlimit = 0x100000;

class SaveException : public std::exception
{
    public:
        SaveException(const SaveException *parent = 0, const char *msg = 0)
        {
            if (parent)
                CopyMsgs(parent);
            if (msg)
                whats.push_back(msg);
        }
        void CopyMsgs(const SaveException *other)
        {
            whats = other->whats;
        }
        virtual const char *what() const noexcept override
        {
            return "Misc save exception";
        }
        std::string cause() const noexcept
        {
            if (whats.empty())
                return "?";

            std::string out;
            for (auto it = whats.begin(); it != whats.end() - 1; ++it)
            {
                out.append(*it);
                out.append(", ");
            }
            out.append(*whats.rbegin());
            return out;
        }
        void AddWhat(const std::string &what)
        {
            whats.push_back(what);
        }

    private:
        std::vector<std::string> whats;
};

#define NewSaveConvertFail(type, in, parent) SaveConvertFail<type>(#type, in, parent)
template<typename type>
class SaveConvertFail : public SaveException
{
    public:
        SaveConvertFail(const char *name, void *ptr, const SaveException *parent = 0) : SaveException(parent)
        {
            char buf[64];
            snprintf(buf, 64, "Save convert fail with ptr %p, type %s", ptr, name);
            AddWhat(buf);
            data = new uint8_t[sizeof(type)];
            memcpy(data, ptr, sizeof(type));
        }
        ~SaveConvertFail()
        {
            delete[] data;
        }

        virtual const char *what() const noexcept override
        {
            return "Save pointer conversion fail";
        }

        uint8_t *data;
};

#define SaveReadFail(type) SaveReadFail_(#type, 0)
class SaveReadFail_ : public SaveException
{
    public:
        SaveReadFail_(const char *name, const SaveException *parent = 0) : SaveException(parent)
        {
            char buf[64];
            snprintf(buf, 64, "Fail while reading %s", name);
            AddWhat(buf);
        }

        virtual const char *what() const noexcept override
        {
            return "Save reading fail";
        }

    private:
};

class ReadCompressedFail : public SaveException
{
    public:
        ReadCompressedFail(void *address, const SaveException *parent = 0) : SaveException(parent)
        {
            char buf[64];
            snprintf(buf, 64, "ReadCompressed fail: %p", address);
            AddWhat(buf);
        }

        virtual const char *what() const noexcept override
        {
            return "ReadCompressed fail";
        }
};

template <class List>
void ValidateList(List &head)
{
    auto i = head.begin();
    if (*i == nullptr)
        return;
    if (i.prev() != nullptr)
        throw SaveException(nullptr, "ValidateList");
    auto prev = *i;
    i = nullptr;
    while (i != nullptr)
    {
        if (i.prev() != prev)
        {
            throw SaveException(nullptr, "ValidateList");
        }
        prev = *i;
        ++i;
    }
}

template <class P>
SaveBase<P>::~SaveBase()
{
    Close();
}

Save::Save(const char *fn) : filename(fn)
{
    file = fopen(fn, "wb+");
    buf = new datastream(true, buf_defaultmax);
    compressing = false;
}

template <class P>
void SaveBase<P>::Close()
{
    if (file)
    {
        fclose(file);
        file = nullptr;
    }
}

Load::Load(File *file)
{
    this->file = (FILE *)file;
    buf_size = buf_defaultmax;
    buf_beg = (uint8_t *)malloc(buf_size);
    buf = buf_end = buf_beg;
}

Load::~Load()
{
    free(buf_beg);
}

template <class C>
void Load::BufRead(C *out)
{
    int amount = sizeof(C);
    if (buf + amount > buf_end)
        throw SaveException();
    memcpy(out, buf, amount);
    buf += amount;
}

template <class C>
void Save::BeginBufWrite(C **out, C *in)
{
    if (in)
        buf->Append(in, sizeof(C));
    else
        buf->Seek(sizeof(C));
    *out = (C *)buf->GetEnd() - 1;
}

void Save::WriteCompressedChunk()
{
    auto len = buf->Length();
    fwrite(&len, 1, 4, file);
    bw::WriteCompressed(buf->GetData(), buf->Length(), (File *)file);
    buf->Clear();
}

void Save::BeginCompression(int chunk_size)
{
    compressed_chunk_size = chunk_size;
    compressing = true;
}

void Save::EndCompression()
{
    if (buf->Length() > 0)
        WriteCompressedChunk();
    compressing = false;
}

void Save::AddData(const void *data, int len)
{
    if (compressing)
    {
        if (buf->Length() + len > compressed_chunk_size)
            WriteCompressedChunk();
        buf->Append(data, len);
    }
    else
        fwrite(data, 1, len, file);
}

int Load::ReadCompressedChunk()
{
    uint32_t size;
    if (fread(&size, 4, 1, file) != 1)
        throw SaveException(0, "ReadCompressedChunk: eof");
    if (size > buf_size)
    {
        buf_size = size;
        buf_beg = (uint8_t *)realloc(buf_beg, size);
    }
    buf = buf_beg;
    buf_end = buf + size;
    debug_log->Log("Reading chunk, size %x\n", size);
    bw::ReadCompressed(buf, size, (File *)file);
    return size;
}

void Load::Read(void *buf, int size)
{
    if (fread(buf, size, 1, file) != 1)
        throw SaveException(0, "Read: eof");
}

void Load::ReadCompressed(void *out, int size)
{
    if (buf == buf_end)
        ReadCompressedChunk();
    if (size > buf_end - buf)
        throw SaveException(0, "ReadCompressed: Too small chunk");
    memcpy(out, buf, size);
    buf += size;
}

void Load::ReadCompressed(FILE *file, void *out, int size)
{
    if (!bw::ReadCompressed(out, size, (File *)file))
        throw ReadCompressedFail(out);
}

template <bool saving>
void ConvertUnitPtr(Unit **ptr)
{
    if (*ptr)
    {
        if (saving)
            *ptr = (Unit *)((*ptr)->lookup_id);
        else
            *ptr = Unit::FindById((uint32_t)(*ptr));
        if (!*ptr)
            throw NewSaveConvertFail(Unit *, ptr, 0);
    }
}

template <class P> template <bool saving>
void SaveBase<P>::ConvertSpritePtr(Sprite **in, const LoneSpriteSystem *lone_sprites) const
{
    if (*in != nullptr)
    {
        try
        {
            if (saving)
                *in = (Sprite *)sprite_to_id.at(*in);
            else
                *in = id_to_sprite.at((uintptr_t)*in);
        }
        catch (const std::out_of_range &e)
        {
            throw NewSaveConvertFail(Sprite *, in, 0);
        }
    }
}

template <class Cb>
void LoneSpriteSystem::MakeSaveIdMapping(Cb callback) const
{
    uintptr_t pos = 1;
    for (auto &sprite : lone_sprites)
    {
        callback(sprite.get(), pos);
        pos += 1;
    }
}

template <class P> template <bool saving>
void SaveBase<P>::ConvertBulletPtr(Bullet **in, const BulletSystem *bullets) const
{
    if (*in != nullptr)
    {
        try
        {
            if (saving)
                *in = (Bullet *)bullet_to_id.at(*in);
            else
                *in = id_to_bullet.at((uintptr_t)*in);
        }
        catch (const std::out_of_range &e)
        {
            throw NewSaveConvertFail(Bullet *, in, 0);
        }
    }
}

template <class Cb>
void BulletSystem::MakeSaveIdMapping(Cb callback) const
{
    uintptr_t pos = 1;
    for (auto *container : Containers())
    {
        for (auto &bullet : *container)
        {
            callback(bullet.get(), pos);
            pos += 1;
        }
    }
}

template <bool saving>
void Image::SaveConvert()
{
    if (grp)
    {
        if (saving)
        {
            GrpSprite *orig_value = grp;
            if (drawfunc == HpBar)
            {
                Warning("Saving image (id %x) with hp bar drawfunc\n", image_id);
            }
            else
            {
                if (grp == bw::image_grps[image_id])
                    grp = (GrpSprite *)(image_id + 1);
                else
                {
                    for (unsigned int i = 0; i < Limits::ImageTypes; i++)
                    {
                        if (grp == bw::image_grps[i])
                        {
                            grp = (GrpSprite *)(i + 1);
                            break;
                        }
                    }
                }
                if (orig_value == grp)
                    throw NewSaveConvertFail(Image, this, 0);
            }
        }
        else
        {
            if ((unsigned int)grp > Limits::ImageTypes)
                throw SaveConvertFail<Image>("Image: grp", this, 0);
            grp = bw::image_grps[(int)grp - 1];
            if (IsFlipped())
                Render = bw::image_renderfuncs[drawfunc].flipped;
            else
                Render = bw::image_renderfuncs[drawfunc].nonflipped;
            Update = bw::image_updatefuncs[drawfunc].func;
        }
    }
    if (drawfunc == Image::HpBar)
    {
        ConvertUnitPtr<saving>((Unit **)&drawfunc_param);
    }
    else if (drawfunc == Image::Remap)
    {
        if (saving)
        {
            void *orig_value = drawfunc_param;
            for (int i = 0; i < 0x8; i++)
            {
                if (drawfunc_param == bw::blend_palettes[i].data)
                {
                    drawfunc_param = (void *)(i);
                    break;
                }
            }
            if (drawfunc_param == orig_value)
                throw NewSaveConvertFail(Image, this, 0);
        }
        else
        {
            if ((int)drawfunc_param > 0x7)
                throw SaveConvertFail<Image>("Image: drawfunc param", this, 0);
            drawfunc_param = bw::blend_palettes[(int)drawfunc_param].data;
        }
    }
}

template <bool saving>
void ConvertPath(Path *path)
{
    ConvertUnitPtr<saving>(&path->dodge_unit);
}

template<class P> template <bool saving>
void SaveBase<P>::ConvertPathing(Pathing::PathingSystem *pathing, Pathing::PathingSystem *offset)
{
    if (!offset)
        offset = pathing;
    for (int i = 0; i < pathing->region_count; i++)
    {
        if (saving)
            pathing->regions[i].neighbour_ids = (uint16_t *)((uintptr_t)(pathing->regions[i].neighbour_ids) - (uintptr_t)(offset));
        else
            pathing->regions[i].neighbour_ids = (uint16_t *)((uintptr_t)(pathing->regions[i].neighbour_ids) + (uintptr_t)(offset));
    }
    if (saving)
        pathing->unk_ids = (uint16_t *)((uintptr_t)(pathing->unk_ids) - (uintptr_t)(offset));
    else
        pathing->unk_ids = (uint16_t *)((uintptr_t)(pathing->unk_ids) + (uintptr_t)(offset));
    if (saving)
        pathing->unk = (Pathing::SplitRegion *)((uintptr_t)(pathing->unk) - (uintptr_t)(offset));
    else
        pathing->unk = (Pathing::SplitRegion *)((uintptr_t)(pathing->unk) + (uintptr_t)(offset));
}

template<class P> template <bool saving>
void SaveBase<P>::ConvertUnit(Unit *unit)
{
    try
    {
        ConvertUnitPtr<saving>(&unit->list.next);
        ConvertUnitPtr<saving>(&unit->list.prev);
        ConvertUnitPtr<saving>(&unit->move_target_unit);
        ConvertUnitPtr<saving>(&unit->target);
        ConvertBulletPtr<saving>(&unit->spawned_bullets.AsRawPointer(), bullet_system);
        ConvertBulletPtr<saving>(&unit->targeting_bullets.AsRawPointer(), bullet_system);
        ConvertUnitPtr<saving>(&unit->player_units.prev);
        ConvertUnitPtr<saving>(&unit->player_units.next);
        ConvertUnitPtr<saving>(&unit->subunit);
        ConvertUnitPtr<saving>(&unit->previous_attacker);
        ConvertUnitPtr<saving>(&unit->related);
        ConvertUnitPtr<saving>(&unit->currently_building);
        ConvertUnitPtr<saving>(&unit->invisible_list.prev);
        ConvertUnitPtr<saving>(&unit->invisible_list.next);
        ConvertUnitPtr<saving>(&unit->irradiated_by);
        ConvertUnitPtr<saving>(&unit->first_loaded);
        ConvertUnitPtr<saving>(&unit->next_loaded);

        if (unit->flags & UnitStatus::Building)
        {
            ConvertUnitPtr<saving>(&unit->building.addon);
            if (unit->Type() == UnitId::NuclearSilo)
                ConvertUnitPtr<saving>(&unit->silo.nuke);
        }
        if (unit->Type().IsWorker())
        {
            ConvertUnitPtr<saving>(&unit->worker.powerup);
            ConvertUnitPtr<saving>(&unit->worker.current_harvest_target);
            ConvertUnitPtr<saving>(&unit->harvester.previous_harvested);
            ConvertUnitPtr<saving>(&unit->harvester.harvesters.prev);
            ConvertUnitPtr<saving>(&unit->harvester.harvesters.next);
        }
        else if (unit->Type().Flags() & (UnitFlags::SingleEntity | UnitFlags::ResourceContainer))
        {
            ConvertUnitPtr<saving>(&unit->powerup.carrying_unit);
        }
        else if (unit->Type().HasHangar())
        {
            ConvertUnitPtr<saving>(&unit->carrier.in_child.AsRawPointer());
            ConvertUnitPtr<saving>(&unit->carrier.out_child.AsRawPointer());
        }
        else if (unit->Type() == UnitId::Scarab || unit->Type() == UnitId::Interceptor)
        {
            ConvertUnitPtr<saving>(&unit->interceptor.parent);
            ConvertUnitPtr<saving>(&unit->interceptor.list.next);
            ConvertUnitPtr<saving>(&unit->interceptor.list.prev);
        }
        else if (unit->Type() == UnitId::Ghost)
        {
            ConvertSpritePtr<saving>(&unit->ghost.nukedot, lone_sprites);
        }
        if (unit->Type() == UnitId::Pylon)
        {
            if (saving)
            {
                unit->pylon_list.list.prev = nullptr;
                unit->pylon_list.list.next = nullptr;
                unit->pylon.aura.release();
            }
        }
        else if (unit->Type().HasRally())
        {
            ConvertUnitPtr<saving>(&unit->rally.unit);
        }
    }
    catch (const SaveException &e)
    {
        throw NewSaveConvertFail(Unit, unit, &e);
    }
}

template<class P> template <bool saving>
void SaveBase<P>::ConvertAiRegionPtr(Ai::Region **region, int player)
{
    if (saving)
    {
        int id = (*region)->region_id;
        *region = (Ai::Region *)id;
    }
    else
        *region = bw::player_ai_regions[player] + (int)*region;
}

template<class P> template <bool saving>
void SaveBase<P>::ConvertAiRegion(Ai::Region *region)
{
    ConvertUnitPtr<saving>(&region->air_target);
    ConvertUnitPtr<saving>(&region->ground_target);
    ConvertUnitPtr<saving>(&region->slowest_military);
    ConvertUnitPtr<saving>(&region->first_important);
}

template<class P> template <bool saving>
void SaveBase<P>::ConvertGuardAiPtr(Ai::GuardAi **ai, int player)
{
    if (*ai)
    {
        int ai_index = 1;
        bool found = false;
        for (auto other_ai : Ai::needed_guards[player])
        {
            if (saving && other_ai == *ai)
            {
                *ai= (Ai::GuardAi *)ai_index;
                found = true;
                break;
            }
            else if (ai_index == (int)*ai)
            {
                *ai = other_ai;
                found = true;
                break;
            }
            ai_index++;
        }
        if (!found)
            throw NewSaveConvertFail(Ai::GuardAi *, ai, 0);
    }
}

template<class P> template <bool saving, class Ai_Type>
void SaveBase<P>::ConvertAiParent(Ai_Type *ai)
{
    ConvertUnitPtr<saving>(&ai->parent);
}

template<class P> template <bool saving>
void SaveBase<P>::ConvertAiRequestValue(int type, void **value, int player)
{
    switch(type)
    {
        case 1:
            ConvertAiRegionPtr<saving>((Ai::Region **)value, player);
        break;
        case 2:
            ConvertGuardAiPtr<saving>((Ai::GuardAi **)value, player);
        break;
        case 3:
        case 4:
        case 5:
        case 6:
        case 7:
            ConvertAiTownPtr<saving>((Ai::Town **)value, player);
        break;
        case 8:
        default:
        break;
    }
}

template<class P> template <bool saving>
void SaveBase<P>::ConvertBuildingAi(Ai::BuildingAi *ai, int player)
{
    ConvertUnitPtr<saving>(&ai->parent);
    for (int i = 0; i < 5; i++)
    {
        ConvertAiRequestValue<saving>(ai->train_queue_types[i], &ai->train_queue_values[i], player);
    }
}

template<class P> template <bool saving>
void SaveBase<P>::ConvertAiTownPtr(Ai::Town **town, int player)
{
    if (*town)
    {
        int town_index = 1;
        bool found = false;
        for (auto other_town : bw::active_ai_towns[player])
        {
            if (saving && other_town == *town)
            {
                *town = (Ai::Town *)town_index;
                found = true;
                break;
            }
            else if (town_index == (int)*town)
            {
                *town = other_town;
                found = true;
                break;
            }
            town_index++;
        }
        if (!found)
            throw NewSaveConvertFail(Ai::Town *, town, 0);
    }
}

template<class P> template <bool saving>
void SaveBase<P>::ConvertAiTown(Ai::Town *town)
{
    ConvertUnitPtr<saving>(&town->main_building);
    ConvertUnitPtr<saving>(&town->building_scv);
    ConvertUnitPtr<saving>(&town->mineral);
    ConvertUnitPtr<saving>(&town->gas_buildings[0]);
    ConvertUnitPtr<saving>(&town->gas_buildings[1]);
    ConvertUnitPtr<saving>(&town->gas_buildings[2]);
}

template<class P> template <bool saving>
void SaveBase<P>::ConvertPlayerAiData(Ai::PlayerData *player_ai, int player)
{
    for (int i = 0; i < player_ai->request_count; i++)
    {
        ConvertAiRequestValue<saving>(player_ai->requests[i].type, &player_ai->requests[i].val, player);
    }
    ConvertUnitPtr<saving>(&player_ai->free_medic);
}

template<class P> template <bool saving>
void SaveBase<P>::ConvertAiScript(Ai::Script *script)
{
    ConvertAiTownPtr<saving>(&script->town, script->player);
}

void Save::CreateSpriteSave(Sprite *sprite_)
{
    Sprite *sprite;
    BeginBufWrite(&sprite, sprite_);

    int count = 0, main_image_id = 0;
    for (Image *img : sprite->first_overlay)
    {
        count++;
        if (img == sprite->main_image)
            main_image_id = count;
    }
    sprite->main_image = (Image *)main_image_id;

    for (Image *img : sprite->first_overlay)
    {
        Image *conv;
        BeginBufWrite(&conv, img);
        conv->SaveConvert<true>();
    }
    sprite->first_overlay = (Image *)count;
}

void Save::CreateUnitSave(Unit *unit_)
{
    Unit *unit;
    BeginBufWrite(&unit, unit_);
    ConvertUnit<true>(unit);

    int order_queue_size = 0;
    for (Order *order : unit->order_queue_begin)
    {
        Order *order_;
        BeginBufWrite(&order_, order);
        ConvertUnitPtr<true>(&order_->target);
        order_queue_size++;
    }
    unit->order_queue_begin = (Order *)order_queue_size;
    if (unit->path)
    {
        Path *path;
        BeginBufWrite(&path, unit->path.get());
        ConvertPath<true>(path);
    }

    // There is single frame for units that die instantly/are removed when their sprite is null
    if (unit->sprite)
        CreateSpriteSave(unit->sprite.get());
}

template <class C, class L>
void Save::SaveObjectChunk(void (Save::*CreateSave)(C *object), const L &list_head)
{
    int i = 0, i_pos = ftell(file);
    fwrite(&i, 1, 4, file);
    for (C *object : list_head)
    {
        (this->*CreateSave)(object);
        if (buf->Length() > buf_defaultlimit)
            WriteCompressedChunk();

        i++;
    }
    if (buf->Length())
        WriteCompressedChunk();

    fseek(file, i_pos, SEEK_SET);
    fwrite(&i, 1, 4, file);
    fseek(file, 0, SEEK_END);
}

void Save::SaveUnitPtr(Unit *ptr)
{
    ConvertUnitPtr<true>(&ptr);
    fwrite(&ptr, 1, 4, file);
}

void Save::CreateMilitaryAiSave(Ai::MilitaryAi *ai_)
{
    Ai::MilitaryAi *ai;
    BeginBufWrite(&ai, ai_);
    ConvertAiParent<true>(ai);
}

void Save::CreateAiRegionSave(Ai::Region *region_)
{
    Ai::Region *region;
    BeginBufWrite(&region, region_);
    ConvertAiRegion<true>(region);

    int count = 0;
    buf->Seek(4);
    for (Ai::MilitaryAi *ai : region->military)
    {
        CreateMilitaryAiSave(ai);
        count += 1;
    }
    buf->Seek(0 - 4 - count * sizeof(Ai::MilitaryAi));
    buf->Append(&count, 4);
    buf->Seek(count * sizeof(Ai::MilitaryAi));
}

void Save::SaveAiRegions(int player)
{
    Ai::Region *regions = bw::player_ai_regions[player];
    for (int i = 0; i < (*bw::pathing)->region_count; i++)
    {
        CreateAiRegionSave(regions + i);
        if (buf->Length() > buf_defaultlimit)
            WriteCompressedChunk();
    }
    if (buf->Length())
        WriteCompressedChunk();
}

template <bool active_ais>
void Save::CreateGuardAiSave(Ai::GuardAi *ai_)
{
    Ai::GuardAi *ai;
    BeginBufWrite(&ai, ai_);
    if (active_ais)
        ConvertAiParent<true>(ai);
}

template <bool active_ais>
void Save::SaveGuardAis(const ListHead<Ai::GuardAi, 0x0> &list_head)
{
    int i = 0, i_pos = 0;
    i_pos = ftell(file);
    fwrite(&i, 1, 4, file);
    for (Ai::GuardAi *ai : list_head)
    {
        CreateGuardAiSave<active_ais>(ai);
        if (buf->Length() > buf_defaultlimit)
            WriteCompressedChunk();

        i++;
    }
    if (buf->Length())
        WriteCompressedChunk();

    fseek(file, i_pos, SEEK_SET);
    fwrite(&i, 1, 4, file);
    fseek(file, 0, SEEK_END);
}

void Save::CreateWorkerAiSave(Ai::WorkerAi *ai_)
{
    Ai::WorkerAi *ai;
    BeginBufWrite(&ai, ai_);
    ConvertAiParent<true>(ai);
}

void Save::CreateBuildingAiSave(Ai::BuildingAi *ai_, int player)
{
    Ai::BuildingAi *ai;
    BeginBufWrite(&ai, ai_);
    ConvertBuildingAi<true>(ai, player);
}

void Save::CreateAiTownSave(Ai::Town *town_)
{
    Ai::Town *town;
    BeginBufWrite(&town, town_);
    ConvertAiTown<true>(town);

    int i = 0;
    buf->Seek(4);
    for (Ai::WorkerAi *ai : town->first_worker)
    {
        CreateWorkerAiSave(ai);
        i += 1;
    }
    buf->Seek(0 - 4 - i * sizeof(Ai::WorkerAi));
    buf->Append(&i, 4);
    buf->Seek(i * sizeof(Ai::WorkerAi) + 4);
    i = 0;
    for (Ai::BuildingAi *ai : town->first_building)
    {
        CreateBuildingAiSave(ai, town->player);
        i += 1;
    }
    buf->Seek(0 - 4 - i * sizeof(Ai::BuildingAi));
    buf->Append(&i, 4);
    buf->Seek(i * sizeof(Ai::BuildingAi));
}

void Save::SaveAiTowns(int player)
{
    int i = 0, i_pos = ftell(file);
    fwrite(&i, 1, 4, file);
    for (Ai::Town *town : bw::active_ai_towns[player])
    {
        CreateAiTownSave(town);
        if (buf->Length() > buf_defaultlimit)
            WriteCompressedChunk();

        i++;
    }
    if (buf->Length())
        WriteCompressedChunk();

    fseek(file, i_pos, SEEK_SET);
    fwrite(&i, 1, 4, file);
    fseek(file, 0, SEEK_END);
}

void Save::SavePlayerAiData(int player)
{
    Ai::PlayerData *data;
    BeginBufWrite(&data, &(bw::player_ai[player]));
    ConvertPlayerAiData<true>(data, player);
    bw::WriteCompressed(buf->GetData(), buf->Length(), (File *)file);
    buf->Clear();
}

void Save::CreateAiScriptSave(Ai::Script *script_)
{
    Ai::Script *script;
    BeginBufWrite(&script, script_);
    ConvertAiScript<true>(script);
}

void Save::SaveAiChunk()
{
    fwrite(&((*bw::pathing)->region_count), 1, 4, file);
    for (unsigned i = 0; i < Limits::ActivePlayers; i++)
    {
        if (bw::players[i].type == 1)
        {
            SaveAiRegions(i);
            SaveGuardAis<true>(bw::first_guard_ai[i]);
            SaveGuardAis<false>(Ai::needed_guards[i]);
            SaveAiTowns(i);
            SavePlayerAiData(i);
        }
    }
    int i = 0, i_pos = ftell(file);
    fwrite(&i, 1, 4, file);
    for (Ai::Script *script : *bw::first_active_ai_script)
    {
        CreateAiScriptSave(script);
        if (buf->Length() > buf_defaultlimit)
            WriteCompressedChunk();

        i++;
    }
    if (buf->Length())
        WriteCompressedChunk();

    fseek(file, i_pos, SEEK_SET);
    fwrite(&i, 1, 4, file);
    fseek(file, 0, SEEK_END);

    bw::WriteCompressed(bw::resource_areas.raw_pointer(), 0x2ee8, (File *)file);
}

void Save::SavePathingChunk()
{
    using namespace Pathing;
    auto contours = (*bw::pathing)->contours;
    uint32_t chunk_size = contours->top_contour_count + contours->right_contour_count + contours->bottom_contour_count + contours->left_contour_count;
    chunk_size = chunk_size * sizeof(Contour) + sizeof(PathingSystem) + sizeof(ContourData);
    fwrite(&chunk_size, 4, 1, file);

    std::unique_ptr<uint8_t[]> chunk(new uint8_t[chunk_size]);
    uint8_t *pos = chunk.get();

    memcpy(pos, *bw::pathing, sizeof(PathingSystem));
    ConvertPathing<true>((PathingSystem *)pos, *bw::pathing);
    pos += sizeof(PathingSystem);
    memcpy(pos, contours, sizeof(ContourData));
    pos += sizeof(ContourData);
    memcpy(pos, contours->top_contours, contours->top_contour_count * sizeof(Contour));
    pos += contours->top_contour_count * sizeof(Contour);
    memcpy(pos, contours->right_contours, contours->right_contour_count * sizeof(Contour));
    pos += contours->right_contour_count * sizeof(Contour);
    memcpy(pos, contours->bottom_contours, contours->bottom_contour_count * sizeof(Contour));
    pos += contours->bottom_contour_count * sizeof(Contour);
    memcpy(pos, contours->left_contours, contours->left_contour_count * sizeof(Contour));
    pos += contours->left_contour_count * sizeof(Contour);

    bw::WriteCompressed(chunk.get(), chunk_size, (File *)file);
}

void Save::SaveGame(uint32_t time)
{
    datastream buf(true, buf_defaultmax);
    Sprite::RemoveAllSelectionOverlays();

    bw::WriteReadableSaveHeader((File *)file, filename.c_str());
    bw::WriteSaveHeader(time, (File *)file);

    *bw::saved_elapsed_seconds = (GetTickCount() - *bw::game_start_tick) / 1000 + *bw::elapsed_time_modifier;
    bw::WriteCompressed(bw::players.raw_pointer(), sizeof(Player) * Limits::Players, (File *)file);
    if (!*bw::campaign_mission)
        bw::ReplaceWithShortPath(&bw::map_path[0], MAX_PATH);
    bw::WriteCompressed(bw::minerals.raw_pointer(), 0x17700, (File *)file);
    fwrite(bw::local_player_id.raw_pointer(), 1, 4, file);
    if (!*bw::campaign_mission)
        bw::ReplaceWithFullPath(&bw::map_path[0], MAX_PATH);

    bullet_to_id.reserve(bullet_system->BulletCount());
    bullet_system->MakeSaveIdMapping([this] (Bullet *bullet, uintptr_t id) {
        bullet_to_id[bullet] = id;
    });
    sprite_to_id.reserve(lone_sprites->lone_sprites.size());
    lone_sprites->MakeSaveIdMapping([this] (Sprite *sprite, uintptr_t id) {
        sprite_to_id[sprite] = id;
    });

    lone_sprites->Serialize(this);
    //SaveObjectChunk(&Save::CreateFlingySave, first_allocated_flingy);
    bullet_system->Serialize(this);
    SaveObjectChunk(&Save::CreateUnitSave, first_allocated_unit);
    fwrite(&Unit::next_id, 4, 1, file);

    SaveUnitPtr(*bw::first_invisible_unit);
    SaveUnitPtr(*bw::first_active_unit);
    SaveUnitPtr(*bw::first_hidden_unit);
    SaveUnitPtr(*bw::first_dying_unit);
    SaveUnitPtr(*bw::first_revealer);
    SaveUnitPtr(*bw::last_active_unit);
    SaveUnitPtr(*bw::last_hidden_unit);
    SaveUnitPtr(*bw::last_dying_unit);
    SaveUnitPtr(*bw::last_revealer);
    for (unsigned i = 0; i < Limits::Players; i++)
        ValidateList(bw::first_player_unit[i]);
    for (unsigned i = 0; i < Limits::Players; i++)
        SaveUnitPtr(bw::first_player_unit[i]);

    uint32_t original_tile_length = *bw::original_tile_width * *bw::original_tile_height * 2;
    fwrite(&original_tile_length, 1, 4, file);
    bw::WriteCompressed(*bw::original_tiles, original_tile_length, (File *)file);
    bw::WriteCompressed(*bw::creep_tile_borders, original_tile_length / 2, (File *)file);
    bw::SaveDisappearingCreepChunk((File *)file);
    bw::WriteCompressed(*bw::map_tile_ids, Limits::MapHeight_Tiles * Limits::MapWidth_Tiles * 2, (File *)file);
    bw::WriteCompressed(*bw::megatiles, Limits::MapHeight_Tiles * Limits::MapWidth_Tiles * 2, (File *)file);
    bw::WriteCompressed(*bw::map_tile_flags, Limits::MapHeight_Tiles * Limits::MapWidth_Tiles * 4, (File *)file);

    bw::SaveTriggerChunk((File *)file);
    fwrite(bw::scenario_chk_STR_size.raw_pointer(), 1, 4, file);
    bw::WriteCompressed(*bw::scenario_chk_STR, *bw::scenario_chk_STR_size, (File *)file);

    Unit *tmp_selections[Limits::Selection * Limits::ActivePlayers];
    Unit **tmp_selections_pos = tmp_selections;
    for (auto selection : bw::selection_groups)
    {
        for (Unit *unit : selection)
        {
            *tmp_selections_pos = unit;
            ConvertUnitPtr<true>(tmp_selections_pos);
            tmp_selections_pos += 1;
        }
    }
    bw::WriteCompressed(tmp_selections, Limits::Selection * Limits::ActivePlayers * sizeof(Unit *), (File *)file);

    SavePathingChunk();

    SaveAiChunk();
    bw::SaveDatChunk((File *)file);
    fwrite(bw::screen_x.raw_pointer(), 1, 4, file);
    fwrite(bw::screen_y.raw_pointer(), 1, 4, file);

    bw::AddSelectionOverlays();
}

void Command_Save(const uint8_t *data)
{
    if (IsMultiplayer() && bw::FirstCommandUser() != *bw::lobby_command_user)
        return;
    if (bw::game_data->got.unk_tournament)
        return;

    const char *filename = (const char *)data + 5;
    uint32_t time = *(uint32_t *)(data + 1);

    int len = Sc_strlen(filename, 0x1c, 0x1c, false, false);
    if (len == 0) // color / control chars
        return;
    if (!bw::IsInvalidFilename(filename, 0, len - 1)) // wtf?
        return;
    SaveGame(filename, time);
}

void SaveGame(const char *filename, uint32_t time)
{
    char full_path[MAX_PATH];
    if (bw::GetUserFilePath(filename, full_path, MAX_PATH, 0) == 0)
        return;
    if (bw::IsInvalidFilename(full_path, 1, MAX_PATH))
        return;

    char dialog_string[MAX_PATH];
    snprintf(dialog_string, MAX_PATH, "%s \"%s\"", (*bw::network_tbl)->GetTblString(NetworkString::SaveDialogMsg), filename);

    auto orig_cmd_user = *bw::command_user, orig_select_cmd_user = *bw::select_command_user;
    *bw::command_user = *bw::local_player_id;
    *bw::select_command_user = *bw::local_unique_player_id;
    bw::ShowWaitDialog(dialog_string);
    *bw::command_user = orig_cmd_user;
    *bw::select_command_user = orig_select_cmd_user;

    Save save(full_path);
    if (save.IsOk())
    {
        try
        {
            save.SaveGame(time);
        }
        catch (const SaveConvertFail<Image> &e)
        {
            debug_log->Log("Save convert fail:");
            for (unsigned i = 0; i < sizeof(Image); i++)
                debug_log->Log(" %02x", e.data[i]);
            debug_log->Log("\n");
        }
        catch (const SaveException &e)
        {
            debug_log->Log("Save failed: %s\n", e.cause().c_str());
        }
        save.Close();
    }
    // Here would be some error handling but meh ^_^
    bw::HidePopupDialog();
}

// These don't leak memory, cause if they fail they should delete everything allocated
std::pair<int, Unit *> Unit::SaveAllocate(uint8_t *in, uint32_t size, DummyListHead<Unit, Unit::offset_of_allocated> *list_head, uint32_t *out_id)
{
    if (size < sizeof(Unit))
        return std::make_pair(0, (Unit *)0);
    Unit *in_unit = (Unit *)in;
    Unit *out = Unit::RawAlloc();
    memcpy(out, in_unit, sizeof(Unit));
    out->allocated.Add(*list_head);
    out->AddToLookup();
    size -= sizeof(Unit);
    in += sizeof(Unit);
    int order_count = (int)in_unit->order_queue_begin.AsRawPointer();
    out->order_queue_begin = 0;
    out->order_queue_end = 0;
    if (size < sizeof(Order) * order_count)
        return std::make_pair(0, (Unit *)0);

    for (int i = 0; i < order_count; i++)
    {
        Order *order = Order::RawAlloc();
        memcpy(order, in, sizeof(Order));
        order->allocated.Add(first_allocated_order);
        order->list.next = 0;
        order->list.prev = out->order_queue_end;
        if (!out->order_queue_begin)
            out->order_queue_begin = order;
        else
            out->order_queue_end->list.next = order;
        out->order_queue_end = order;
        in += sizeof(Order);
    }
    if (out->path)
    {
        out->path.release();
        out->path = make_unique<Path>();
        memcpy(out->path.get(), in, sizeof(Path));
        in += sizeof(Path);
    }

    int diff = 0;
    // Instantly dead units may get saved without sprite
    if (out->sprite)
    {
        Sprite *sprite;
        std::tie(diff, sprite) = Sprite::SaveAllocate(in, size);
        if (diff == 0)
            return std::make_pair(0, (Unit *)0);
        out->sprite.release();
        out->sprite.reset(sprite);
    }
    out->ai = nullptr;
    if (out->search_left != -1)
    {
        // Bw does that but it should only set a flag that would be already set
        // And accessing unit search now is not safe as search indices are wrong
        //bw::CheckUnstack(out);
        if (out->flags & UnitStatus::Building)
            bw::SetBuildingTileFlag(out, out->sprite->position.x, out->sprite->position.y);
        if (out->IsFlying())
            bw::IncrementAirUnitx14eValue(out);
        unit_search->Add(out);
    }
    if (out->path)
        return std::make_pair(sizeof(Unit) + sizeof(Order) * order_count + diff + sizeof(Path), out);
    else
        return std::make_pair(sizeof(Unit) + sizeof(Order) * order_count + diff, out);
}

void BulletSystem::Serialize(Save *save)
{
    uintptr_t container_sizes[0x7];
    auto containers = Containers();
    std::transform(containers.begin(), containers.end(), container_sizes, [](auto *c) { return c->size(); });
    save->AddData(container_sizes, sizeof container_sizes);
    save->BeginCompression();
    for (auto *c : Containers())
    {
        for (auto &bullet : *c)
        {
            bullet->Serialize(save, this);
        }
    }
    save->EndCompression();
    Bullet *first_active_bullet = *bw::first_active_bullet;
    Bullet *last_active_bullet = *bw::last_active_bullet;
    save->ConvertBulletPtr<true>(&first_active_bullet, this);
    save->ConvertBulletPtr<true>(&last_active_bullet, this);
    save->AddData(&first_active_bullet, sizeof(Bullet *));
    save->AddData(&last_active_bullet, sizeof(Bullet *));
}

template <bool saving, class T>
void Bullet::SaveConvert(SaveBase<T> *save, const BulletSystem *parent_sys)
{
    try
    {
        save->template ConvertBulletPtr<saving>(&list.next, parent_sys);
        save->template ConvertBulletPtr<saving>(&list.prev, parent_sys);
        ConvertUnitPtr<saving>(&move_target_unit);
        ConvertUnitPtr<saving>(&target);
        ConvertUnitPtr<saving>(&previous_target);
        ConvertUnitPtr<saving>(&parent);
        if (target)
        {
            save->template ConvertBulletPtr<saving>(&targeting.next, parent_sys);
            save->template ConvertBulletPtr<saving>(&targeting.prev, parent_sys);
        }
        if (parent)
        {
            save->template ConvertBulletPtr<saving>(&spawned.next, parent_sys);
            save->template ConvertBulletPtr<saving>(&spawned.prev, parent_sys);
        }
    }
    catch (const SaveException &e)
    {
        throw NewSaveConvertFail(Bullet, this, &e);
    }
}

void Bullet::Serialize(Save *save, const BulletSystem *parent)
{
    char buf[sizeof(Bullet)];
    Bullet *copy = (Bullet *)buf;
    memcpy(buf, this, sizeof(Bullet));
    copy->SaveConvert<true>(save, parent);
    save->AddData(buf, sizeof(Bullet));
    sprite->Serialize(save);
}

void BulletSystem::Deserialize(Load *load)
{
    try
    {
        uintptr_t container_sizes[0x7];
        load->Read(container_sizes, sizeof container_sizes);

        uintptr_t *current_remaining = container_sizes;
        for (auto *cont : Containers())
        {
            while (*current_remaining != 0)
            {
                auto bullet = ptr<Bullet>(new Bullet);
                load->ReadCompressed(bullet.get(), sizeof(Bullet));
                memset(&bullet->sprite, 0, sizeof(ptr<Sprite>));
                bullet->sprite = Sprite::Deserialize(load);
                cont->emplace(move(bullet));
                *current_remaining -= 1;
            }
            current_remaining++;
        }
        load->Read(&bw::first_active_bullet->AsRawPointer(), sizeof(uintptr_t));
        load->Read(&bw::last_active_bullet->AsRawPointer(), sizeof(uintptr_t));
    }
    catch (const SaveException &e)
    {
        throw SaveReadFail_("BulletSystem", &e);
    }
}

void BulletSystem::FinishLoad(Load *load)
{
    load->ConvertBulletPtr<false>(&bw::first_active_bullet->AsRawPointer(), this);
    load->ConvertBulletPtr<false>(&bw::last_active_bullet->AsRawPointer(), this);
    for (auto &bullet : ActiveBullets())
        bullet->SaveConvert<false>(load, this);
}

void LoneSpriteSystem::Serialize(Save *save)
{
    uintptr_t lone_count = lone_sprites.size();
    uintptr_t fow_count = fow_sprites.size();
    save->AddData(&lone_count, sizeof(uintptr_t));
    save->AddData(&fow_count, sizeof(uintptr_t));
    save->BeginCompression();
    for (ptr<Sprite> &sprite : lone_sprites)
        sprite->Serialize(save);
    for (ptr<Sprite> &sprite : fow_sprites)
        sprite->Serialize(save);
    save->EndCompression();
}

void LoneSpriteSystem::Deserialize(Load *load)
{
    uintptr_t lone_count, fow_count;
    load->Read(&lone_count, sizeof(uintptr_t));
    load->Read(&fow_count, sizeof(uintptr_t));
    // There's the cursor marker already, however it will not be pointed by the global cursor_marker...
    for (int i = 0; i < *bw::map_height_tiles; i++)
    {
        bw::horizontal_sprite_lines[i] = nullptr;
        bw::horizontal_sprite_lines_rev[i] = nullptr;
    }
    lone_sprites.clear();
    for (auto i = 0; i < lone_count; i++)
    {
        ptr<Sprite> sprite = Sprite::Deserialize(load);
        lone_sprites.emplace(move(sprite));
    }
    for (auto i = 0; i < fow_count; i++)
    {
        ptr<Sprite> sprite = Sprite::Deserialize(load);
        fow_sprites.emplace(move(sprite));
    }
}

void Sprite::Serialize(Save *save)
{
    char buf[sizeof(Sprite)];
    Sprite *copy = (Sprite *)buf;
    memcpy(buf, this, sizeof(Sprite));

    int count = 0, main_image_id = 0;
    for (Image *img : first_overlay)
    {
        count++;
        if (img == main_image)
            main_image_id = count;
    }
    copy->main_image = (Image *)main_image_id;
    copy->first_overlay = (Image *)count;
    debug_log->Log("Ser sprite %x\n", sprite_id);
    save->AddData(buf, sizeof(Sprite));

    for (Image *img : first_overlay)
    {
        debug_log->Log("Ser image %x\n", img->image_id);
        char img_buf[sizeof(Image)];
        memcpy(img_buf, img, sizeof(Image));
        Image *copy_img = (Image *)img_buf;
        copy_img->SaveConvert<true>();
        save->AddData(img_buf, sizeof(Image));
    }
}

ptr<Sprite> Sprite::Deserialize(Load *load)
{
    debug_log->Log("Des sprite\n");
    try
    {
        auto sprite = ptr<Sprite>(new Sprite);
        load->ReadCompressed(sprite.get(), sizeof(Sprite));
        uintptr_t count = (uintptr_t)sprite->first_overlay.AsRawPointer();
        uintptr_t main_image_id = (uintptr_t)sprite->main_image - 1;
        sprite->first_overlay = nullptr;
        sprite->last_overlay = nullptr;
        if (count == 0)
            throw SaveReadFail_("Sprite: image count");
        for (auto i = 0; i < count; i++)
        {
            Image *img = new Image;
            load->ReadCompressed(img, sizeof(Image));
            img->SaveConvert<false>();
            img->list.next = nullptr;
            img->list.prev = sprite->last_overlay;
            if (!sprite->first_overlay)
                sprite->first_overlay = img;
            else
                sprite->last_overlay->list.next = img;
            sprite->last_overlay = img;
            if (i == main_image_id)
                sprite->main_image = img;
            img->parent = sprite.get();
        }
        if ((uintptr_t)sprite->main_image == main_image_id)
            throw SaveReadFail_("Sprite/main image");

        sprite->AddToHlines();
        return sprite;
    }
    catch (const SaveException &e)
    {
        throw SaveReadFail_("Sprite::Deserialize", &e);
    }
}

std::pair<int, Sprite *> Sprite::SaveAllocate(uint8_t *in, uint32_t size)
{
    if (size < sizeof(Sprite))
        throw SaveReadFail_("Sprite");
    Sprite *in_sprite = (Sprite *)in;
    int count = (int)in_sprite->first_overlay.AsRawPointer(), main_image_id = (int)in_sprite->main_image - 1;
    size -= sizeof(Sprite);
    if (size < sizeof(Image) * count)
        throw SaveReadFail_("Sprite/image");

    Sprite *out = new Sprite;
    memcpy(out, in, sizeof(Sprite));
    in += sizeof(Sprite);
    out->first_overlay = 0;
    out->last_overlay = 0;

    for (int i = 0; i < count; i++)
    {
        Image *img = new Image;
        memcpy(img, in, sizeof(Image));
        img->SaveConvert<false>();
        img->list.next = 0;
        img->list.prev = out->last_overlay;
        if (!out->first_overlay)
            out->first_overlay = img;
        else
            out->last_overlay->list.next = img;
        out->last_overlay = img;
        if (i == main_image_id)
            out->main_image = img;
        in += sizeof(Image);
        img->parent = out;
    }
    if ((int)out->main_image == main_image_id)
        throw SaveReadFail_("Sprite/image");

    out->AddToHlines();

    return std::make_pair(sizeof(Sprite) + sizeof(Image) * count, out);
}

template <class C, bool uses_temp_ids, class L>
void Load::LoadObjectChunk(std::pair<int, C*> (*LoadSave)(uint8_t *, uint32_t, L *, uint32_t *), L *list_head, std::unordered_map<uint32_t, C *> *temp_id_map)
{
    int count, size;
    if (fread(&count, 4, 1, file) != 1)
        throw SaveException(0, "LoadObjectChunk: eof");
    if (uses_temp_ids && count)
        temp_id_map->reserve(count);
    while (count)
    {
        size = ReadCompressedChunk();
        while (size)
        {
            int diff;
            C *out;
            uint32_t id;
            std::tie(diff, out) = (*LoadSave)(buf, size, list_head, &id);

            if (diff == 0)
                throw SaveException();
            if (uses_temp_ids)
                temp_id_map->insert(std::make_pair(id, out));

            buf += diff;
            size -= diff;
            count -= 1;
            if (size < 0 || (count == 0 && size != 0))
                throw SaveReadFail(C);
        }
    }
}

void Load::LoadUnitPtr(Unit **ptr)
{
    fread(ptr, 1, 4, file);
    ConvertUnitPtr<false>(ptr);
}

Ai::Script *Load::LoadAiScript()
{
    Ai::Script *script = Ai::Script::RawAlloc();
    BufRead(script);
    ConvertAiScript<false>(script);
    return script;
}

Ai::MilitaryAi *Load::LoadMilitaryAi()
{
    Ai::MilitaryAi *ai = new Ai::MilitaryAi;
    BufRead(ai);
    try
    {
        ConvertAiParent<false>(ai);
    }
    catch (const SaveException &e)
    {
        auto ex = NewSaveConvertFail(Ai::MilitaryAi, ai, &e);
        delete ai;
        throw ex;
    }
    ai->parent->ai = (Ai::UnitAi *)ai;

    return ai;
}

int Load::LoadAiRegion(Ai::Region *region, uint32_t size)
{
    if (size < sizeof(Ai::Region) + 4)
        throw SaveReadFail(Ai::Region);
    size -= sizeof(Ai::Region) + 4;

    BufRead(region);
    ConvertAiRegion<false>(region);

    uint32_t ai_count;
    BufRead(&ai_count);
    if (size < ai_count * sizeof(Ai::MilitaryAi))
        throw SaveReadFail(Ai::MilitaryAi);

    Ai::MilitaryAi *prev = 0;
    for (uint32_t i = 0; i < ai_count; i++)
    {
        Ai::MilitaryAi *ai = LoadMilitaryAi();
        ai->region = region;
        if (prev)
            prev->list.next = ai;
        else
            region->military = ai;
        ai->list.prev = prev;
        prev = ai;
    }
    if (prev)
        prev->list.next = 0;

    return sizeof(Ai::Region) + 4 + ai_count * sizeof(Ai::MilitaryAi);
}

void Load::LoadAiRegions(int player, int region_count)
{
    Ai::Region *regions = bw::player_ai_regions[player];
    try
    {
        while (region_count)
        {
            int size = ReadCompressedChunk();
            while (size)
            {
                int diff = LoadAiRegion(regions, size);
                size -= diff;
                region_count -= 1;
                regions++;
                if (region_count == 0 && size != 0)
                    throw SaveReadFail(Ai::Region);
            }
        }
    }
    catch (const ReadCompressedFail &e)
    {
        throw SaveReadFail_("AiRegion", &e);
    }
}

template <bool active_ais>
void Load::LoadGuardAis(ListHead<Ai::GuardAi, 0x0> &list_head)
{
    int ai_count;
    fread(&ai_count, 4, 1, file);
    Ai::GuardAi *prev = nullptr;
    while (ai_count)
    {
        int size = ReadCompressedChunk();
        while (size)
        {
            if (size < (int)sizeof(Ai::GuardAi))
                throw SaveReadFail(Ai::GuardAi);

            Ai::GuardAi *ai = new Ai::GuardAi;
            BufRead(ai);
            if (active_ais)
            {
                try
                {
                    ConvertAiParent<false>(ai);
                }
                catch (const SaveException &e)
                {
                    auto ex = NewSaveConvertFail(Ai::GuardAi, ai, &e);
                    delete ai;
                    throw ex;
                }
                ai->parent->ai = (Ai::UnitAi *)ai;
            }
            if (prev)
                prev->list.next = ai;
            else
                list_head = ai;
            ai->list.prev = prev;
            prev = ai;

            ai_count -= 1;
            size -= sizeof(Ai::GuardAi);
            if (ai_count == 0 && size != 0)
                throw SaveReadFail(Ai::GuardAi);
        }
    }
    if (prev)
        prev->list.next = nullptr;
}

int Load::LoadAiTown(Ai::Town *out, uint32_t size)
{
    if (size < sizeof(Ai::Town) + 8)
        throw SaveReadFail(Ai::Town);
    BufRead(out);
    ConvertAiTown<false>(out);
    size -= sizeof(Ai::Town) + 4;

    uint32_t worker_ai_count;
    BufRead(&worker_ai_count);
    if (size < worker_ai_count * sizeof(Ai::WorkerAi) + 4)
        throw SaveReadFail(Ai::Town);
    size -= worker_ai_count * sizeof(Ai::WorkerAi) + 4;

    Ai::WorkerAi *prev_worker = 0;
    int i = worker_ai_count;
    while (i)
    {
        Ai::WorkerAi *ai = Ai::WorkerAi::RawAlloc();
        BufRead(ai);
        try
        {
            ConvertAiParent<false>(ai);
        }
        catch (const SaveException &e)
        {
            auto ex = NewSaveConvertFail(Ai::WorkerAi, ai, &e);
            if (prev_worker)
                prev_worker->list.next = 0;
            delete ai;
            throw ex;
        }
        ai->parent->ai = (Ai::UnitAi *)ai;
        ai->town = out;

        if (prev_worker)
            prev_worker->list.next = ai;
        else
            out->first_worker = ai;
        ai->list.prev = prev_worker;
        prev_worker = ai;

        i -= 1;
    }


    uint32_t building_ai_count;
    BufRead(&building_ai_count);
    if (size < building_ai_count * sizeof(Ai::BuildingAi))
        throw SaveReadFail(Ai::Town);

    Ai::BuildingAi *prev_building = 0;
    i = building_ai_count;
    while (i)
    {
        Ai::BuildingAi *ai = Ai::BuildingAi::RawAlloc();
        BufRead(ai);

        ai->town = out;

        if (prev_building)
            prev_building->list.next = ai;
        else
            out->first_building = ai;
        ai->list.prev = prev_building;
        prev_building = ai;

        i -= 1;
    }

    return sizeof(Ai::Town) + 8 + worker_ai_count * sizeof(Ai::WorkerAi) + building_ai_count * sizeof(Ai::BuildingAi);
}

void Load::LoadAiTowns(int player)
{
    uint32_t town_count;
    fread(&town_count, 4, 1, file);
    Ai::Town *prev = 0;
    while (town_count)
    {
        int size = ReadCompressedChunk();
        while (size)
        {
            Ai::Town *town = new Ai::Town;
            int diff;
            try
            {
                diff = LoadAiTown(town, size);
            }
            catch (const SaveException &e)
            {
                delete town;
                if (prev)
                    prev->list.next = 0;
                throw SaveReadFail_("AiTown", &e);
            }
            if (prev)
                prev->list.next = town;
            else
                bw::active_ai_towns[player] = town;
            town->list.prev = prev;
            prev = town;

            size -= diff;
            town_count -= 1;
            if (size < 0 || (town_count == 0 && size != 0))
                throw SaveReadFail(Ai::Town);
        }
    }
    if (prev)
        prev->list.next = 0;

    // BuildingAi:ssa can have Town pointer,
    // it usually points to parent town but i'm not 100% sure so let's do this
    for (Ai::Town *town : bw::active_ai_towns[player])
    {
        for (Ai::BuildingAi *ai : town->first_building)
        {
            ConvertBuildingAi<false>(ai, town->player);
            ai->parent->ai = (Ai::UnitAi *)ai;
        }
    }
}

void Load::LoadPlayerAiData(int player)
{
    bw::ReadCompressed(&bw::player_ai[player], sizeof(Ai::PlayerData), (File *)file);
    ConvertPlayerAiData<false>(&bw::player_ai[player], player);
}

void Load::LoadAiChunk()
{
    uint32_t region_count;
    fread(&region_count, 4, 1, file);
    bw::DeleteAiRegions();
    bw::AllocateAiRegions(region_count);
    for (unsigned i = 0; i < Limits::ActivePlayers; i++)
    {
        if (bw::players[i].type == 1)
        {
            LoadAiRegions(i, region_count);
            LoadGuardAis<true>(bw::first_guard_ai[i]);
            LoadGuardAis<false>(Ai::needed_guards[i]);
            LoadAiTowns(i);
            LoadPlayerAiData(i);
        }
    }
    int count, size;
    fread(&count, 1, 4, file);
    Ai::Script *prev = nullptr;
    while (count)
    {
        size = ReadCompressedChunk();
        while (size)
        {
            if (size < (int)sizeof(Ai::Script))
                throw SaveReadFail(Ai::Script);

            Ai::Script *script = LoadAiScript();
            if (prev)
                prev->list.next = script;
            else
                *bw::first_active_ai_script = script;
            script->list.prev = prev;
            script->list.next = nullptr;

            size -= sizeof(Ai::Script);
            count -= 1;
            if (size < 0 || (count == 0 && size != 0))
                throw SaveReadFail(Ai::Script);
            prev = script;
        }
    }
    bw::ReadCompressed(bw::resource_areas.raw_pointer(), 0x2ee8, (File *)file);
}

void Load::LoadPathingChunk()
{
    using namespace Pathing;
    uint32_t chunk_size;
    fread(&chunk_size, 4, 1, file);
    std::unique_ptr<uint8_t[]> chunk(new uint8_t[chunk_size]);
    uint8_t *pos = chunk.get();
    bw::ReadCompressed(chunk.get(), chunk_size, (File *)file);

    PathingSystem *pathing = *bw::pathing =
        (PathingSystem *)storm::SMemAlloc(sizeof(PathingSystem), "LoadPathingChunk", 42, 0);
    memcpy(pathing, pos, sizeof(PathingSystem));
    ConvertPathing<false>(pathing);
    pos += sizeof(PathingSystem);

    ContourData *contours = pathing->contours =
        (ContourData *)storm::SMemAlloc(sizeof(ContourData), "LoadPathingChunk", 42, 0);
    memcpy(pathing->contours, pos, sizeof(ContourData));
    pos += sizeof(ContourData);

    contours->top_contours =
        (Contour *)storm::SMemAlloc(sizeof(Contour) * contours->top_contour_count, "LoadPathingChunk", 42, 0);
    memcpy(contours->top_contours, pos, contours->top_contour_count * sizeof(Contour));
    pos += contours->top_contour_count * sizeof(Contour);

    contours->right_contours =
        (Contour *)storm::SMemAlloc(sizeof(Contour) * contours->right_contour_count, "LoadPathingChunk", 42, 0);
    memcpy(contours->right_contours, pos, contours->right_contour_count * sizeof(Contour));
    pos += contours->right_contour_count * sizeof(Contour);

    contours->bottom_contours =
        (Contour *)storm::SMemAlloc(sizeof(Contour) * contours->bottom_contour_count, "LoadPathingChunk", 42, 0);
    memcpy(contours->bottom_contours, pos, contours->bottom_contour_count * sizeof(Contour));
    pos += contours->bottom_contour_count * sizeof(Contour);

    contours->left_contours =
        (Contour *)storm::SMemAlloc(sizeof(Contour) * contours->left_contour_count, "LoadPathingChunk", 42, 0);
    memcpy(contours->left_contours, pos, contours->left_contour_count * sizeof(Contour));
}

void Load::LoadGame()
{
    lone_sprites->Deserialize(this);
//  LoadObjectChunk<Flingy, false>(&Flingy::SaveAllocate, &first_allocated_flingy, 0);
    bullet_system->Deserialize(this);
    id_to_bullet.reserve(bullet_system->BulletCount());
    bullet_system->MakeSaveIdMapping([this] (Bullet *bullet, uintptr_t id) {
        id_to_bullet[id] = bullet;
    });
    id_to_sprite.reserve(lone_sprites->lone_sprites.size());
    lone_sprites->MakeSaveIdMapping([this] (Sprite *sprite, uintptr_t id) {
        id_to_sprite[id] = sprite;
    });
    LoadObjectChunk<Unit, false>(&Unit::SaveAllocate, &first_allocated_unit, 0);
    bullet_system->FinishLoad(this); // Bullets reference units and vice versa

    for (Unit *unit : first_allocated_unit)
    {
        ConvertUnit<false>(unit);
        for (Order *order : unit->order_queue_begin)
        {
            ConvertUnitPtr<false>(&order->target);
        }
        if (unit->path)
        {
            ConvertPath<false>(unit->path.get());
        }
    }
    fread(&Unit::next_id, 4, 1, file);
    LoadUnitPtr(&(*bw::first_invisible_unit).AsRawPointer());
    LoadUnitPtr(&(*bw::first_active_unit).AsRawPointer());
    LoadUnitPtr(&(*bw::first_hidden_unit).AsRawPointer());
    LoadUnitPtr(&(*bw::first_dying_unit).AsRawPointer());
    LoadUnitPtr(&(*bw::first_revealer).AsRawPointer());
    LoadUnitPtr(&*bw::last_active_unit);
    LoadUnitPtr(&*bw::last_hidden_unit);
    LoadUnitPtr(&*bw::last_dying_unit);
    LoadUnitPtr(&*bw::last_revealer);
    for (unsigned i = 0; i < Limits::Players; i++)
        LoadUnitPtr(&bw::first_player_unit[i].AsRawPointer());
    for (unsigned i = 0; i < Limits::Players; i++)
        ValidateList(bw::first_player_unit[i]);

    uint32_t original_tile_length;
    fread(&original_tile_length, 1, 4, file);
    bw::ReadCompressed(*bw::original_tiles, original_tile_length, (File *)file);
    bw::ReadCompressed(*bw::creep_tile_borders, original_tile_length / 2, (File *)file);
    if (!bw::LoadDisappearingCreepChunk((File *)file))
        throw SaveException();
    bw::ReadCompressed(*bw::map_tile_ids, Limits::MapHeight_Tiles * Limits::MapWidth_Tiles * 2, (File *)file);
    bw::ReadCompressed(*bw::megatiles, Limits::MapHeight_Tiles * Limits::MapWidth_Tiles * 2, (File *)file);
    bw::ReadCompressed(*bw::map_tile_flags, Limits::MapHeight_Tiles * Limits::MapWidth_Tiles * 4, (File *)file);

    if (!bw::LoadTriggerChunk((File *)file))
        throw SaveException();
    fread(bw::scenario_chk_STR_size.raw_pointer(), 1, 4, file);
    storm::SMemFree(*bw::scenario_chk_STR, "notasourcefile", 42, 0);
    *bw::scenario_chk_STR = storm::SMemAlloc(*bw::scenario_chk_STR_size, "notasourcefile", 42, 0);
    bw::ReadCompressed(*bw::scenario_chk_STR, *bw::scenario_chk_STR_size, (File *)file);
    bw::ReadCompressed(bw::selection_groups.raw_pointer(), Limits::Selection * Limits::ActivePlayers * sizeof(Unit *), (File *)file);
    for (auto selection : bw::selection_groups)
    {
        for (Unit *&unit : selection)
        {
            ConvertUnitPtr<false>(&unit);
        }
    }

    LoadPathingChunk();
    unit_search->Init();

    LoadAiChunk();
    if (!bw::LoadDatChunk((File *)file, 0x3))
        throw SaveException();
    fread(bw::screen_x.raw_pointer(), 1, 4, file);
    fread(bw::screen_y.raw_pointer(), 1, 4, file);

    bw::RestorePylons();
    bw::AddSelectionOverlays();
    if (!IsMultiplayer())
        *bw::single_player_custom_game = *bw::single_player_custom_game_saved;

    bw::MoveScreen(*bw::screen_x, *bw::screen_y);
    InitCursorMarker();
}

int LoadGameObjects()
{
    Load load(*bw::loaded_save);
    bool success = true;
    try
    {
        load.LoadGame();
        *bw::load_succeeded = 1;
    }
    catch (const SaveException &e)
    {
        debug_log->Log("Load failed: %s\n", e.cause().c_str());
        *bw::load_succeeded = 0;
        *bw::replay_data = nullptr; // Otherwise it would save empty lastreplay
        success = false;
    }
    load.Close();
    *bw::loaded_save = 0;
    return success;
}
