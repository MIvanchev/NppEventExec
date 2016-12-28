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
#ifndef __CSV_H__
#define __CSV_H__

#ifdef __cplusplus
extern "C" {
#endif

int csvOpen(wchar_t *path, int header, size_t numFields);
void csvClose(void);
int csvHasChars(void);
int csvGetChars(wchar_t *buf, size_t bufLen, size_t *numUnits,
                size_t *numChars);
wchar_t *csvGetString(size_t minChars, size_t maxChars);
int csvSkipValue(void);
int csvGetValue(wchar_t *buf, size_t maxLen);
int csvGetBool(void);
int csvGetEvent(unsigned int *event);

#ifdef __cplusplus
}
#endif

#endif /* __CSV_H__ */
