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
#ifndef __UTIL_H__
#define __UTIL_H__

typedef struct
{
    HWND hWnd;
    HWND hWndInsertAfter;
    int X;
    int Y;
    int cx;
    int cy;
    UINT uFlags;
} SetWindowPosArgs;

typedef struct
{
    int column;
    wchar_t *header;
} ListViewColumn;

typedef struct
{
    int column;
    double size;
} ListViewColumnSize;

#ifdef __cplusplus
extern "C" {
#endif

wchar_t* copyStr(const wchar_t *str);
wchar_t* allocStr(size_t unitCnt);
wchar_t* reallocStr(wchar_t *str, size_t unitCnt);
void freeStr(wchar_t *str);
wchar_t* getFilename(const wchar_t *path);
wchar_t* combinePaths(const wchar_t *parent, const wchar_t *child);
void centerWndToParent(HWND wnd);
int getChildWndCount(HWND wnd);
void setWndPosDeferred(const SetWindowPosArgs *ops);
void addListViewColumns(HWND listView, ListViewColumn *columns);
void sizeListViewColumns(HWND listView, ListViewColumnSize *sizes);
int msgBox(UINT type,
           HWND parent,
           const wchar_t *title,
           const wchar_t *format,
           ...);
int errorMsgBox(HWND parent, const wchar_t *format, ...);

size_t countChars(const wchar_t *str);

#ifdef __cplusplus
}
#endif

#endif /* __UTIL_H__ */
