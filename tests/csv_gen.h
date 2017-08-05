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
#ifndef __CSV_GEN_H__
#define __CSV_GEN_H__

typedef enum
{
    CSV_STRING,
    CSV_EVENT,
    CSV_BOOL,
    CSV_TYPE_CNT
} CsvType;

typedef struct
{
    union
    {
        struct
        {
            wchar_t *strVal;
            size_t charCnt;
            size_t unitCnt;
        };
        unsigned int eventVal;
        bool boolVal;
    };
    bool quoted;
} CsvField;

typedef struct
{
    CsvType type;
    wchar_t *hdr;
    CsvField *fields;
} CsvColumn;

typedef struct
{
    bool hdr;
    unsigned int columnCnt;
    unsigned int recordCnt;
    CsvColumn columns[];
} CsvData;

CsvData* genCsv(void);
void freeData(CsvData *data);

#endif /* __CSV_GEN_H__ */
