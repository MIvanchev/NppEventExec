/*
This file is part of NppEventExec
Copyright (C) 2016-2017 Mihail Ivanchev

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program. If not, see <http://www.gnu.org/licenses/>.
*/
#include "base.h"
#include "event_map.h"
#include "csv.h"
#include "mem.h"
#include "util.h"
#include "utf8.h"
#include "plugin.h"

/** TODO */
#define BYTE_BUF_SIZE_READ 1024

/** TODO */
#define BYTE_BUF_SIZE_WRITE 1024

/** TODO */
#define STRING_ALLOC_STEP 512

/** TODO */
#define MAX_BOOL_CHARS 5

/** TODO */
#define MAX_EVENT_CHARS 32

#define CHR_LF    0x0A
#define CHR_CR    0x0D
#define CHR_QUOTE 0x22
#define CHR_COMMA 0x2C

typedef enum
{
    ST_INITIAL,
    ST_QUOTED,
    ST_UNQUOTED,
    ST_QUOTE,
    ST_QUOTED_EOL,
    ST_EOL
} ParserState;

static int readHeader(void);
static int readValue(wchar_t *buf, size_t maxLen);
static int readChar(wchar_t *chr);
static int nextChar(wchar_t *chr);
static int readBytes(void);
static int writeValue(const wchar_t *str, bool escape);
static int writeByte(unsigned char byte);
static int writeBytes(void);

static HANDLE file;
static unsigned char *byteBuf;
static unsigned char *byteBufPtr;
static size_t byteBufLen;
static size_t byteBufSize;
static size_t fieldCnt;
static size_t remFieldCnt;
static ParserState state;

int csvOpen(wchar_t *path, size_t _fieldCnt, int header)
{
    assert(path);
    assert(_fieldCnt);
    assert(BYTE_BUF_SIZE_READ <= SIZE_MAX);

    file = CreateFileW(path,
                       GENERIC_READ,
                       FILE_SHARE_READ, /* Other procs can read too */
                       NULL,
                       OPEN_EXISTING,
                       FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN,
                       NULL);

    if (file == INVALID_HANDLE_VALUE)
    {
        /* TODO error */
        goto fail_file;
    }

    byteBufSize = BYTE_BUF_SIZE_READ;

    if (!(byteBuf = allocMem(byteBufSize)))
    {
        /* TODO error */
        goto fail_alloc;
    }

    fieldCnt = _fieldCnt;
    remFieldCnt = fieldCnt;
    state = ST_INITIAL;

    if (readBytes())
    {
        /* TODO error */
        goto fail_bytes;
    }
    if (header && readHeader())
    {
        /* TODO error */
        goto fail_header;
    }

    return 0;

fail_header:
fail_bytes:
    freeMem(byteBuf);
fail_alloc:
    CloseHandle(file);
fail_file:
    return 1;
}

int csvCreate(wchar_t *path, size_t _fieldCnt)
{
    assert(path);
    assert(_fieldCnt);
    assert(BYTE_BUF_SIZE_WRITE <= SIZE_MAX);

    file = CreateFileW(path,
                       GENERIC_WRITE,
                       0, /* Other procs cannot open the file */
                       NULL,
                       CREATE_ALWAYS,
                       FILE_ATTRIBUTE_NORMAL,
                       NULL);

    if (file == INVALID_HANDLE_VALUE)
    {
        /* TODO error */
        goto fail_file;
    }

    byteBufSize = BYTE_BUF_SIZE_WRITE;

    if (!(byteBuf = allocMem(byteBufSize)))
    {
        /* TODO error */
        goto fail_alloc;
    }

    byteBufLen = byteBufSize;
    byteBufPtr = byteBuf;
    fieldCnt = _fieldCnt;
    remFieldCnt = fieldCnt;

    return 0;

fail_alloc:
    freeMem(byteBuf);
fail_file:
    return 1;
}

int csvFlush(void)
{
    if (byteBufLen < byteBufSize && writeBytes())
    {
        /* TODO error */
        return 1;
    }

    return 0;
}

void csvClose(void)
{
    freeMem(byteBuf);
    CloseHandle(file);
}

int csvHasData(void)
{
    return byteBufLen;
}

wchar_t* csvReadString(size_t *unitCnt, size_t *charCnt)
{
    wchar_t chr[2];
    wchar_t *buf;
    wchar_t *tmp;
    size_t bufLen;
    size_t bufPos;
    size_t strLen;
    int res;

    assert(unitCnt);
    assert(charCnt);
    assert(BUFLEN(chr) == 2);
    assert(STRING_ALLOC_STEP >= 2);
    assert(STRING_ALLOC_STEP <= SIZE_MAX);

    bufLen = STRING_ALLOC_STEP;

    if (!(buf = allocStr(bufLen)))
    {
        /* TODO error */
        goto fail_alloc;
    }

    bufPos = 0;
    strLen = 0;

    while ((res = readChar(chr)) > 0)
    {
        if (bufPos > bufLen - res)
        {
            if (bufLen <= SIZE_MAX - STRING_ALLOC_STEP)
                bufLen += STRING_ALLOC_STEP;
            else if (bufLen != SIZE_MAX)
                bufLen = SIZE_MAX;
            else
            {
                /* TODO error */
                goto fail_too_long;
            }

            if (!(tmp = reallocStr(buf, bufLen)))
            {
                /* TODO error */
                goto fail_realloc;
            }

            buf = tmp;
        }

        if (res == 1)
            buf[bufPos++] = *chr;
        else
        {
            buf[bufPos++] = chr[0];
            buf[bufPos++] = chr[1];
        }

        strLen++;
    }

    if (res < 0)
    {
        /* TODO error */
        goto fail_char;
    }

    if (bufPos < bufLen - 1)
    {
        if (!(tmp = reallocStr(buf, bufPos + 1)))
        {
            /* TODO warning */
        }
        else
            buf = tmp;
    }
    else if (bufPos == bufLen)
    {
        if (bufPos == SIZE_MAX)
        {
            /* TODO error */
            goto fail_too_long;
        }
        if (!(tmp = reallocStr(buf, bufPos + 1)))
        {
            /* TODO error */
            goto fail_realloc;
        }

        buf = tmp;
    }

    buf[bufPos] = L'\0';
    *unitCnt = bufPos;
    *charCnt = strLen;

    return buf;

fail_realloc:
fail_too_long:
fail_char:
    freeStr(buf);
fail_alloc:
    return NULL;
}

int csvReadBool(void)
{
    wchar_t buf[BUF_LEN_FOR_CHAR_COUNT(MAX_BOOL_CHARS)];

    if (readValue(buf, BUFLEN(buf)))
    {
        /* TODO error */
        return 1;
    }
    if (!_wcsicmp(buf, L"0")
        || !_wcsicmp(buf, L"off")
        || !_wcsicmp(buf, L"no")
        || !_wcsicmp(buf, L"false"))
    {
        return 0;
    }
    if (!_wcsicmp(buf, L"1")
        || !_wcsicmp(buf, L"on")
        || !_wcsicmp(buf, L"yes")
        || !_wcsicmp(buf, L"true"))
    {
        return 1;
    }

    /* TODO error */
    return -1;
}

int csvReadEvent(unsigned int *event)
{
    wchar_t val[BUF_LEN_FOR_CHAR_COUNT(MAX_EVENT_CHARS)];
    size_t ii;

    if (readValue(val, BUFLEN(val)))
    {
        /* TODO error */
        return 1;
    }

    /* TODO error */

    for (ii = 0; ii < eventMapSize; ii++)
    {
        if (!_wcsicmp(val, eventMap[ii].name))
        {
            *event = eventMap[ii].event;
            return 0;
        }
    }

    /* TODO error */
    return 1;
}

int readHeader(void)
{
    wchar_t chr[2];
    int res;
    unsigned int ii;

    assert(BUFLEN(chr) == 2);

    for (ii = 0; ii < fieldCnt; ii++)
    {
        while ((res = readChar(chr)) > 0)
            ;

        if (res < 0)
        {
            /* TODO error */
            return 1;
        }
    }

    return 0;
}

int csvWriteString(const wchar_t *str)
{
    return writeValue(str, 1);
}

int csvWriteBool(int val, BoolOutputMode mode)
{
    wchar_t *str;

    switch (mode)
    {
    case BOOL_TRUE_FALSE:
        str = BOOL_TO_STR_TRUE_FALSE(val);
        break;
    case BOOL_YES_NO:
        str = BOOL_TO_STR_YES_NO(val);
        break;
    case BOOL_ON_OFF:
        str = BOOL_TO_STR_ON_OFF(val);
        break;
    case BOOL_ONE_ZERO:
        str = BOOL_TO_STR_ONE_ZERO(val);
        break;
    default:
        /* TODO error */
        return 1;
    }

    return writeValue(str, 0);
}

int csvWriteEvent(unsigned int event)
{
    return writeValue(getEventMapEntry(event)->name, 0);
}

int readValue(wchar_t *buf, size_t bufLen)
{
    wchar_t chr[2];
    wchar_t *ptr;
    int step;
    int res;

    assert(buf);
    assert(bufLen);
    assert(BUFLEN(chr) == 2);

    ptr = buf;
    step = 0;

    while ((res = readChar(chr)) > 0)
    {
        switch (step)
        {
        case 0:
            if (IS_SPACE(*chr))
                continue;

            step++;
            break;

        case 1:
            if (!IS_SPACE(*chr))
                break;

            step++;
            continue;

        case 2:
            if (!IS_SPACE(*chr))
            {
                /* TODO error */
                return 1;
            }

            continue;
        }

        if (IS_HIGH_SURROGATE(*chr))
        {
            if (bufLen < 3)
            {
                /* TODO error */
                return 1;
            }

            *ptr++ = chr[0];
            *ptr++ = chr[1];
            bufLen -= 2;
        }
        else
        {
            if (bufLen < 2)
            {
                /* TODO error */
                return 1;
            }

            *ptr++ = *chr;
            bufLen--;
        }
    }

    if (res < 0)
    {
        /* TODO error */
        return 1;
    }

    *ptr = L'\0';
    return 0;
}

int readChar(wchar_t *chr)
{
    assert(chr);

    while (byteBufLen)
    {
        if (nextChar(chr))
        {
            /* TODO error */
            goto fail_char;
        }

        switch (state)
        {
        case ST_INITIAL:

            remFieldCnt--;

            if (*chr == L'"')
            {
                state = ST_QUOTED;
                continue;
            }

            state = ST_UNQUOTED;

        /* Fall through to ST_UNQUOTED directly to process the same char, no
        ** need to waste an iteration.
        */

        case ST_UNQUOTED:
            switch (*chr)
            {
            case L',':
                if (!remFieldCnt)
                {
                    /* TODO error */
                    goto fail_syntax;
                }

                goto next_field;
            case L'\r':
                state = ST_EOL;
                continue;
            case L'\n':
                /* TODO error */
                goto fail_syntax;
            case L'"':
                /* TODO error */
                goto fail_syntax;
            }

            break;

        case ST_QUOTED:
            switch (*chr)
            {
            case L'\r':
                state = ST_QUOTED_EOL;
                break;
            case L'\n':
                /* TODO error */
                goto fail_syntax;
            case L'"':
                state = ST_QUOTE;
                continue;
            }

            break;

        case ST_EOL:
            if (*chr != L'\n')
            {
                /* TODO error */
                goto fail_syntax;
            }
            if (remFieldCnt)
            {
                /* TODO error */
                goto fail_syntax;
            }

            goto next_record;

        case ST_QUOTED_EOL:
            if (*chr != L'\n')
            {
                /* TODO error */
                goto fail_syntax;
            }

            state = ST_QUOTED;
            break;

        case ST_QUOTE:
            switch (*chr)
            {
            case L',':
                if (!remFieldCnt)
                {
                    /* TODO error */
                    goto fail_syntax;
                }

                goto next_field;
            case L'\r':
                state = ST_EOL;
                continue;
            case L'\n':
                /* TODO error */
                goto fail_syntax;
            case L'"':
                state = ST_QUOTED;
                break;
            }

            /* TODO error */
            goto fail_syntax;
        }

        return IS_HIGH_SURROGATE(*chr) ? 2 : 1;
    }

    switch (state)
    {
    case ST_QUOTED:
        /* TODO error */
        goto fail_syntax;
    case ST_QUOTED_EOL:
        /* TODO error */
        goto fail_syntax;
    case ST_EOL:
        /* TODO error */
        goto fail_syntax;
    case ST_INITIAL:
    case ST_UNQUOTED:
    case ST_QUOTE:
        if (remFieldCnt)
        {
            /* TODO error */
            goto fail_syntax;
        }
    }

    return 0;

next_record:
    remFieldCnt = fieldCnt;
next_field:
    state = ST_INITIAL;
    return 0;

fail_syntax:
fail_char:
    return -1;
}

int nextChar(wchar_t *chr)
{
    unsigned char unit;
    unsigned char seqLen;
    unsigned long code;

    assert(chr);

    unit = *byteBufPtr++;
    byteBufLen--;

    if((unit & 0xC0) == 0x80 || (unit & 0xF8) == 0xF8)
    {
        /* TODO error */
        return 1;
    }
    if (UTF8_CTRL_CODE[unit])
    {
        /* TODO error */
        return 1;
    }
    if (!(unit & 0x80))
    {
        *chr = unit;

        if (!byteBufLen && readBytes())
        {
            /* TODO error */
            return 1;
        }

        return 0;
    }

    seqLen = UTF8_SEQ_LEN[unit];
    code = unit & ((1 << (6 - seqLen)) - 1);

    if (seqLen < byteBufLen)
    {
        switch (seqLen)
        {
        case 3:
            code = (code << 6) | (*byteBufPtr++ & 0x3F);
        case 2:
            code = (code << 6) | (*byteBufPtr++ & 0x3F);
        case 1:
            code = (code << 6) | (*byteBufPtr++ & 0x3F);
        }

        byteBufLen -= seqLen;
    }
    else
    {
        switch (byteBufLen)
        {
        case 3:
            code = (code << 6) | (*byteBufPtr++ & 0x3F);
        case 2:
            code = (code << 6) | (*byteBufPtr++ & 0x3F);
        case 1:
            code = (code << 6) | (*byteBufPtr++ & 0x3F);
        }

        seqLen -= (unsigned char) byteBufLen; /* We know byteLen <= seqLen */
        byteBufLen = 0;

        if (readBytes())
        {
            /* TODO error */
            return 1;
        }
        if (byteBufLen < seqLen)
        {
            /* TODO error */
            return 1;
        }

        switch (seqLen)
        {
        case 3:
            code = (code << 6) | (*byteBufPtr++ & 0x3F);
        case 2:
            code = (code << 6) | (*byteBufPtr++ & 0x3F);
        case 1:
            code = (code << 6) | (*byteBufPtr++ & 0x3F);
        }

        byteBufLen -= seqLen;
    }

    if ((code & 0xF800) == 0xD800)
    {
        /* TODO error */
        return 1;
    }

    if (code < 0x10000)
    {
        *chr = (wchar_t) code;
    }
    else
    {
        code -= 0x10000;
        chr[0] = (wchar_t) (0xD800 + (code >> 10));
        chr[1] = (wchar_t) (0xDC00 + (code & 0x3FF));
    }

    return 0;
}

int readBytes(void)
{
    DWORD byteCnt;

    assert(byteBufSize <= SIZE_MAX);
    assert(byteBufSize < (DWORD) -1);

    if (!ReadFile(file, byteBuf, byteBufSize, &byteCnt, NULL))
    {
        /* TODO error */
        return 1;
    }

    byteBufPtr = byteBuf;
    byteBufLen = byteCnt;

    return 0;
}

int writeValue(const wchar_t *str, bool escape)
{
    unsigned long code;
    unsigned char unitCnt;

    assert(str);

    if (escape && writeByte(CHR_QUOTE))
    {
        /* TODO error */
        return 1;
    }

    while (*str)
    {
        if (*str == L'"')
        {
            if (!escape)
            {
                /* TODO error */
                return 1;
            }
            if (writeByte(CHR_QUOTE))
            {
                /* TODO error */
                return 1;
            }

            code = *str++;
            unitCnt = 1;
        }
        else if (*str <= 0xFF)
        {
            if (UTF8_CTRL_CODE[*str])
            {
                /* TODO error */
                return 1;
            }

            code = *str++;
            unitCnt = 1;
        }
        else if (IS_HIGH_SURROGATE(*str))
        {
            code = (*str++ - 0xD800) << 10;
            code |= *str++ - 0xDC00;
            unitCnt = 4;
        }
        else
        {
            code = *str++;
            unitCnt = 1 + (code > 0x007F) + (code > 0x07FF);
        }

#define WRITE_LEADING_FAILED(len)                              \
    writeByte((unsigned char) ((0xFF ^ ((1 << (9 - len)) - 1)) \
                               + (code >> (6 * (len - 1)))))

#define WRITE_CONT_FAILED(pos) \
    writeByte(0x80 + ((code >> (6 * (pos - 1))) & 0x3F))

        switch (unitCnt)
        {
        case 4:
            if (WRITE_LEADING_FAILED(4)
                || WRITE_CONT_FAILED(3)
                || WRITE_CONT_FAILED(2)
                || WRITE_CONT_FAILED(1))
            {
                /* TODO error */
                return 1;
            }

            break;
        case 3:
            if (WRITE_LEADING_FAILED(3)
                || WRITE_CONT_FAILED(2)
                || WRITE_CONT_FAILED(1))
            {
                /* TODO error */
                return 1;
            }

            break;
        case 2:
            if (WRITE_LEADING_FAILED(2) || WRITE_CONT_FAILED(1))
            {
                /* TODO error */
                return 1;
            }

            break;
        case 1:
            if (writeByte((unsigned char) code))
            {
                /* TODO error */
                return 1;
            }

            break;
        }
    }

    if (remFieldCnt > 1)
    {
        if ((escape && writeByte(CHR_QUOTE))
            || writeByte(CHR_COMMA))
        {
            /* TODO error */
            return 1;
        }

        remFieldCnt--;
    }
    else
    {
        if ((escape && writeByte(CHR_QUOTE))
            || writeByte(CHR_CR)
            || writeByte(CHR_LF))
        {
            /* TODO error */
            return 1;
        }

        remFieldCnt = fieldCnt;
    }

    return 0;
}

int writeByte(unsigned char byte)
{
    if (!byteBufLen && writeBytes())
    {
        /* TODO error */
        return 1;
    }

    *byteBufPtr++ = byte;
    byteBufLen--;

    return 0;
}

int writeBytes(void)
{
    DWORD written;

    assert(byteBufSize <= SIZE_MAX);
    assert(byteBufSize < (DWORD) -1);
    assert(byteBufLen < byteBufSize);

    if (!WriteFile(file, byteBuf, byteBufSize - byteBufLen, &written, NULL))
    {
        /* TODO error */
        return 1;
    }

    byteBufLen = written;
    byteBufPtr = byteBuf;

    return 0;
}
