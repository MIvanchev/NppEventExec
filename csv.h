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
#ifndef __CSV_H__
#define __CSV_H__

typedef enum
{
    BOOL_TRUE_FALSE,
    BOOL_YES_NO,
    BOOL_ON_OFF,
    BOOL_ONE_ZERO
} BoolOutputMode;

#ifdef __cplusplus
extern "C" {
#endif

int csvOpen(wchar_t *path, size_t fieldCnt, int header);
int csvCreate(wchar_t *path, size_t fieldCnt);
int csvFlush(void);
void csvClose(void);
int csvHasData(void);
wchar_t* csvReadString(size_t *unitCnt, size_t *charCnt);
int csvReadBool(void);
int csvReadEvent(unsigned int *val);
int csvWriteString(const wchar_t *str);
int csvWriteBool(int val, BoolOutputMode mode);
int csvWriteEvent(unsigned int event);

#ifdef __cplusplus
}
#endif

#endif /* __CSV_H__ */
