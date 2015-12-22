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
#include <streambuf>
#include <unordered_map>
#include <string>

#include <cereal/cereal.hpp>
#include <cereal/archives/binary.hpp>
#include <cereal/types/memory.hpp>

#ifndef SEEK_SET
#define SEEK_CUR 1
#define SEEK_END 2
#define SEEK_SET 0
#endif

using std::get;

// Bw also has its own version, but we won't use that
const uint32_t SaveVersion = 1;

/// Helper for nicely logging SaveFail info
/// (Uses global error_log)
static void PrintNestedExceptions(const std::exception &e)
{
    try {
        std::rethrow_if_nested(e);
    } catch (const std::exception &e) {
        error_log->Log("    %s\n", e.what());
        PrintNestedExceptions(e);
    } catch (...) { }
}

/// Exception which is thrown when something fails during saving or loading
class SaveFail : public std::exception
{
    public:
        enum Type {
            Version,
            Unit,
            /// Misc unit globals
            UnitMisc,
            UnitPointer,
            SpritePointer,
            BulletPointer,
            Grp,
            ImageRemap,
            OwnedList,
            // ReadCompressed had too much data
            ExtraData,
            // ReadCompressed didn't have expected amount of data
            CompressedFormat
        };
        SaveFail(Type ty) : type(ty), index(~0), desc(type_description()) { }
        SaveFail(Type ty, uintptr_t index) : type(ty), index(index), desc(type_description())
        {
            char buf[16];
            if (type == ExtraData)
                snprintf(buf, sizeof buf, " (%d bytes)", index);
            else
                snprintf(buf, sizeof buf, " #%d", index);
            desc += buf;
        }

        const char *type_description() const
        {
            switch (type)
            {
                case Version:
                    return "Save version";
                case Unit:
                    return "Unit";
                case UnitMisc:
                    return "Unit globals";
                case UnitPointer:
                    return "Unit *";
                case SpritePointer:
                    return "Sprite *";
                case BulletPointer:
                    return "Bullet *";
                case Grp:
                    return "Grp";
                case ImageRemap:
                    return "Image remap";
                case OwnedList:
                    return "Owned list";
                case ExtraData:
                    return "Too large compressed chunk";
                case CompressedFormat:
                    return "Invalid compressed chunk";
                default:
                    return "???";
            }
        }

        Type type;
        uintptr_t index;
        std::string desc;

        virtual const char *what() const noexcept override { return desc.c_str(); }
};

/// Output stream for cereal, writing to the same FILE * as bw functions do.
/// Also compresses the output using bw's WriteCompressed().
class CompressedOutputStreambuf : public std::streambuf  {
    const int BufferSize = 0x8000;
    public:
        CompressedOutputStreambuf(FILE *f) : std::streambuf(), file(f),
            buffer(make_unique<char[]>(BufferSize))
        {
            setp(buffer.get(), buffer.get() + BufferSize);
        }

    protected:
        virtual int sync() override
        {
            if (FlushBuffer())
                return 0;
            return -1;
        }
        virtual int_type overflow(int_type ch) override
        {
            if (!FlushBuffer())
                return traits_type::eof();
            if (traits_type::eq_int_type(ch, traits_type::eof()) != true)
            {
                sputc(ch);
            }
            return traits_type::to_int_type(0);
        }

    private:
        bool FlushBuffer()
        {
            uintptr_t size = pptr() - pbase();
            fwrite(&size, 1, sizeof(uintptr_t), file);
            WriteCompressed((File *)file, pbase(), size);
            setp(buffer.get(), buffer.get() + BufferSize);
            return true; // TODO check WriteCompressed return val
        }

        FILE *file;
        ptr<char[]> buffer;
};

/// Loading counterpart for CompressedOutputStreambuf.
class CompressedInputStreambuf : public std::streambuf  {
    public:
        CompressedInputStreambuf(FILE *f) : file(f), buffer(nullptr), bufsize(0)
        {
        }

        void ConfirmEmptyBuffer() const
        {
            if (gptr() != egptr())
                throw SaveFail(SaveFail::ExtraData, egptr() - gptr());
        }

    protected:
        virtual int_type underflow() override
        {
            uintptr_t size;
            auto read = fread(&size, 1, sizeof(uintptr_t), file);
            if (read != sizeof(uintptr_t))
                return traits_type::eof();

            if (size > bufsize)
            {
                buffer = make_unique<char[]>(size);
                bufsize = size;
            }
            if (!bw::funcs::ReadCompressed((File *)file, buffer.get(), size))
                throw SaveFail(SaveFail::CompressedFormat);
            setg(buffer.get(), buffer.get(), buffer.get() + size);
            return traits_type::to_int_type(buffer[0]);
        }

    private:
        FILE *file;
        ptr<char[]> buffer;
        uintptr_t bufsize;
};

/// Combines cereal's BinaryOutputArchive with CompressedOutputStreambuf
/// and provides access to Sprite/Bullet to id mappings (and version).
/// If saving is too slow, not using std streams would help with performance.
class SaveArchive : public cereal::OutputArchive<SaveArchive>
{
    public:
        SaveArchive(FILE *file, Save *save);
        void Flush() { stream.flush(); }

        bool IsSaving() const { return true; }
        uint32_t Version() const { return version; }

    private:
        CompressedOutputStreambuf buffer;
        std::ostream stream;

    public:
        cereal::BinaryOutputArchive inner;

        const std::unordered_map<const Sprite *, uintptr_t> &sprites;
        const std::unordered_map<const Bullet *, uintptr_t> &bullets;

    private:
        uint32_t version;
};

template<class T>
typename std::enable_if<std::is_arithmetic<T>::value, void>::type
    save(SaveArchive &ar, const T & t)
{
    ar.inner(t);
}

template <class T>
void save(SaveArchive &ar, const cereal::SizeTag<T> & t)
{
    ar(t.size);
}

template<class T>
void save(SaveArchive &ar, const cereal::NameValuePair<T> & t)
{
    ar(t.value);
}

template <class T>
void save(SaveArchive &ar, const cereal::BinaryData<T> & bd)
{
    ar.inner(bd);
}

CEREAL_REGISTER_ARCHIVE(SaveArchive);

/// See SaveArchive.
/// Does not provide Pointer-id mappings, they are handled by FixupArchive
class LoadArchive : public cereal::InputArchive<LoadArchive>
{
    public:
        LoadArchive(FILE *file, Load *load);

        void ConfirmEmptyBuffer() const { buffer.ConfirmEmptyBuffer(); }

        bool IsSaving() const { return false; }
        uint32_t Version() const;

    private:
        CompressedInputStreambuf buffer;
        std::basic_istream<char> stream;

    public:
        cereal::BinaryInputArchive inner;

    private:
        const Load *load;
};

template<class T>
typename std::enable_if<std::is_arithmetic<T>::value, void>::type
    load(LoadArchive &ar, T & t)
{
    ar.inner(t);
}

template <class T>
void load(LoadArchive &ar, cereal::SizeTag<T> & t)
{
    ar(t.size);
}

template<class T>
void load(LoadArchive &ar, cereal::NameValuePair<T> & t)
{
    ar(t.value);
}

template <class T>
void load(LoadArchive &ar, cereal::BinaryData<T> & bd)
{
    ar.inner(bd);
}

CEREAL_REGISTER_ARCHIVE(LoadArchive);

/// Behaves like a cereal archive, but does not actually read from anything,
/// just applies pointer fixups from Load and no-op for integer data types.
/// Throws SaveFail when meeting a pointer without valid save id.
class FixupArchive : public cereal::InputArchive<FixupArchive>
{
    public:
        typedef std::unordered_map<uintptr_t, Sprite *> SpriteFixup;
        typedef std::unordered_map<uintptr_t, Bullet *> BulletFixup;

        FixupArchive(uint32_t version, const SpriteFixup &s, const BulletFixup &b) :
            cereal::InputArchive<FixupArchive>(this), sprites(s), bullets(b), version(version) { }

        bool IsSaving() const { return false; }
        uint32_t Version() const { return version; }

        const SpriteFixup &sprites;
        const BulletFixup &bullets;
        uint32_t version;
};

CEREAL_REGISTER_ARCHIVE(FixupArchive);

template<class Parent>
class SaveBase
{
    public:
        SaveBase() {}
        ~SaveBase();
        void Close();

    protected:
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
};

class Save : public SaveBase<Save>
{
    friend class SaveArchive;
    public:
        Save(FILE *file, uint32_t version);
        ~Save() {}
        void SaveGame(uint32_t time);

        Sprite *FindSpriteById(uint32_t id) { return 0; }
        Bullet *FindBulletById(uint32_t id) { return 0; }

        bool IsOk() const { return file != nullptr; }

    private:
        void WriteCompressedChunk();
        template <class C>
        void BeginBufWrite(C **out, C *in = 0);

        void SaveUnits();
        void SaveUnitPtr(Unit *ptr);

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

        // Most likely SaveVersion, but can be configured anyways
        uint32_t version;

        SaveArchive cereal_archive;

        std::unordered_map<const Sprite *, uintptr_t> sprite_to_id;
        std::unordered_map<const Bullet *, uintptr_t> bullet_to_id;
};

class Load : public SaveBase<Load>
{
    friend class LoadArchive;
    public:
        Load(FILE *file);
        ~Load();
        void LoadGame();

        void Read(void *buf, int size);
        void ReadCompressed(void *out, int size);

    private:
        int ReadCompressedChunk();
        void ReadCompressed(FILE *file, void *out, int size);
        void LoadUnitPtr(Unit **ptr);
        void LoadAiChunk();
        void LoadAiRegions(int player, int region_count);
        template <bool active_ais> void LoadGuardAis(ListHead<Ai::GuardAi, 0x0> &list_head);
        void LoadAiTowns(int player);
        void LoadPlayerAiData(int player);

        int LoadAiRegion(Ai::Region *region, uint32_t size);
        int LoadAiTown(Ai::Town *out, uint32_t size);
        Ai::MilitaryAi *LoadMilitaryAi();
        Ai::Script *LoadAiScript();

        void LoadPathingChunk();

        void LoadUnits();
        void FixupUnits(FixupArchive &fixup);

        template <class C>
        void BufRead(C *out);

        uint8_t *buf_beg;
        uint8_t *buf;
        uint8_t *buf_end;
        uint32_t buf_size;

        uint32_t version;

        LoadArchive cereal_archive;
};

/// Has to be after Save declaration
SaveArchive::SaveArchive(FILE *file, Save *save) : cereal::OutputArchive<SaveArchive>(this),
    buffer(file), stream(&buffer), inner(stream), sprites(save->sprite_to_id), bullets(save->bullet_to_id),
    version(save->version)
{
}

LoadArchive::LoadArchive(FILE *file, Load *load) : cereal::InputArchive<LoadArchive>(this),
    buffer(file), stream(&buffer), inner(stream), load(load)
{
}

uint32_t LoadArchive::Version() const { return load->version; }

// FixupArchive specializations
template<class T>
typename std::enable_if<std::is_arithmetic<T>::value, void>::type
    load(FixupArchive &ar, T & t)
{
    // nothing
}

template <class T>
void load(FixupArchive &ar, cereal::SizeTag<T> & t)
{
    // nothing
}

template <class T>
void load(FixupArchive &ar, cereal::NameValuePair<T> & t)
{
    ar(t.value);
}

template <class T>
void load(FixupArchive &ar, cereal::BinaryData<T> & bd)
{
    // nothing
}

template <class T>
void load(FixupArchive &ar, std::unique_ptr<T> &ptr)
{
    if (ptr)
    {
        ar(*ptr);
    }
}

CEREAL_SPECIALIZE_FOR_ALL_ARCHIVES(Unit *, cereal::specialization::non_member_load_save);
CEREAL_SPECIALIZE_FOR_ALL_ARCHIVES(Bullet *, cereal::specialization::non_member_load_save);
CEREAL_SPECIALIZE_FOR_ALL_ARCHIVES(Sprite *, cereal::specialization::non_member_load_save);

void save(SaveArchive &archive, Unit * const &ptr)
{
    if (ptr == nullptr)
        archive((uintptr_t)0);
    else
        archive(ptr->lookup_id);
}

template <class Archive>
void load(Archive &archive, Unit *&ptr)
{
    uintptr_t id;
    archive(id);
    reinterpret_cast<uintptr_t &>(ptr) = id;
}

void load(FixupArchive &ar, Unit *&ptr)
{
    if (ptr != nullptr)
    {
        Unit *unit = Unit::FindById((uintptr_t)(ptr));
        if (unit == nullptr)
            throw SaveFail(SaveFail::UnitPointer);
        ptr = unit;
    }
}

void save(SaveArchive &archive, Sprite * const &ptr)
{
    try {
        uintptr_t id = ptr == nullptr ? 0 : archive.sprites.at(ptr);
        archive(id);
    }
    catch (const std::out_of_range &e) {
        throw SaveFail(SaveFail::SpritePointer);
    }
}

template <class Archive>
void load(Archive &archive, Sprite *&ptr)
{
    uintptr_t id;
    archive(id);
    reinterpret_cast<uintptr_t &>(ptr) = id;
}

void load(FixupArchive &ar, Sprite *&ptr)
{
    if (ptr != nullptr)
    {
        try {
            ptr = ar.sprites.at((uintptr_t)ptr);
        } catch (const std::out_of_range &e) {
            throw SaveFail(SaveFail::SpritePointer);
        }
    }
}

void save(SaveArchive &archive, Bullet * const &ptr)
{
    try {
        uintptr_t id = ptr == nullptr ? 0 : archive.bullets.at(ptr);
        archive(id);
    }
    catch (const std::out_of_range &e) {
        throw SaveFail(SaveFail::BulletPointer);
    }
}

template <class Archive>
void load(Archive &archive, Bullet *&ptr)
{
    uintptr_t id;
    archive(id);
    reinterpret_cast<uintptr_t &>(ptr) = id;
}

void load(FixupArchive &ar, Bullet *&ptr)
{
    if (ptr != nullptr)
    {
        try {
            ptr = ar.bullets.at((uintptr_t)ptr);
        } catch (const std::out_of_range &e) {
            throw SaveFail(SaveFail::BulletPointer);
        }
    }
}

/// Used for handling grp and drawfunc param pointer serialization
class ImageDrawfuncParam { };

template <class Archive, class T, unsigned O>
void serialize(Archive &archive, RevListEntry<T, O> &list_entry)
{
    archive(list_entry.next, list_entry.prev);
}

template <class Archive, class T, unsigned O>
void serialize(Archive &archive, ListEntry<T, O> &list_entry)
{
    archive(list_entry.next, list_entry.prev);
}

namespace Common {
    template <class Archive, class Size>
    void serialize(Archive &archive, xint<Size> &val)
    {
        Size as_int = val;
        archive(as_int);
        val = as_int;
    }

    template <class Archive, class Size>
    void serialize(Archive &archive, yint<Size> &val)
    {
        Size as_int = val;
        archive(as_int);
        val = as_int;
    }

    template <class Archive>
    void serialize(Archive &archive, Point16 &point)
    {
        archive(point.x, point.y);
    }

    template <class Archive>
    void serialize(Archive &archive, Point32 &point)
    {
        archive(point.x, point.y);
    }

    template <class Archive>
    void serialize(Archive &archive, Rect16 &rect)
    {
        archive(rect.left, rect.top, rect.right, rect.bottom);
    }
}

template <class Archive, class T, uintptr_t N, class A>
void save(Archive &archive, const UnsortedList<T, N, A> &list)
{
    archive(list.size());
    for (auto &val : list)
        archive(val);
}

template <class Archive, class T, uintptr_t N, class A>
void load(Archive &archive, UnsortedList<T, N, A> &list)
{
    uintptr_t size;
    archive(size);
    list.clear_keep_capacity();
    for (uintptr_t i = 0; i < size; i++)
    {
        list.emplace();
        T &val = list.back();
        archive(val);
    }
}

template <class T, uintptr_t N, class A>
void load(FixupArchive &archive, UnsortedList<T, N, A> &list)
{
    for (auto &val : list)
        archive(val);
}

/// Handles serialization of Order lists in Units and such
template <class T, unsigned O>
struct OwnedList
{
    OwnedList(ListHead<T, O> &head, RevListHead<T, O> &end) : head(head), end(end), main(nullptr) { }
    OwnedList(ListHead<T, O> &head, RevListHead<T, O> &end, T *&main) : head(head), end(end), main(&main) { }

    template <class Archive>
    void save(Archive &archive) const
    {
        uintptr_t count = 0;
        uintptr_t main_pos = ~0;
        for (T *item : head)
        {
            if (main && item == *main)
                main_pos = count;
            count += 1;
        }
        archive(count);
        if (main != nullptr)
        {
            if (main_pos == ~0)
                throw SaveFail(SaveFail::OwnedList);
            archive(main_pos);
        }
        for (T *item : head)
        {
            archive(*item);
        }
    }

    template <class Archive>
    void load(Archive &archive)
    {
        uintptr_t count;
        uintptr_t main_pos;
        archive(count);
        if (main != nullptr)
            archive(main_pos);
        head = nullptr;
        end = nullptr;
        for (uintptr_t i = 0; i < count; i++)
        {
            T *value = new T;
            archive(*value);
            RevListEntry<T, O>::FromValue(value)->Add(end);
            if (main != nullptr && i == main_pos)
                *main = value;
        }
        for (T *item : end)
            head = item;
    }

    void load(FixupArchive &archive)
    {
        for (T *item : head)
        {
            archive(*item);
        }
    }
    ListHead<T, O> &head;
    RevListHead<T, O> &end;
    T **main;
};

/// Convenience class for loading/saving all unit pointers at once
class GlobalUnitPointers
{
    public:
        constexpr GlobalUnitPointers() { }

        template <class Archive>
        void serialize(Archive &archive)
        {
            try {
                archive(*bw::first_invisible_unit);
                archive(*bw::first_active_unit, *bw::first_hidden_unit);
                archive(*bw::first_dying_unit, *bw::first_revealer);
                archive(*bw::last_active_unit, *bw::last_hidden_unit);
                archive(*bw::last_dying_unit, *bw::last_revealer);
                for (unsigned i = 0; i < Limits::Players; i++)
                    archive(bw::first_player_unit[i]);

                for (auto selection : bw::selection_groups)
                {
                    for (Unit *&unit : selection)
                    {
                        archive(unit);
                    }
                }
            } catch (const std::exception &e) {
                std::throw_with_nested(SaveFail(SaveFail::UnitMisc));
            }
        }
};

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

template <class P>
SaveBase<P>::~SaveBase()
{
    Close();
}

Save::Save(FILE *f, uint32_t version) : version(version), cereal_archive(f, this)
{
    file = f;
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

Load::Load(FILE *file) : cereal_archive(file, this)
{
    this->file = file;
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
    WriteCompressed((File *)file, buf->GetData(), buf->Length());
    buf->Clear();
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
    ReadCompressed(file, buf, size);
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
    if (!bw::funcs::ReadCompressed((File *)file, out, size))
        throw ReadCompressedFail(out);
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
    WriteCompressed((File *)file, buf->GetData(), buf->Length());
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

    WriteCompressed((File *)file, bw::resource_areas.raw_pointer(), 0x2ee8);
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

    WriteCompressed((File *)file, chunk.get(), chunk_size);
}

void Save::SaveUnits()
{
    for (Unit *unit : first_allocated_unit)
    {
        // Lazy and rather inefficent way to tell when there are no more units.
        // Having just UnitSystem or something instead of global linked list
        // would be enough to fix.
        cereal_archive(true, *unit);
    }
    cereal_archive(false);
    cereal_archive(Unit::next_id);
}

void Load::LoadUnits()
{
    uintptr_t index = 0;
    try {
        bool next;
        while (true)
        {
            cereal_archive(next);
            if (!next)
                break;

            Unit *unit = Unit::RawAlloc();
            cereal_archive(*unit);
            // Ai will be restored later - as long as loading doesn't fail before that
            unit->ai = nullptr;
            // Also pylon aura and list have garbage data
            if (unit->unit_id == Unit::Pylon)
            {
                unit->pylon.aura.release();
                unit->pylon_list.list.next = nullptr;
                unit->pylon_list.list.prev = nullptr;
            }
            unit->allocated.Add(first_allocated_unit);
            unit->AddToLookup();
            if (unit->search_left != -1)
            {
                // Bw does that but it should only set a flag that would be already set
                // And accessing unit search now is not safe as search indices are wrong
                //CheckUnstack(out);
                if (unit->flags & UnitStatus::Building)
                    SetBuildingTileFlag(unit, unit->sprite->position.x, unit->sprite->position.y);
                if (unit->IsFlying())
                    IncrementAirUnitx14eValue(unit);
                unit_search->Add(unit);
            }
            index += 1;
        }
    } catch (const std::exception &e) {
        std::throw_with_nested(SaveFail(SaveFail::Unit, index));
    }
    try {
        cereal_archive(Unit::next_id);
    } catch (const std::exception &e) {
        std::throw_with_nested(SaveFail(SaveFail::UnitMisc));
    }
}

void Load::FixupUnits(FixupArchive &fixup)
{
    for (Unit *unit : first_allocated_unit)
    {
        fixup(*unit);
        unit->sprite->AddToHlines();
    }
}

void Save::SaveGame(uint32_t time)
{
    Sprite::RemoveAllSelectionOverlays();

    WriteReadableSaveHeader((File *)file, filename.c_str());
    WriteSaveHeader((File *)file, time);

    *bw::unk_57F240 = (GetTickCount() - *bw::unk_59CC7C) / 1000 + *bw::unk_6D5BCC;
    WriteCompressed((File *)file, bw::players.raw_pointer(), sizeof(Player) * Limits::Players);
    if (!*bw::campaign_mission)
        ReplaceWithShortPath(&bw::map_path[0], MAX_PATH);
    WriteCompressed((File *)file, bw::minerals.raw_pointer(), 0x17700);
    fwrite(bw::local_player_id.raw_pointer(), 1, 4, file);
    if (!*bw::campaign_mission)
        ReplaceWithFullPath(&bw::map_path[0], MAX_PATH);

    bullet_to_id.reserve(bullet_system->BulletCount());
    bullet_system->MakeSaveIdMapping([this] (Bullet *bullet, uintptr_t id) {
        bullet_to_id[bullet] = id;
    });
    sprite_to_id.reserve(lone_sprites->lone_sprites.size());
    lone_sprites->MakeSaveIdMapping([this] (Sprite *sprite, uintptr_t id) {
        sprite_to_id[sprite] = id;
    });

    cereal_archive(version);
    cereal_archive(*lone_sprites, *bullet_system);
    SaveUnits();
    GlobalUnitPointers global_unit_ptrs;
    cereal_archive(global_unit_ptrs);
    cereal_archive.Flush();

    uint32_t original_tile_length = *bw::original_tile_width * *bw::original_tile_height * 2;
    fwrite(&original_tile_length, 1, 4, file);
    WriteCompressed((File *)file, *bw::original_tiles, original_tile_length);
    WriteCompressed((File *)file, *bw::creep_tile_borders, original_tile_length / 2);
    SaveDisappearingCreepChunk((File *)file);
    WriteCompressed((File *)file, *bw::map_tile_ids, Limits::MapHeight_Tiles * Limits::MapWidth_Tiles * 2);
    WriteCompressed((File *)file, *bw::megatiles, Limits::MapHeight_Tiles * Limits::MapWidth_Tiles * 2);
    WriteCompressed((File *)file, *bw::map_tile_flags, Limits::MapHeight_Tiles * Limits::MapWidth_Tiles * 4);

    SaveTriggerChunk((File *)file);
    fwrite(bw::scenario_chk_STR_size.raw_pointer(), 1, 4, file);
    WriteCompressed((File *)file, *bw::scenario_chk_STR, *bw::scenario_chk_STR_size);

    SavePathingChunk();

    SaveAiChunk();
    SaveDatChunk((File *)file);
    fwrite(bw::screen_x.raw_pointer(), 1, 4, file);
    fwrite(bw::screen_y.raw_pointer(), 1, 4, file);

    AddSelectionOverlays();
}

void Command_Save(const uint8_t *data)
{
    if (IsMultiplayer() && FirstCommandUser() != *bw::lobby_command_user)
        return;
    if (bw::game_data->got.unk_tournament)
        return;

    const char *filename = (const char *)data + 5;
    uint32_t time = *(uint32_t *)(data + 1);

    int len = Sc_strlen(filename, 0x1c, 0x1c, false, false);
    if (len == 0) // color / control chars
        return;
    if (!IsInvalidFilename(filename, 0, len - 1)) // wtf?
        return;
    SaveGame(filename, time);
}

void SaveGame(const char *filename, uint32_t time)
{
    char full_path[MAX_PATH];
    if (GetUserFilePath(filename, full_path, MAX_PATH, 0) == 0)
        return;
    if (IsInvalidFilename(full_path, 1, MAX_PATH))
        return;

    char dialog_string[MAX_PATH];
    snprintf(dialog_string, MAX_PATH, "%s \"%s\"", (*bw::network_tbl)->GetTblString(NetworkString::SaveDialogMsg), filename);

    auto orig_cmd_user = *bw::command_user, orig_select_cmd_user = *bw::select_command_user;
    *bw::command_user = *bw::local_player_id;
    *bw::select_command_user = *bw::local_unique_player_id;
    ShowWaitDialog(dialog_string);
    *bw::command_user = orig_cmd_user;
    *bw::select_command_user = orig_select_cmd_user;

    FILE *file = fopen(full_path, "wb+");
    Save save(file, SaveVersion);
    if (save.IsOk())
    {
        try
        {
            save.SaveGame(time);
        }
        catch (const SaveConvertFail<Image> &e)
        {
            error_log->Log("Save convert fail:");
            for (unsigned i = 0; i < sizeof(Image); i++)
                error_log->Log(" %02x", e.data[i]);
            error_log->Log("\n");
        }
        catch (const SaveException &e)
        {
            auto cause = e.cause();
            error_log->Log("Save failed: %s\n", cause.c_str());
        }
        catch (const std::exception &e)
        {
            error_log->Log("Save failed: %s\n", e.what());
            PrintNestedExceptions(e);
        }
        save.Close();
    }
    else
        fclose(file);
    // Here would be some error handling but meh ^_^
    HidePopupDialog();
}

template <class Archive>
void Unit::serialize(Archive &archive)
{
    archive(sprite, hitpoints, move_target, next_move_waypoint, unk_move_waypoint, flingy_flags);
    archive(facing_direction, flingy_turn_speed, movement_direction, flingy_id, _unknown_0x026);
    archive(flingy_movement_type, position, exact_position, flingy_top_speed, current_speed, next_speed);
    archive(speed, acceleration, new_direction, target_direction, player, order, order_state);
    // At least unused52 is used, because reasons... (TODO make clearer and avoid saving unused data)
    archive(order_signal, order_fow_unit, unused52, unused53, order_timer, ground_cooldown, air_cooldown);
    archive(spell_cooldown, order_target_pos, shields, unit_id, unused66, highlighted_order_count);
    archive(order_wait, unk86, attack_notify_timer, previous_unit_id, lastEventTimer, lastEventColor);
    archive(unused8c, rankIncrease, last_attacking_player, secondary_order_wait, ai_spell_flags, order_flags);
    archive(buttons, invisibility_effects, movement_state, build_queue, energy, current_build_slot);
    archive(secondary_order, buildingOverlayState, build_hp_gain, build_shield_gain, previous_hp, lookup_id);
    archive(flags, carried_powerup_flags, wireframe_randomizer, secondary_order_state, move_target_update_timer);
    archive(detection_status, unke8, unkea, path_frame, pathing_flags, _unused_0x106, is_being_healed);
    archive(contourBounds, death_timer, matrix_hp, matrix_timer, stim_timer, ensnare_timer, lockdown_timer);
    archive(irradiate_timer, stasis_timer, plague_timer, is_under_storm, irradiate_player, parasites);
    archive(master_spell_timer, blind, mael_timer, _unused_125, acid_spore_count, acid_spore_timers);
    archive(bullet_spread_seed, _padding_0x132, air_strength, ground_strength, search_left);
    archive(_repulseUnknown, repulseAngle, driftPosX, driftPosY, kills);

    // All other extended data is invalidated at end of each frame and not needed to be saved
    archive(hotkey_groups);

    archive(path);
    archive(list, move_target_unit, target, player_units, subunit, previous_attacker, related);
    archive(currently_building, invisible_list, irradiated_by, first_loaded, next_loaded);

    archive(targeting_bullets, spawned_bullets);
    archive(OwnedList<Order, 0x0>(order_queue_end, order_queue_begin));

    // Union serialization has to be after unit_id, flags, etc it depends on obviously
    if (IsVulture())
        archive(vulture.spiderMineCount);
    else if (HasHangar())
        archive(carrier.in_child, carrier.out_child, carrier.in_hangar_count, carrier.out_hangar_count);
    else if (unit_id == Unit::Scarab || unit_id == Unit::Interceptor)
        archive(interceptor.parent, interceptor.list, interceptor.is_outside_hangar);
    else if (flags & UnitStatus::Building)
    {
        archive(building.addon, building.addonBuildType, building.upgradeResearchTime, building.tech);
        archive(building.upgrade, building.larva_timer, building.is_landing);
        archive(building.creep_timer, building.upgrade_level);
    }
    else if (IsWorker())
    {
        archive(worker.powerup, worker.target_resource_pos, worker.current_harvest_target);
        archive(worker.repair_resource_loss_timer, worker.is_carrying, worker.carried_resource_count);
    }
    else
        archive(everything_c0);

    if (unit_id == Hatchery || unit_id == Lair || unit_id == Hive)
    {
        archive(resource.resource_amount, resource.resourceIscript, resource.awaiting_workers);
        archive(resource.first_awaiting_worker, resource.resource_area, resource.ai_unk);
    }
    else if (unit_id == NydusCanal)
        archive(nydus.exit);
    else if (unit_id == Ghost)
        archive(ghost.nukedot);
    // No need to save the aura or pylon list, bw recreates them
    //else if (unit_id == Pylon)
        //archive(pylon.aura);
    else if (unit_id == NuclearSilo)
        archive(silo.nuke, silo.has_nuke);
    else if (unit_id == Hatchery || unit_id == Lair || unit_id == Hive)
        archive(hatchery.preferred_larvaspawn);
    else if (units_dat_flags[unit_id] & UnitFlags::SingleEntity)
        archive(powerup.origin_point, powerup.carrying_unit);
    else if (IsWorker())
        archive(harvester.previous_harvested, harvester.harvesters);
    else
        archive(everything_d0);

    if (HasRally())
        archive(rally.position, rally.unit);
}

template <class Archive>
void Path::serialize(Archive &archive)
{
    archive(start, next_pos, end, start_frame, dodge_unit, x_y_speed, flags, unk_count);
    archive(direction, total_region_count, unk1c, unk1d, position_count, position_index, values);
}

template <class Archive>
void Sprite::serialize(Archive &archive)
{
    // id (draw ordering tiebreaker) is not saved, due to it being assigned in
    // Sprite constructor. It is kind of messy but works for now..
    archive(sprite_id, player, selectionIndex, visibility_mask);
    archive(elevation, flags, selection_flash_timer, index, width, height, position);
    archive(sort_order, OwnedList<Image, 0x0>(last_overlay, first_overlay, main_image));
    // Doing image parent/drawfunc resetting here is simple, even though done unnecessarily when
    // saving / fixing pointers
    for (Image *img : first_overlay)
    {
        img->parent = this;
        if (img->IsFlipped())
            img->Render = bw::image_renderfuncs[img->drawfunc].flipped;
        else
            img->Render = bw::image_renderfuncs[img->drawfunc].nonflipped;
        img->Update = bw::image_updatefuncs[img->drawfunc].func;
    }
}

template <class Archive>
void Order::serialize(Archive &archive)
{
    archive(order_id, dc9, fow_unit, position, target);
}

template <class Archive>
void Image::serialize(Archive &archive)
{
    if (archive.IsSaving() && drawfunc == HpBar)
    {
        Warning("Saving image (id %x) with hp bar drawfunc\n", image_id);
        return;
    }
    archive(image_id, drawfunc, direction, flags, x_off, y_off, iscript.header, iscript.pos);
    archive(iscript.return_pos, iscript.animation, iscript.wait, frameset, frame, map_position);
    archive(screen_position, grp_bounds);
    archive(make_tuple(image_id, ref(grp)));
    ImageDrawfuncParam **param = (ImageDrawfuncParam **)&drawfunc_param;
    archive(make_tuple(drawfunc, ref(*param)));
}

template <class Archive>
void save(Archive &archive, const tuple<uint16_t, GrpSprite *&> &tuple)
{
    uint16_t image_id = get<uint16_t>(tuple);
    GrpSprite *&grp = get<GrpSprite *&>(tuple);
    if (grp == nullptr)
        archive((uint16_t)0);
    if (grp == bw::image_grps[image_id])
        archive((uint16_t)(image_id + 1));
    else
    {
        for (unsigned int i = 0; i < Limits::ImageTypes; i++)
        {
            if (grp == bw::image_grps[i])
            {
                archive((uint16_t)(i + 1));
                return;
            }
        }
        throw SaveFail(SaveFail::Grp);
    }
}

template <class Archive>
void load(Archive &archive, tuple<uint16_t, GrpSprite *&> &tuple)
{
    GrpSprite *&grp = get<GrpSprite *&>(tuple);
    uint16_t image_id;
    archive(image_id);
    if (image_id > Limits::ImageTypes)
        throw SaveFail(SaveFail::Grp);
    grp = bw::image_grps[image_id - 1];
}

void load(FixupArchive &archive, tuple<uint16_t, GrpSprite *&> &tuple)
{
    // nothing
}

template <class Archive>
void save(Archive &archive, const tuple<uint8_t, ImageDrawfuncParam *&> &tuple)
{
    uint8_t drawfunc = get<uint8_t>(tuple);
    ImageDrawfuncParam *&param = get<ImageDrawfuncParam *&>(tuple);
    if (drawfunc == Image::HpBar)
    {
        Unit *unit = (Unit *)param;
        archive(unit);
    }
    else if (drawfunc == Image::Remap)
    {
        for (int i = 0; i < Limits::RemapPalettes; i++)
        {
            if ((void *&)param == bw::blend_palettes[i].data)
            {
                archive((uint8_t)i);
                return;
            }
        }
        throw SaveFail(SaveFail::ImageRemap);
    }
    else
        archive((uintptr_t)param);
}

template <class Archive>
void load(Archive &archive, tuple<uint8_t, ImageDrawfuncParam *&> &tuple)
{
    uint8_t drawfunc = get<uint8_t>(tuple);
    ImageDrawfuncParam *&param = get<ImageDrawfuncParam *&>(tuple);
    if (drawfunc == Image::HpBar)
        archive((Unit *&)param);
    else if (drawfunc == Image::Remap)
    {
        uint8_t remap_id;
        archive(remap_id);
        if (remap_id >= Limits::RemapPalettes)
            throw SaveFail(SaveFail::ImageRemap);
        (void *&)param = bw::blend_palettes[remap_id].data;
    }
    else
        archive((uintptr_t &)param);
}

void load(FixupArchive &archive, tuple<uint8_t, ImageDrawfuncParam *&> &tuple)
{
    uint8_t drawfunc = get<uint8_t>(tuple);
    ImageDrawfuncParam *&param = get<ImageDrawfuncParam *&>(tuple);
    if (drawfunc == Image::HpBar)
        archive((Unit *&)param);
}

template <class Archive>
void BulletSystem::serialize(Archive &archive)
{
    archive(initstate, moving_to_point, moving_to_unit, bouncing, damage_ground, moving_near, dying);
    archive(*bw::first_active_bullet, *bw::last_active_bullet);
}

template <class Archive>
void Bullet::serialize(Archive &archive)
{
    // No hitpoints, unused52 or cooldowns
    archive(list, move_target, move_target_unit, next_move_waypoint, unk_move_waypoint);
    archive(flingy_flags, facing_direction, flingyTurnRadius, movement_direction, flingy_id, _unknown_0x026);
    archive(flingyMovementType, position, exact_position, flingyTopSpeed, current_speed, next_speed);
    archive(speed, acceleration, pathing_direction, unk4b, player, order, order_state, order_signal);
    archive(order_fow_unit, order_timer, order_target_pos, target, weapon_id, time_remaining, flags);
    archive(bounces_remaining, parent, previous_target, spread_seed, targeting, spawned, sprite);
}

template <class Archive>
void LoneSpriteSystem::serialize(Archive &archive)
{
    archive(lone_sprites, fow_sprites);
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
    ReadCompressed(file, &bw::player_ai[player], sizeof(Ai::PlayerData));
    ConvertPlayerAiData<false>(&bw::player_ai[player], player);
}

void Load::LoadAiChunk()
{
    uint32_t region_count;
    fread(&region_count, 4, 1, file);
    DeleteAiRegions();
    AllocateAiRegions(region_count);
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
    ReadCompressed(file, bw::resource_areas.raw_pointer(), 0x2ee8);
}

void Load::LoadPathingChunk()
{
    using namespace Pathing;
    uint32_t chunk_size;
    fread(&chunk_size, 4, 1, file);
    std::unique_ptr<uint8_t[]> chunk(new uint8_t[chunk_size]);
    uint8_t *pos = chunk.get();
    ReadCompressed(file, chunk.get(), chunk_size);

    PathingSystem *pathing = *bw::pathing = (PathingSystem *)SMemAlloc(sizeof(PathingSystem), "LoadPathingChunk", 42, 0);
    memcpy(pathing, pos, sizeof(PathingSystem));
    ConvertPathing<false>(pathing);
    pos += sizeof(PathingSystem);

    ContourData *contours = pathing->contours = (ContourData *)SMemAlloc(sizeof(ContourData), "LoadPathingChunk", 42, 0);
    memcpy(pathing->contours, pos, sizeof(ContourData));
    pos += sizeof(ContourData);

    contours->top_contours = (Contour *)SMemAlloc(sizeof(Contour) * contours->top_contour_count, "LoadPathingChunk", 42, 0);
    memcpy(contours->top_contours, pos, contours->top_contour_count * sizeof(Contour));
    pos += contours->top_contour_count * sizeof(Contour);

    contours->right_contours = (Contour *)SMemAlloc(sizeof(Contour) * contours->right_contour_count, "LoadPathingChunk", 42, 0);
    memcpy(contours->right_contours, pos, contours->right_contour_count * sizeof(Contour));
    pos += contours->right_contour_count * sizeof(Contour);

    contours->bottom_contours = (Contour *)SMemAlloc(sizeof(Contour) * contours->bottom_contour_count, "LoadPathingChunk", 42, 0);
    memcpy(contours->bottom_contours, pos, contours->bottom_contour_count * sizeof(Contour));
    pos += contours->bottom_contour_count * sizeof(Contour);

    contours->left_contours = (Contour *)SMemAlloc(sizeof(Contour) * contours->left_contour_count, "LoadPathingChunk", 42, 0);
    memcpy(contours->left_contours, pos, contours->left_contour_count * sizeof(Contour));
}

void Load::LoadGame()
{
    // There's the cursor marker already, however it will not be pointed by the global cursor_marker...
    for (int i = 0; i < *bw::map_height_tiles; i++)
    {
        bw::horizontal_sprite_lines[i] = nullptr;
        bw::horizontal_sprite_lines_rev[i] = nullptr;
    }
    lone_sprites->lone_sprites.clear();

    try {
        cereal_archive(version);
    } catch (const std::exception &e) {
        std::throw_with_nested(SaveFail(SaveFail::Version));
    }
    if (version > SaveVersion)
        throw SaveFail(SaveFail::Version);

    cereal_archive(*lone_sprites, *bullet_system);

    FixupArchive::SpriteFixup id_to_sprite;
    FixupArchive::BulletFixup id_to_bullet;
    id_to_sprite.reserve(lone_sprites->lone_sprites.size());
    lone_sprites->MakeSaveIdMapping([&] (Sprite *sprite, uintptr_t id) {
        id_to_sprite[id] = sprite;
    });
    id_to_bullet.reserve(bullet_system->BulletCount());
    bullet_system->MakeSaveIdMapping([&] (Bullet *bullet, uintptr_t id) {
        id_to_bullet[id] = bullet;
    });
    LoadUnits();
    GlobalUnitPointers global_unit_ptrs;
    cereal_archive(global_unit_ptrs);

    cereal_archive.ConfirmEmptyBuffer();
    FixupArchive fixup(version, id_to_sprite, id_to_bullet);
    fixup(*lone_sprites, *bullet_system);
    for (auto &sprite : lone_sprites->lone_sprites)
        sprite->AddToHlines();
    for (auto &sprite : lone_sprites->fow_sprites)
        sprite->AddToHlines();
    for (auto &bullet : bullet_system->ActiveBullets())
        bullet->sprite->AddToHlines();
    FixupUnits(fixup);
    fixup(global_unit_ptrs);

    uint32_t original_tile_length;
    fread(&original_tile_length, 1, 4, file);
    ReadCompressed(file, *bw::original_tiles, original_tile_length);
    ReadCompressed(file, *bw::creep_tile_borders, original_tile_length / 2);
    if (!LoadDisappearingCreepChunk((File *)file))
        throw SaveException();
    ReadCompressed(file, *bw::map_tile_ids, Limits::MapHeight_Tiles * Limits::MapWidth_Tiles * 2);
    ReadCompressed(file, *bw::megatiles, Limits::MapHeight_Tiles * Limits::MapWidth_Tiles * 2);
    ReadCompressed(file, *bw::map_tile_flags, Limits::MapHeight_Tiles * Limits::MapWidth_Tiles * 4);

    if (!LoadTriggerChunk((File *)file))
        throw SaveException();
    fread(bw::scenario_chk_STR_size.raw_pointer(), 1, 4, file);
    SMemFree(*bw::scenario_chk_STR, "notasourcefile", 42, 0);
    *bw::scenario_chk_STR = SMemAlloc(*bw::scenario_chk_STR_size, "notasourcefile", 42, 0);
    ReadCompressed(file, *bw::scenario_chk_STR, *bw::scenario_chk_STR_size);

    LoadPathingChunk();
    unit_search->Init();

    LoadAiChunk();
    if (!LoadDatChunk((File *)file, 0x3))
        throw SaveException();
    fread(bw::screen_x.raw_pointer(), 1, 4, file);
    fread(bw::screen_y.raw_pointer(), 1, 4, file);

    RestorePylons();
    AddSelectionOverlays();
    if (!IsMultiplayer())
        *bw::unk_51CA1C = *bw::unk_57F1E3;

    MoveScreen(*bw::screen_x, *bw::screen_y);
    InitCursorMarker();
}

int LoadGameObjects()
{
    Load load((FILE *)*bw::loaded_save);
    bool success = true;
    try
    {
        load.LoadGame();
        *bw::load_succeeded = 1;
    }
    catch (const SaveException &e)
    {
        error_log->Log("Load failed: %s\n", e.cause().c_str());
        *bw::load_succeeded = 0;
        *bw::replay_data = nullptr; // Otherwise it would save empty lastreplay
        success = false;
    }
    catch (const std::exception &e)
    {
        error_log->Log("Load failed: %s\n", e.what());
        PrintNestedExceptions(e);
        *bw::load_succeeded = 0;
        *bw::replay_data = nullptr;
        success = false;
    }
    load.Close();
    *bw::loaded_save = nullptr;
    return success;
}
