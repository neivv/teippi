#ifndef DATASTREAM_H
#define DATASTREAM_H
#include <stdint.h>

#define nofree false

class datastream
{
    public:
        datastream(bool del = true, int initSize = 0x100);
        virtual ~datastream();
        datastream& operator<< (uint32_t data);
        datastream& operator<< (uint16_t data);
        datastream& operator<< (uint8_t data);
        datastream& operator<< (float data);
        datastream& operator<< (double data);
        /*datastream& operator<< (int32_t &data) { return operator<<((uint32_t&)data); }
        datastream& operator<< (int16_t &data) { return operator<<((uint16_t&)data); }
        datastream& operator<< (int8_t &data) { return operator<<((uint8_t&)data); }*/

        datastream& operator<< (const char *data);
        datastream& operator= (const datastream &paketti);
        int Length() const { return size; }
        const uint8_t *GetData() const { return buf; }
        const uint8_t *GetEnd() const { return GetData() + Length(); }

        void Insert(const void *buf, int length, int pos);
        void Append(const void *buf, int len);
        void Seek(int amount);

        void Clear();

    protected:
    private:
        void ExpBuf(int newSize);
        unsigned char *buf;
        int size;
        int datalen;
        bool del;
};

#endif // DATASTREAM_H
