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
#include "mem.h"
#include "plugin.h"
#include "util.h"
#include <math.h>

typedef struct
{
    int count;
    bool success;
} GetChildWndCountData;

static LRESULT CALLBACK cbtProc(int code, WPARAM wp, LPARAM lp);
static BOOL CALLBACK getChildWndCountProc(HWND wnd, LPARAM lp);
static int msgBoxV(UINT type,
                   HWND parent,
                   const wchar_t *title,
                   const wchar_t *format,
                   va_list argList);

static HHOOK cbtHook;

wchar_t* copyStr(const wchar_t *str)
{
    wchar_t *res;
    size_t len;

    assert(str);

    len = wcslen(str) + 1;

    if (!(res = allocStr(len)))
    {
        /* TODO error */
        return NULL;
    }

    StringCchCopyW(res, len, str);

    return res;
}

wchar_t* reallocStr(wchar_t *str, size_t unitCnt)
{
    wchar_t *res;

    assert(unitCnt);

    if (unitCnt > SIZE_MAX / sizeof *res)
    {
        /* TODO error */
        return NULL;
    }

    if (!(res = reallocMem(str, unitCnt * sizeof *res)))
    {
        /* TODO error */
        return NULL;
    }

    return res;
}

wchar_t* allocStr(size_t unitCnt)
{
    assert(unitCnt);

    return reallocStr(NULL, unitCnt);
}

void freeStr(wchar_t *str)
{
    freeMem(str);
}

wchar_t* getFilename(const wchar_t *path)
{
    wchar_t *fname;
    wchar_t *chr;

    assert(path);

    fname = (wchar_t*) path;

    for (chr = fname; *chr != L'\0'; chr++)
    {
        if (*chr == L'\\')
            fname = chr + 1;
    }

    return fname;
}

wchar_t* combinePaths(const wchar_t *parent, const wchar_t *child)
{
    size_t lenParent;
    size_t lenChild;
    wchar_t *res;

    assert(parent);
    assert(child);

    lenParent = wcslen(parent);
    lenChild =  wcslen(child);

    if (STRSAFE_MAX_CCH < lenParent
        || STRSAFE_MAX_CCH - lenParent < lenChild
        || STRSAFE_MAX_CCH - lenParent - lenChild < 2)
    {
        /* TODO error */
        return NULL;
    }

    if (!(res = allocStr(lenParent + lenChild + 2)))
    {
        /* TODO error */
        return NULL;
    }

    StringCchCopyW(res, lenParent + 1, parent);
    res[lenParent] = L'\\';
    StringCchCopyW(&res[lenParent + 1], lenChild + 1, child);

    return res;
}

void centerWndToParent(HWND wnd)
{
    HWND parent;
    RECT parentRc;
    RECT wndRc;
    LONG parentW;
    LONG parentH;
    LONG width;
    LONG height;
    LONG left;
    LONG top;

    if (!(parent = GetParent(wnd)))
        parent = GetDesktopWindow();

    GetWindowRect(parent, &parentRc);
    GetWindowRect(wnd, &wndRc);

    parentW = parentRc.right - parentRc.left;
    parentH = parentRc.bottom - parentRc.top;
    width = wndRc.right - wndRc.left;
    height = wndRc.bottom - wndRc.top;
    left = parentRc.left + (parentW - width) / 2;
    top = parentRc.top + (parentH - height) / 2;

    MoveWindow(wnd, left, top, width, height, FALSE);
}

int getChildWndCount(HWND wnd)
{
    GetChildWndCountData data;

    assert(wnd);

    data.count = 0;
    data.success = true;

    EnumChildWindows(wnd, getChildWndCountProc, (LPARAM) &data);

    if (!data.success)
    {
        /* TODO error */
        return -1;
    }

    return data.count;
}

BOOL CALLBACK getChildWndCountProc(HWND wnd, LPARAM lp)
{
    GetChildWndCountData *data;

    data = (GetChildWndCountData*) lp;

    if (data->count == INT_MAX)
    {
        /* TODO error */
        data->success = false;
        return FALSE;
    }

    data->count++;

    return TRUE;
}

void setWndPosDeferred(const SetWindowPosArgs *ops)
{
    size_t count;
    SetWindowPosArgs *op;
    HDWP hdwp;

    assert(ops);

    count = 0;

    for (op = (SetWindowPosArgs*) ops; op->hWnd; op++)
        count++;

    if (count > INT_MAX)
    {
        /* TODO error */
        goto fail;
    }

    if (!(hdwp = BeginDeferWindowPos((int) count)))
    {
        /* TODO error */
        goto fail;
    }

    for (op = (SetWindowPosArgs*) ops; op->hWnd; op++)
    {
        if (!(hdwp = DeferWindowPos(hdwp,
                                    op->hWnd,
                                    op->hWndInsertAfter,
                                    op->X,
                                    op->Y,
                                    op->cx,
                                    op->cy,
                                    op->uFlags)))
        {
            /* TODO error */
            goto fail;
        }
    }

    EndDeferWindowPos(hdwp);

    return;

fail:

    for (op = (SetWindowPosArgs*) ops; op->hWnd; op++)
    {
        SetWindowPos(op->hWnd,
                     op->hWndInsertAfter,
                     op->X,
                     op->Y,
                     op->cx,
                     op->cy,
                     op->uFlags);
    }
}

void addListViewColumns(HWND listView, ListViewColumn *columns)
{
    LVCOLUMN col;

    assert(listView);
    assert(columns);

    for (; columns->column != -1; columns++)
    {
        col.mask = LVCF_WIDTH | LVCF_TEXT | LVCF_SUBITEM;
        col.iSubItem = columns->column;
        col.pszText = columns->header;
        col.cx = 0;

        ListView_InsertColumn(listView, columns->column, &col);
    }
}

void sizeListViewColumns(HWND listView, ListViewColumnSize *sizes)
{
    RECT rc;
    LONG totalWidth;
    LONG remainingWidth;
    LONG width;

    assert(listView);
    assert(sizes);

    GetClientRect(listView, &rc);

    totalWidth = rc.right - rc.left;
    remainingWidth = totalWidth;

    for (; sizes->column != -1; sizes++)
    {
        /* Unlikely that this will get problematic */
        width = (LONG) (totalWidth * sizes->size);
        if (width > remainingWidth)
            width = remainingWidth;

        remainingWidth -= width;

        ListView_SetColumnWidth(listView, sizes->column, width);
    }
}

int msgBox(UINT type,
           HWND parent,
           const wchar_t *title,
           const wchar_t *format,
           ...)
{
    va_list argList;
    int ret;

    assert(title);
    assert(format);

    va_start(argList, format);
    ret = msgBoxV(type, parent, title, format, argList);
    va_end(argList);

    return ret;
}

int msgBoxV(UINT type,
            HWND parent,
            const wchar_t *title,
            const wchar_t *format,
            va_list argList)
{
    wchar_t buf[1024];
    int res;

    assert(title);
    assert(format);

    StringCbVPrintfW(buf, sizeof(buf), format, argList);

    if (!parent)
        parent = GetDesktopWindow();

    cbtHook = SetWindowsHookEx(WH_CBT,
                               cbtProc,
                               getPluginInstance(),
                               GetCurrentThreadId());

    res = MessageBoxEx(parent, buf, title, type,
                       MAKELANGID(LANG_ENGLISH, SUBLANG_ENGLISH_US));

    if (cbtHook)
    {
        UnhookWindowsHookEx(cbtHook);
        cbtHook = NULL;
    }

    return res;
}

int errorMsgBox(HWND parent, const wchar_t *format, ...)
{
    va_list argList;
    int ret;

    assert(format);

    va_start(argList, format);
    ret = msgBoxV(MB_OK | MB_ICONERROR, parent, PLUGIN_NAME L": Error", format,
                  argList);
    va_end(argList);

    return ret;
}

LRESULT CALLBACK cbtProc(int code, WPARAM wp, LPARAM lp)
{
    LRESULT res;

    if (code == HCBT_ACTIVATE)
    {
        centerWndToParent((HWND) wp);
        res = CallNextHookEx(cbtHook, code, wp, lp);
        UnhookWindowsHookEx(cbtHook);
        cbtHook = NULL;
        return res;
    }

    return CallNextHookEx(cbtHook, code, wp, lp);
}
