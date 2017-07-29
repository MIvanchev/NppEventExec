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
#include "csv_gen.h"
#include "mem.h"
#include "util.h"
#include <criterion/logging.h>
#include <time.h>

/** TODO */
#define FILENAME L"csv\\harness.csv"

/** TODO */
#define MAX_STR_LEN 1024

/** TODO */
#define MAX_WHITESPACE_LEN 48

/* TODO */
#define BYTE_BUF_SIZE 1024

/* TODO */
#define MAX_COLUMNS 32

/* TODO */
#define MAX_RECORDS 256

#define logError(...)         logError_(__LINE__, __VA_ARGS__)
#define logError_(line, ...)  logError__(line, __VA_ARGS__)
#define logError__(line, ...) cr_log_error(__FILE__ ":" # line ": " __VA_ARGS__)

static HANDLE file;
static unsigned char byteBuf[BYTE_BUF_SIZE];
static unsigned char *byteBufPtr;
static size_t byteBufSize;

/**
 * These are the Unicode character ranges used under a set of different
 * circumstances.
 */
static const unsigned long const (*CHAR_RANGES[2][2])[2] = {
    {
        /* 1 UTF-16 unit remaining, unquoted string. */
        (unsigned long[][2]) { { 0x09, 0x09 }, { 0x20, 0x21 }, { 0x23, 0x2B },
                               { 0x2D, 0x7E }, { 0x80, 0xD7FF }, {0} },

        /* 1 UTF-16 unit remaining, quoted string. */
        (unsigned long[][2]) { { 0x09, 0x09 }, { 0x20, 0x7E }, { 0x80, 0xD7FF },
                               {0} }
    },
    {
        /* At least 2 UTF-16 units remaining, unquoted string. */
        (unsigned long[][2]) { { 0x09, 0x09 }, { 0x20, 0x21 }, { 0x23, 0x2B },
                               { 0x2D, 0x7E }, { 0x80, 0xD7FF },
                               { 0x10000, 0x10FFFF }, {0} },

        /* At least 2 UTF-16 units remaining, quoted string. */
        (unsigned long[][2]) { { 0x09, 0x09 }, { 0x0D, 0x0D }, { 0x20, 0x7E },
                               { 0x80, 0xD7FF }, { 0x10000, 0x10FFFF }, {0} }
    },
};

static CsvData* genData(void);
static wchar_t* genStr(int quoted);
static void genChar(wchar_t *buf, size_t bufLen, int quoted);
static unsigned long genUlong(void);
static int writeHeader(size_t columnCnt);
static int writeStr(const wchar_t* str, bool quoted);
static int writeEvent(unsigned int val, bool quoted);
static int writeBool(int val, bool quoted);
static int writeVal(const wchar_t *val, bool quoted);
static int writeSeparator(void);
static int writeEol(void);
static int writeQuote(void);
static int writeWhitespace(bool quoted);
static int writeByte(unsigned char val);
static int flush(void);
static unsigned char* convToUtf8(const wchar_t *str);
static void logFileCreationError(const wchar_t *name);
static void logUtf8ConvError(const wchar_t *str);
static wchar_t* getSysError(DWORD code, wchar_t *buf, size_t bufLen);

CsvData* genCsv(void)
{
    wchar_t *name;
    CsvData *data;
    CsvColumn *col;
    CsvField *field;
    wchar_t *str;
    bool quoted;
    DWORD written;
    unsigned int ii;
    unsigned int jj;

    if (!(data = genData()))
    {
        logError("Failed to generate CSV data.");
        goto fail_data;
    }

    file = CreateFile(FILENAME,
                      GENERIC_WRITE,
                      FILE_SHARE_READ,
                      NULL,
                      CREATE_ALWAYS,
                      FILE_ATTRIBUTE_NORMAL,
                      NULL);

    if (file == INVALID_HANDLE_VALUE)
    {
        logFileCreationError(FILENAME);
        goto fail_file;
    }

    byteBufPtr = byteBuf;
    byteBufSize = BYTE_BUF_SIZE;

    if (data->hdr && writeHeader(data->columnCnt))
    {
        logError("Failed to write the column headers.");
        goto fail_hdr;
    }

    for (jj = 0; jj < data->recordCnt; jj++)
    {
        if ((jj || data->hdr) && writeEol())
        {
            logError("Failed to write an EOL before record #%u.", jj + 1);
            goto fail_eol;
        }

        for (ii = 0; ii < data->columnCnt; ii++)
        {
            field = &data->columns[ii].fields[jj];

            switch (data->columns[ii].type)
            {
            case CSV_STRING:
                if (writeStr(field->strVal, field->quoted))
                {
                    logError("Failed to write the string value \"%ls\" at "
                             "column #%u of record #%u.",
                             field->strVal,
                             ii + 1,
                             jj + 1);
                    goto fail_str;
                }
                break;
            case CSV_EVENT:
                if (writeEvent(field->eventVal, field->quoted))
                {
                    logError("Failed to write the event value %ls at "
                             "column #%u of record #%u.",
                             getEventMapEntry(field->eventVal)->name,
                             ii + 1,
                             jj + 1);
                    goto fail_event;
                }
                break;
            case CSV_BOOL:
                if (writeBool(field->boolVal, field->quoted))
                {
                    logError("Failed to write the boolean value %ls at "
                             "column #%u of record #%u.",
                             BOOL_TO_STR_TRUE_FALSE(field->boolVal),
                             ii + 1,
                             jj + 1);
                    goto fail_bool;
                }
                break;
            default:
                logError("Unknown column type %u.", data->columns[ii].type);
                goto fail_invalid_type;
            }

            if (ii < data->columnCnt - 1 && writeSeparator())
            {
                logError(
                    "Failed to write a separator after column #%u of record #%u.",
                    ii + 1,
                    jj + 1);
                goto fail_separator;
            }
        }
    }

    if ((data->hdr || data->recordCnt) && rand() % 2 && writeEol())
    {
        logError("Failed to write an EOL before EOF.");
        goto fail_eol;
    }

    if (byteBufSize < BYTE_BUF_SIZE && flush())
    {
        logError("Failed to flush the byte buffer after writing all of the CSV "
                 "data.");
        goto fail_flush;
    }

    CloseHandle(file);

    return data;

fail_flush:
fail_eol:
fail_separator:
fail_invalid_type:
fail_bool:
fail_event:
fail_str:
fail_hdr:
    CloseHandle(file);
fail_file:
    freeData(data);
fail_data:
    return NULL;
}

void freeData(CsvData *data)
{
    unsigned int ii;
    unsigned int jj;

    for (ii = 0; ii < data->columnCnt; ii++)
    {
        if (data->columns[ii].type == CSV_STRING)
        {
            for (jj = 0; jj < data->recordCnt; jj++)
                freeStr(data->columns[ii].fields[jj].strVal);
        }

        freeMem(data->columns[ii].fields);
    }

    freeMem(data);
}

CsvData* genData(void)
{
    CsvData *data;
    int columnCnt;
    CsvColumn *cols;
    CsvField *field;
    unsigned int ii;
    unsigned int jj;

    assert(MAX_COLUMNS <= UINT_MAX);
    assert(MAX_COLUMNS <= SIZE_MAX / sizeof(CsvColumn));
    assert(sizeof(CsvData) <= SIZE_MAX - sizeof(CsvColumn) * MAX_COLUMNS);
    assert(MAX_RECORDS < UINT_MAX);
    assert(MAX_RECORDS <= SIZE_MAX / sizeof(CsvField));

    columnCnt = 1 + rand() % MAX_COLUMNS;

    if (!(data = allocMem(sizeof(CsvData) + sizeof(CsvColumn) * columnCnt)))
    {
        logError("Failed to allocate memory.");
        goto fail_alloc;
    }

    data->columnCnt = columnCnt;
    data->recordCnt = rand() % (MAX_RECORDS + 1);
    data->hdr = rand() % 2;

    cols = data->columns;

    for (ii = 0; ii < columnCnt; ii++)
    {
        cols[ii].type = rand() % CSV_TYPE_CNT;

        if (!(cols[ii].fields = allocMem(sizeof(CsvField) * data->recordCnt)))
        {
            logError("Failed to allocate memory.");
            goto fail_alloc_fields;
        }

        for (jj = 0; jj < data->recordCnt; jj++)
        {
            field = &cols[ii].fields[jj];
            field->quoted = rand() % 2;

            switch (cols[ii].type)
            {
            case CSV_STRING:
                if (!(field->strVal = genStr(field->quoted)))
                {
                    logError("Failed to generate a string value for column %u "
                             "of record %u.",
                             ii + 1,
                             jj + 1);
                    goto fail_gen_str;
                }
                break;
            case CSV_EVENT:
                field->eventVal = eventMap[rand() % eventMapSize].event;
                break;
            case CSV_BOOL:
                field->boolVal = rand() % 2;
                break;
            default:
                logError("Invalid column type %d specified while generating "
                         "random data.",
                         cols[ii].type);
                goto fail_invalid_type;
            }
        }
    }

    return data;

fail_gen_str:
    for (; jj > 1; jj--)
        freeStr(cols[ii].fields[jj - 1].strVal);

fail_invalid_type:
    freeMem(cols[ii].fields);

fail_alloc_fields:
    for (; ii > 1; ii--)
    {
        if (cols[ii - 1].type == CSV_STRING)
        {
            for (jj = 0; jj < data->recordCnt; jj++)
                freeStr(cols[ii - 1].fields[jj].strVal);
        }

        freeMem(cols[ii - 1].fields);
    }

    freeMem(data);
fail_alloc:

    return NULL;
}

wchar_t* genStr(int quoted)
{
    wchar_t *str;
    wchar_t *ptr;
    unsigned int len;

    assert(MAX_STR_LEN < UINT_MAX);
    assert(MAX_STR_LEN < SIZE_MAX);

    len = rand() % (MAX_STR_LEN + 1);

    if (!(str = allocStr(len + 1)))
    {
        logError("Failed to allocate memory.");
        return NULL;
    }

    ptr = str;

    while (len)
    {
        genChar(ptr, len, quoted);

        if (IS_HIGH_SURROGATE(*ptr))
        {
            ptr += 2;
            len -= 2;
        }
        else if (*ptr == L'\r')
        {
            *(ptr + 1) = L'\n';
            ptr += 2;
            len -= 2;
        }
        else
        {
            ptr++;
            len--;
        }
    }

    *ptr = L'\0';

    return str;
}

void genChar(wchar_t *buf, size_t bufLen, int quoted)
{
    const unsigned long const (*ranges)[2];
    unsigned long rangeLen;
    size_t chrCnt;
    unsigned long chrPos;
    unsigned long chrCode;
    unsigned long offset;
    size_t ii;

    ranges = CHAR_RANGES[bufLen > 1][quoted];

    chrCnt = 0;

    for (ii = 0; ranges[ii][0]; ii++)
        chrCnt += ranges[ii][1] - ranges[ii][0] + 1;

    chrPos = genUlong() %  chrCnt;
    offset = 0;

    for (ii = 0; ranges[ii][0]; ii++)
    {
        rangeLen = ranges[ii][1] - ranges[ii][0];

        if (chrPos >= offset && chrPos <= offset + rangeLen)
        {
            chrCode = ranges[ii][0] + (chrPos - offset);
            break;
        }

        offset += rangeLen + 1;
    }

    if (chrCode > 0xD7FF)
    {
        chrCode -= 0x10000;
        buf[0] = 0xD800 + (chrCode >> 10);
        buf[1] = 0xDC00 + (chrCode & 0x3FF);
    }
    else
        *buf = chrCode;
}

unsigned long genUlong(void)
{
    return (rand() % 256)
           | ((rand() % 256) << 8)
           | ((rand() % 256) << 16)
           | ((rand() % 256) << 24);
}

int writeHeader(size_t columnCnt)
{
    bool quoted;
    wchar_t *str;
    size_t ii;

    for (ii = 0; ii < columnCnt; ii++)
    {
        quoted = rand() % 2;

        if (!(str = genStr(quoted)))
        {
            logError("Failed to generate a string for the header of column "
                     "#%u.", ii);
            return 1;
        }

        if (writeStr(str, quoted))
        {
            freeStr(str);
            logError("Failed to write the header of column #%u.", ii);
            return 1;
        }

        freeStr(str);

        if (ii < columnCnt - 1 && writeSeparator())
        {
            logError("Faield to write a separator after the header of column "
                     "#%u.", ii);
            return 1;
        }
    }

    return 0;
}

int writeStr(const wchar_t *str, bool quoted)
{
    unsigned char *utf8;
    unsigned char *ptr;
    size_t ii;

    if (quoted && writeQuote())
    {
        logError("Failed to write an opening quote.");
        goto fail_opening_quote;
    }

    if (!(utf8 = convToUtf8(str)))
        goto fail_utf8;

    for (ii = 0, ptr = utf8; ii < strlen(utf8); ii++, ptr++)
    {
        if (writeByte(*ptr))
            goto fail_byte;

        if (*ptr == '"' && writeByte('"'))
        {
            logError("Failed to write an escaping quote.");
            goto fail_escaping_quote;
        }
    }

    freeMem(utf8);

    if (quoted && writeQuote())
    {
        logError("Failed to write a closing quote.");
        goto fail_closing_quote;
    }

    return 0;

fail_closing_quote:
    return 1;
fail_escaping_quote:
fail_byte:
    freeMem(utf8);
fail_utf8:
fail_opening_quote:
    return 1;
}

int writeEvent(unsigned int val, bool quoted)
{
    return writeVal(getEventMapEntry(val)->name, quoted);
}

int writeBool(int val, bool quoted)
{
    int mode;
    const wchar_t *str;
    wchar_t buf[64];
    size_t ii;

    mode = rand() % 4;

    switch (mode)
    {
    case 0:
        str = BOOL_TO_STR_TRUE_FALSE(val);
        break;
    case 1:
        str = BOOL_TO_STR_YES_NO(val);
        break;
    case 2:
        str = BOOL_TO_STR_ON_OFF(val);
        break;
    case 3:
        str = BOOL_TO_STR_ONE_ZERO(val);
        break;
    default:
        logError("Unknown boolean output mode %d.", mode);
        return 1;
    }

    if (StringCbCopyW(buf, sizeof(buf), str) != S_OK)
    {
        logError("Failed to copy the boolean value's \"%ls\" string "
                 "representation \"%ls\" to an internal buffer.",
                 BOOL_TO_STR_TRUE_FALSE(val),
                 str);
        return 1;
    }

    for (ii = 0; ii < wcslen(buf); ii++)
    {
        if (rand() % 2)
            buf[ii] = towupper(buf[ii]);
    }

    if (writeVal(buf, quoted))
    {
        logError("Failed to write the boolean value's \"%ls\" string "
                 "representation \"%ls\".",
                 BOOL_TO_STR_TRUE_FALSE(val),
                 buf);
        return 1;
    }

    return 0;
}

int writeVal(const wchar_t *val, bool quoted)
{
    unsigned char *utf8;
    unsigned char *ptr;
    size_t ii;

    if (quoted && writeQuote())
    {
        logError("Failed to write an opening quote.");
        return 1;
    }
    if (writeWhitespace(quoted))
    {
        logError("Failed to write leading whitespace.");
        return 1;
    }

    if (!(utf8 = convToUtf8(val)))
        return 1;

    for (ii = 0, ptr = utf8; ii < strlen(utf8); ii++, ptr++)
    {
        if (writeByte(*ptr))
        {
            freeMem(utf8);
            return 1;
        }
    }

    freeMem(utf8);

    if (writeWhitespace(quoted))
    {
        logError("Failed to write trailing whitespace.");
        return 1;
    }
    if (quoted && writeQuote())
    {
        logError("Failed to write a closing quote quote.");
        return 1;
    }

    return 0;
}

int writeSeparator(void)
{
    return writeByte(',');
}

int writeEol(void)
{
    return writeByte('\r') || writeByte('\n');
}

int writeQuote(void)
{
    if (writeByte('"'))
    {
        logError("Failed to write a quote.");
        return 1;
    }

    return 0;
}

int writeWhitespace(bool quoted)
{
    int ws;
    size_t len;

    len = rand() % MAX_WHITESPACE_LEN;

    while(len)
    {
        ws = rand() % (2 + (quoted && len > 1));

        switch (ws)
        {
        case 0:
            if (writeByte(' '))
            {
                logError("Failed to write a space.");
                return 1;
            }

            break;
        case 1:
            if (writeByte('\t'))
            {
                logError("Failed to write a horizontal tab.");
                return 1;
            }

            break;
        case 2:
            if (writeEol())
            {
                logError("Failed to write an EOL.");
                return 1;
            }

            break;
        default:
            logError("Unknown type of whitespace %d.", ws);
            return 1;
        }

        len--;
    }

    return 0;
}

int writeByte(unsigned char val)
{
    DWORD written;

    if (!byteBufSize)
    {
        if (flush())
        {
            logError("Failed to flush the byte buffer while writing data.");
            return 1;
        }

        byteBufPtr = byteBuf;
        byteBufSize = BYTE_BUF_SIZE;
    }

    *byteBufPtr++ = val;
    byteBufSize--;

    return 0;
}

int flush(void)
{
    DWORD written;

    if (!WriteFile(file, byteBuf, BYTE_BUF_SIZE - byteBufSize, &written, NULL))
    {
        logError("Failed to flush the byte buffer.");
        return 1;
    }

    return 0;
}

unsigned char* convToUtf8(const wchar_t *str)
{
    unsigned char* res;
    int len;

    len = WideCharToMultiByte(CP_UTF8,
                              0,
                              str,
                              -1,
                              NULL,
                              0,
                              NULL,
                              NULL);

    if (!len)
    {
        logUtf8ConvError(str);
        return NULL;
    }

    if (len > SIZE_MAX)
    {
        logError("The UTF-8 encoding of the specified string would not fit "
                 "in memory.");
        return NULL;
    }

    if (!(res = allocMem(len)))
    {
        logError("Failed to allocate memory.");
        return NULL;
    }

    WideCharToMultiByte(CP_UTF8, 0, str, -1, res, len, NULL, NULL);

    return res;
}

void logFileCreationError(const wchar_t *name)
{
    wchar_t buf[1024];

    logError("Failed to create an empty file to write the CSV data in. %ls",
             getSysError(GetLastError(), buf, BUFLEN(buf)));
}

void logUtf8ConvError(const wchar_t *str)
{
    wchar_t buf[1024];

    logError("Failed to convert the specified string to UTF-8. %ls",
             getSysError(GetLastError(), buf, BUFLEN(buf)));
}

wchar_t* getSysError(DWORD code, wchar_t *buf, size_t bufLen)
{
    size_t len;

    assert(buf);
    assert(bufLen);

    len = FormatMessageW(
        FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
        NULL,
        code,
        MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        buf,
        bufLen,
        NULL);

    if (len)
        buf[len - 2] = L'\0';
    else
        StringCchCopyW(buf,
                       bufLen,
                       L"Failed to extract the system error message.");

    return buf;
}
