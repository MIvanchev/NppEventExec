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
#include "mem.h"
#include "plugin.h"
#include "util.h"

static LRESULT CALLBACK cbtProc(int code, WPARAM wp, LPARAM lp);
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

    len = wcslen(str) + 1;

    if (!(res = allocStr(len)))
    {
        /* TODO error */
        return NULL;
    }

    StringCchCopyW(res, len, str);

    return res;
}

wchar_t* reallocStr(wchar_t *str, size_t numUnits)
{
    wchar_t *res;

    if (numUnits > SIZE_MAX / sizeof(wchar_t))
    {
        /* TODO error */
        return NULL;
    }
    else if (!(res = reallocMem(str, sizeof(wchar_t) * numUnits)))
    {
        /* TODO error */
        return NULL;
    }

    return res;
}

wchar_t* allocStr(size_t numUnits)
{
    return reallocStr(NULL, numUnits);
}

void freeStr(wchar_t *str)
{
    freeMem(str);
}

unsigned long countDigits(long num)
{
    unsigned long res;

    for (res = 1; num; res++, num /= 10)
        ;

    return res;
}

size_t countChars(wchar_t *str)
{
    size_t cnt = 0;

    while (*str != L'\0')
    {
        str += ((*str & 0xF800) == 0xD800) ? 2 : 1;
        cnt++;
    }

    return cnt;
}

wchar_t* getFilename(const wchar_t *path)
{
    wchar_t *fname = (wchar_t*) path, *p;

    for (p = fname; *p != L'\0'; p++)
    {
        if (*p == L'\\')
            fname = p + 1;
    }

    return fname;
}

wchar_t* combinePaths(const wchar_t *parent, const wchar_t *child)
{
    size_t lenParent = wcslen(parent);
    size_t lenChild = wcslen(child);
    wchar_t *res;

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

    StringCchPrintfW(res, lenParent + lenChild + 2, L"%ls\\%ls", parent, child);

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

void offsetCtrlPos(HWND parent, HWND ctrl, int dx, int dy)
{
    RECT rc;
    GetWindowRect(ctrl, &rc);
    MapWindowPoints(NULL, parent, (POINT*) &rc, 2);
    SetWindowPos(ctrl,
                 NULL,
                 rc.left + dx,
                 rc.top + dy,
                 0,
                 0,
                 SWP_NOSIZE | SWP_NOZORDER);
}

void offsetCtrlSize(HWND ctrl, int dw, int dh)
{
    RECT rc;
    GetWindowRect(ctrl, &rc);
    SetWindowPos(ctrl,
                 NULL,
                 0,
                 0,
                 rc.right - rc.left + dw,
                 rc.bottom - rc.top + dh,
                 SWP_NOMOVE | SWP_NOZORDER);
}

int msgBox(UINT type,
           HWND parent,
           const wchar_t *title,
           const wchar_t *format,
           ...)
{
    va_list argList;
    int ret;

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

    StringCbVPrintfW(buf, sizeof(buf), format, argList);

    if (!parent)
        parent = GetDesktopWindow();

    cbtHook
        = SetWindowsHookEx(WH_CBT, cbtProc, getPluginInstance(),
                           GetCurrentThreadId());

    res = MessageBox(parent, buf, title, type);

    if (!cbtHook)
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
