#include "datastream.h"

#include <malloc.h>
#include <string.h>

datastream::datastream(bool del_, int initSize)
{
    buf = (uint8_t *)malloc(initSize);
    size = 0;
    datalen = initSize;
    del = del_;
    //ctor
}

datastream::~datastream()
{
    if (del)
        free(buf);
    //dtor
}

void datastream::Clear()
{
    size = 0;
}

void datastream::Insert(const void *data_, int length, int pos)
{
    if (length + pos > datalen)
        ExpBuf(length + pos);
    if (length + pos > size)
        size = length + pos;

    memcpy(buf + pos, data_, length);
}

void datastream::Append(const void *data, int length)
{
    Insert(data, length, size);
}

void datastream::Seek(int amount)
{
    if (amount < 0 - size)
        amount = 0 - size;
    if (amount + size > datalen)
        ExpBuf(amount + size);
    size += amount;
}

void datastream::ExpBuf(int newSize)
{
    uint8_t *newbuf = (uint8_t *)realloc(buf, newSize);
    if (!newbuf)
    {
        free(buf);
        buf = 0;
    }
    buf = newbuf;
    datalen = newSize;
}

datastream& datastream::operator= (const datastream &paketti)
{
    if (&paketti != this)
    {
        free(buf);
        buf = (uint8_t *)malloc(paketti.Length());
        Insert(paketti.GetData(), paketti.Length(), 0);
        del = true; // TODO: Onko?
    }
    return *this;
}

datastream& datastream::operator<< (uint32_t data)
{
    if (size + 4 > datalen)
        ExpBuf(size + 4);
    Insert(&data, 4, size);
    return *this;
}

datastream& datastream::operator<< (uint16_t data)
{
    if (size + 2 > datalen)
        ExpBuf(size + 2);
    Insert(&data, 2, size);
    return *this;
}

datastream& datastream::operator<< (uint8_t data)
{
    if (size + 1 > datalen)
        ExpBuf(size + 1);
    Insert(&data, 1, size);
    return *this;
}

datastream& datastream::operator<< (float data)
{
    if (size + 4 > datalen)
        ExpBuf(size + 4);
    Insert(&data, 4, size);
    return *this;
}

datastream& datastream::operator<< (double data)
{
    if (size + 8 > datalen)
        ExpBuf(size + 8);
    Insert(&data, 8, size);
    return *this;
}

datastream& datastream::operator<< (const char *data)
{
    int len = strlen(data) + 1;
    while(size + len > datalen)
        ExpBuf(size + len);
    Insert(data, len, size);
    return *this;
}

