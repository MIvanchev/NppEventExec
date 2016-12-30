/*
This file is part of NppEventExec
Copyright (C) 2016 Mihail Ivanchev

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
#define BYTE_BUF_SIZE 1024

/** TODO */
#define STRING_ALLOC_STEP 512

/** TODO */
#define CHAR_BUF_SIZE 256

/** TODO */
#define MAX_BOOL_CHARS 5

/** TODO */
#define MAX_EVENT_CHARS 32

#define IS_QUOTE(c) (*c == L'"')
#define IS_LF(c)    (*c == L'\n')

typedef enum
{
    INITIAL,
    QUOTED,
    UNQUOTED,
    QUOTE,
    QUOTED_EOL,
    EOL
} ParserState;

static bool readHeader(void);
static bool nextChar(wchar_t *chr);
static bool readBytes(void);

static HANDLE file;
static unsigned char byteBuf[BYTE_BUF_SIZE];
static unsigned char *byteBufPtr;
static size_t byteBufLen;
static bool eof;
static size_t numFields;
static size_t remFields;
static ParserState state;

int csvOpen(wchar_t *path, int header, size_t _numFields)
{
    file = CreateFileW(path,
                       GENERIC_READ,
                       FILE_SHARE_READ,
                       NULL,
                       OPEN_EXISTING,
                       FILE_FLAG_SEQUENTIAL_SCAN,
                       NULL);

    if (file == INVALID_HANDLE_VALUE)
    {
        /* TODO error */
        goto fail_file;
    }

    eof = false;
    numFields = _numFields;
    remFields = numFields;
    state = INITIAL;

    if (header && !readHeader())
        goto fail_header;

    return 0;

fail_header:
    csvClose();
fail_file:
    return 1;
}

void csvClose(void)
{
    CloseHandle(file);
}

int csvHasChars(void)
{
    if (byteBufLen)
        return 1;
    else if (!eof && !readBytes())
    {
        /* TODO error */
        return -1;
    }

    return !eof;
}

int csvGetChars(wchar_t *buf, size_t bufLen, size_t *numUnits, size_t *numChars)
{
    wchar_t *chr;
    size_t lenUnits;
    size_t lenChars;
    int res;

    chr = buf;
    lenUnits = 0;
    lenChars = 0;

    for(;; )
    {
        if (!byteBufLen)
        {
            if (!eof && !readBytes())
            {
                /* TODO error */
                return -1;
            }
            else if (eof)
            {
                switch (state)
                {
                case QUOTED:
                    /* TODO error */
                    return -1;
                case QUOTED_EOL:
                    /* TODO error */
                    return -1;
                case EOL:
                    /* TODO error */
                    return -1;
                default:
                    if (remFields)
                    {
                        /* TODO error */
                        return -1;
                    }

                    goto eof;
                }
            }
        }

        if (bufLen - lenUnits < 2)
        {
            res = 0;
            goto buf_full;
        }
        else if (!nextChar(chr))
        {
            /* TODO error */
            return -1;
        }

        switch (state)
        {
        case INITIAL:

            remFields--;

            if (IS_QUOTE(chr))
            {
                state = QUOTED;
                continue;
            }

            state = UNQUOTED;

        /* Fall through to UNQUOTED to process the first char */

        case UNQUOTED:
            switch (*chr)
            {
            case L',':
                if (!remFields)
                {
                    /* TODO error */
                    return -1;
                }

                goto next_field;
            case L'\r':
                state = EOL;
                continue;
            case L'\n':
                /* TODO error */
                return -1;
            case L'"':
                /* TODO error */
                return -1;
            }

            break;

        case QUOTED:
            switch (*chr)
            {
            case L'\r':
                state = QUOTED_EOL;
                break;
            case L'\n':
                /* TODO error */
                return -1;
            case L'"':
                state = QUOTE;
                continue;
            }

            break;

        case EOL:
            if (!IS_LF(chr))
            {
                /* TODO error */
                return -1;
            }
            else if (remFields)
            {
                /* TODO error */
                return -1;
            }

            goto next_record;

        case QUOTED_EOL:
            if (*chr != L'\n')
            {
                /* TODO error */
                return 1;
            }

            state = QUOTED;
            break;

        case QUOTE:
            switch (*chr)
            {
            case L',':
                if (!remFields)
                {
                    /* TODO error */
                    return 1;
                }

                goto next_field;
            case L'\r':
                state = EOL;
                continue;
            case L'\n':
                /* TODO error */
                return 1;
            case L'"':
                state = QUOTED;
                break;
            }

            break;

        }

        if (IS_HIGH_SURROGATE(*chr))
        {
            chr += 2;
            lenUnits += 2;
        }
        else
        {
            chr++;
            lenUnits++;
        }

        lenChars++;
    }

next_record:
    remFields = numFields;
next_field:
    state = INITIAL;
eof:
    res = 1;
buf_full:
    *numUnits = lenUnits;
    *numChars = lenChars;
    return res;
}

wchar_t* csvGetString(size_t minChars, size_t maxChars)
{
    wchar_t *buf;
    wchar_t *str;
    wchar_t *tmp;
    size_t bufLen;
    size_t bufPos;
    size_t chrLen;
    size_t numUnits;
    size_t numChars;
    int res;

    if (!(buf = allocStr(STRING_ALLOC_STEP)))
    {
        /* TODO error */
        goto fail_alloc;
    }

    bufLen = STRING_ALLOC_STEP;
    bufPos = 0;
    chrLen = 0;
    str = buf;

    for (;; )
    {
        if ((res = csvGetChars(str,
                               bufLen - bufPos - 1,
                               &numUnits,
                               &numChars)) == -1)
        {
            /* TODO error */
            goto fail_chars;
        }

        chrLen += numChars;
        if (chrLen > maxChars)
        {
            /* TODO error */
            goto fail_max_chars;
        }

        str += numUnits;
        bufPos += numUnits;

        if (res)
        {
            break;
        }
        else if (bufPos >= bufLen - 2)
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
            str = tmp + bufPos;
        }
    }

    if (chrLen < minChars)
    {
        /* TODO error */
        goto fail_min_chars;
    }

    *str = L'\0';

    if (!(tmp = reallocStr(buf, bufPos + 1)))
    {
        /* TODO warning */
        return buf;
    }

    return tmp;

fail_min_chars:
fail_realloc:
fail_too_long:
fail_max_chars:
fail_chars:
    freeStr(buf);
fail_alloc:
    return NULL;
}

int csvGetValue(wchar_t *buf, size_t maxLen)
{
    wchar_t *ptr;
    wchar_t chrBuf[CHAR_BUF_SIZE];
    wchar_t *chrPtr;
    size_t numChars;
    size_t numUnits;
    int step;
    int res;

    ptr = buf;
    step = 0;

    do
    {
        if ((res = csvGetChars(chrBuf,
                               BUFLEN(chrBuf),
                               &numUnits,
                               &numChars)) == -1)
        {
            /* TODO error */
            return 1;
        }

        chrPtr = chrBuf;

        while (numChars--)
        {
            switch (step)
            {
            case 0:
                if (*chrPtr == L' ' || *chrPtr == L'\t')
                {
                    chrPtr++;
                    continue;
                }

                step++;
                break;

            case 1:
                if (*chrPtr != L' ' && *chrPtr != L'\t')
                    break;

                chrPtr++;
                step++;
                continue;

            case 2:
                if (*chrPtr != L' ' && *chrPtr != L'\t')
                {
                    /* TODO error */
                    return 1;
                }

                chrPtr++;
                continue;
            }

            if (!maxLen--)
            {
                /* TODO error */
                return 1;
            }
            else if (IS_HIGH_SURROGATE(*chrPtr))
            {
                *(ptr + 0) = *(chrPtr + 0);
                *(ptr + 1) = *(chrPtr + 1);
                chrPtr += 2;
                ptr += 2;
            }
            else
                *ptr++ = *chrPtr++;
        }
    }
    while (!res);

    *ptr = L'\0';

    return 0;
}

int csvGetBool(void)
{
    wchar_t val[BUF_LEN_FROM_MIN_CHAR_COUNT(MAX_BOOL_CHARS)];

    if (csvGetValue(val, BUF_LEN_TO_MIN_CHAR_COUNT(val)))
    {
        /* TODO error */
        return -1;
    }
    else if (!_wcsicmp(val, L"0")
             || !_wcsicmp(val, L"off")
             || !_wcsicmp(val, L"no")
             || !_wcsicmp(val, L"false"))
    {
        return 0;
    }
    else if (!_wcsicmp(val, L"1")
             || !_wcsicmp(val, L"on")
             || !_wcsicmp(val, L"yes")
             || !_wcsicmp(val, L"true"))
    {
        return 1;
    }

    /* TODO error */
    return -1;
}

int csvGetEvent(unsigned int *event)
{
    wchar_t val[BUF_LEN_FROM_MIN_CHAR_COUNT(MAX_EVENT_CHARS)];
    size_t ii;

    if (csvGetValue(val, BUF_LEN_TO_MIN_CHAR_COUNT(val)))
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

bool readHeader(void)
{
    wchar_t buf[512];
    size_t numUnits;
    size_t numChars;
    size_t ii;
    int res;

    for (ii = 0; ii < numFields; ii++)
    {
        do
        {
            if ((res = csvGetChars(buf,
                                   BUFLEN(buf),
                                   &numUnits,
                                   &numChars)) == -1)
            {
                /* TODO error */
                return false;
            }
        }
        while (!res);
    }

    return true;
}

bool nextChar(wchar_t *chr)
{
    unsigned char unit;
    unsigned char seqLen;
    unsigned long code;

    unit = *byteBufPtr++;
    byteBufLen--;

    if((unit & 0xC0) == 0x80 || (unit & 0xF8) == 0xF8)
    {
        /* TODO error */
        return false;
    }
    else if (UTF8_CTRL_CODE[unit])
    {
        /* TODO error */
        return false;
    }
    else if (!(unit & 0x80))
    {
        *chr = unit;
        return true;
    }

    seqLen = UTF8_SEQ_LEN[unit];
    code = unit & ((1 << (6 - seqLen)) - 1);

    if (seqLen <= byteBufLen)
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
        while (seqLen--)
        {
            if (!byteBufLen)
            {
                if (!readBytes())
                {
                    /* TODO error */
                    return false;
                }
                else if (eof)
                {
                    /* TODO error */
                    return false;
                }
            }

            code = (code << 6) | (*byteBufPtr++ & 0x3F);
            byteBufLen--;
        }
    }

    if ((code & 0xF800) == 0xD800)
    {
        /* TODO error */
        return false;
    }
    else if (code < 0x10000)
    {
        *chr = code;
    }
    else
    {
        code -= 0x10000;
        *(chr + 0) = 0xD800 + (code >> 10);
        *(chr + 1) = 0xDC00 + (code & 0x3FF);
    }

    return true;
}

bool readBytes(void)
{
    DWORD numBytes;

    if (!ReadFile(file, byteBuf, BYTE_BUF_SIZE, &numBytes, NULL))
    {
        /* TODO error */
        return false;
    }

    byteBufPtr = byteBuf;
    byteBufLen = numBytes;
    eof = !numBytes;

    return true;
}
