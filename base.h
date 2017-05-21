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
#ifndef __BASE_H__
#define __BASE_H__

#define WIN32_LEAN_AND_MEAN
#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif
#include <windows.h>
#include <windowsx.h>
#include <shellapi.h>
#include <commctrl.h>
#include <tchar.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <limits.h>
#include <assert.h>

#ifndef SIZE_MAX
#define SIZE_MAX ((size_t) -1)
#endif

#ifndef LVS_EX_FULLROWSELECT
#define LVS_EX_FULLROWSELECT 0x00000020
#endif

#ifndef LVS_OWNERDATA
#define LVS_OWNERDATA 0x00001000
#endif

#ifndef LVM_SETEXTENDEDLISTVIEWSTYLE
#define LVM_SETEXTENDEDLISTVIEWSTYLE (LVM_FIRST+54)
#endif

#ifndef ListView_SetExtendedListViewStyleEx
#define ListView_SetExtendedListViewStyleEx(ctrl, wp, lp) SendMessage((ctrl),                       \
                                                                      LVM_SETEXTENDEDLISTVIEWSTYLE, \
                                                                      (wp),                         \
                                                                      (lp))
#endif

/* Eliminate the warnings about inline functions used but never defined when
** compiling with Mingw-w64 and including strsafe.h.
*/

#ifdef __MINGW32__

#ifndef __CRT_STRSAFE_IMPL
#define __CRT_STRSAFE_IMPL
#define DID_DEFINE__CRT_STRSAFE_IMPL
#endif

#ifndef __STRSAFE__NO_INLINE
#define __STRSAFE__NO_INLINE
#define DID_DEFINE__STRSAFE__NO_INLINE
#endif

#ifdef __cplusplus
extern "C" {
#endif

#endif /* __MINGW32__ */

#include <strsafe.h>

#ifdef __MINGW32__

#ifdef __cplusplus
}
#endif

#ifdef DID_DEFINE__CRT_STRSAFE_IMPL
#undef __CRT_STRSAFE_IMPL
#undef DID_DEFINE__CRT_STRSAFE_IMPL
#endif

#ifdef DID_DEFINE__STRSAFE__NO_INLINE
#undef __STRSAFE__NO_INLINE
#undef DID_DEFINE__STRSAFE__NO_INLINE
#endif

#endif /* __MINGW32__ */

#define BUFLEN(b) (sizeof(b) / sizeof((b)[0]))

#define BUF_LEN_FOR_CHAR_COUNT(cnt) ((cnt) * sizeof(wchar_t) + 1)

#ifndef IS_HIGH_SURROGATE
#define IS_HIGH_SURROGATE(code) (((code) & 0xF800) == 0xD800)
#endif

#define IS_SPACE(chr) ((chr) == L' ' || (chr) == L'\t')

#define STRA(x)  # x
#define STRW_(x) L ## x
#define STRW(x)  STRW_(# x)
#define STR(x)   STRW(x)

#define MIN(x, y) ((x) < (y) ? (x) : (y))
#define MAX(x, y) ((x) < (y) ? (y) : (x))

#define VERSION_MAJOR  0
#define VERSION_MINOR  1
#define VERSION_PATCH  1
#define VERSION_STRING STR(VERSION_MAJOR) L"." \
    STR(VERSION_MINOR) L"."                    \
    STR(VERSION_PATCH) L".0"

#define DLGPROC_RESULT(dlg, res)                    \
    SetWindowLongPtr((dlg), DWLP_MSGRESULT, (res)); \
    return TRUE;

#define setWndPos(wnd, px, py)  SetWindowPos((wnd), \
                                             NULL,  \
                                             (px),  \
                                             (py),  \
                                             0,     \
                                             0,     \
                                             SWP_NOSIZE | SWP_NOZORDER)
#define setWndSize(wnd, sw, sh) SetWindowPos((wnd), \
                                             NULL,  \
                                             0,     \
                                             0,     \
                                             (sw),  \
                                             (sh),  \
                                             SWP_NOMOVE | SWP_NOZORDER)

#define BOOL_TO_STR_TRUE_FALSE(val) ((val) ? L"true" : L"false")
#define BOOL_TO_STR_YES_NO(val)     ((val) ? L"yes" : L"no")
#define BOOL_TO_STR_ON_OFF(val)     ((val) ? L"on" : L"off")
#define BOOL_TO_STR_ONE_ZERO(val)   ((val) ? L"1" : L"0")

/** The official name of this plugin. */
#define PLUGIN_NAME L"NppEventExec"

/** A very short description of what this plugin is. */
#define PLUGIN_DESCRIPTION L"NppEventExec plugin for Notepad++"

/** A very short description of what this plugin is. */
#define PLUGIN_COPYRIGHT L"(C) 2016-2017 Mihail Ivanchev"

/** The license this plugin is distributed under. */
#define PLUGIN_LICENSE L"GNU General Public License, Version 3 (GNU GPLv3)"

/** The (required) name of this plugin's DLL. */
#define PLUGIN_MODULE PLUGIN_NAME L".dll"

/** The web page of this plugin. */
#define PLUGIN_PAGE L"https://www.github.com/MIvanchev/NppEventExec"

/** The usual name of the NppExec DLL. */
#define NPPEXEC_PLUGIN_MODULE L"NppExec.dll"

#endif /* __BASE_H__ */
