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
#include "about_dlg.h"
#include "mem.h"
#include "plugin.h"
#include "resource.h"
#include "util.h"
#include "Notepad_plus_msgs.h"

/** TODO doc */
#define LICENSE_FILENAME PLUGIN_NAME L"_license.txt"

/** TODO doc */
#define FOCUS_MARGIN 2

static void layoutDlg(void);
static INT_PTR CALLBACK aboutDlgProc(HWND dlg, UINT msg, WPARAM wp, LPARAM lp);
static LONG getContentWidth(HWND ctrl);
static void onInitDlg(HWND dlg);
static void onDrawLicenseBtn(DRAWITEMSTRUCT *dis);
static void onOpenLicense(void);

static HWND aboutDlg;
static HWND txtLicense;
static HWND btnLicense;
static HCURSOR handCursor;
static HFONT linkFont;

void openAboutDlg(void)
{
    int ret = DialogBox(getPluginInstance(),
                        MAKEINTRESOURCE(IDD_ABOUT),
                        getNppWnd(),
                        aboutDlgProc);
    if (ret == -1)
    {
        /* TODO error */
        return;
    }
}

void layoutDlg(void)
{
    RECT rc;
    LONG widths[2];
    LONG left;

    GetClientRect(aboutDlg, &rc);
    widths[0] = getContentWidth(txtLicense);
    widths[1] = getContentWidth(btnLicense);
    left = (rc.right - rc.left) / 2 - (widths[0] + widths[1]) / 2;

    GetWindowRect(txtLicense, &rc);
    MapWindowPoints(NULL, aboutDlg, (POINT*) &rc, 2);
    setWndPos(txtLicense, left, rc.top);
    setWndSize(txtLicense, widths[0], rc.bottom - rc.top);
    setWndPos(btnLicense, left + widths[0] - FOCUS_MARGIN, rc.top);
    setWndSize(btnLicense, widths[1] + FOCUS_MARGIN * 2, rc.bottom - rc.top);
}

LONG getContentWidth(HWND ctrl)
{
    RECT rc;
    HDC dc;
    wchar_t content[1024];
    HFONT ctrlFont;
    HFONT prevFont;

    rc.left = 0;
    rc.right = 0;

    dc = GetDC(ctrl);
    ctrlFont = (HFONT) SendMessage(ctrl, WM_GETFONT, 0, 0);
    prevFont = (HFONT) SelectObject(dc, ctrlFont);
    GetWindowText(ctrl, content, BUFLEN(content));
    DrawTextW(dc,
              content,
              wcslen(content),
              &rc,
              DT_SINGLELINE | DT_CALCRECT);
    SelectObject(dc, prevFont);
    ReleaseDC(ctrl, dc);

    return rc.right - rc.left;
}

INT_PTR CALLBACK aboutDlgProc(HWND dlg, UINT msg, WPARAM wp, LPARAM lp)
{
    switch (msg)
    {
    case WM_INITDIALOG:
        onInitDlg(dlg);
        return FALSE;

    case WM_DESTROY:
        if (linkFont)
            DeleteObject(linkFont);

        DLGPROC_RESULT(dlg, 0);

    case WM_DRAWITEM:
        onDrawLicenseBtn((DRAWITEMSTRUCT*) lp);
        DLGPROC_RESULT(dlg, TRUE);

    case WM_CLOSE:
        EndDialog(dlg, IDCANCEL);
        DLGPROC_RESULT(dlg, 0);

    case WM_SYSCOMMAND:
        if (wp == SC_CLOSE)
        {
            EndDialog(dlg, IDCLOSE);
            DLGPROC_RESULT(dlg, 0);
        }
        break;

    case WM_COMMAND:
        switch (LOWORD(wp))
        {
        case IDOK:
        case IDCANCEL:
            EndDialog(dlg, LOWORD(wp));
            DLGPROC_RESULT(dlg, 0);
        case IDC_BT_LICENSE:
            onOpenLicense();
            DLGPROC_RESULT(dlg, 0);
        }
        break;
    } /* switch (msg) */

    return FALSE;
}

void onInitDlg(HWND dlg)
{
    HFONT font;
    LOGFONT lf;
    LONG style;

    aboutDlg = dlg;
    txtLicense = GetDlgItem(dlg, IDC_ST_LICENSE);
    btnLicense = GetDlgItem(dlg, IDC_BT_LICENSE);

    /* Not set in the RC file because of a weird resource compiler bug when
    ** concatenating strings in the DIALOG template (only MSVC).
    */

    SetWindowText(GetDlgItem(dlg, IDC_ST_VERSION),
                  PLUGIN_NAME L" " VERSION_STRING);

    SetWindowText(dlg, PLUGIN_NAME L": About");

    handCursor = LoadCursor(NULL, IDC_HAND);

    font = (HFONT) SendMessage(btnLicense, WM_GETFONT, 0, 0);
    GetObject(font, sizeof(LOGFONT), &lf);
    lf.lfUnderline = TRUE;

    if (!(linkFont = CreateFontIndirect(&lf)))
    {
        /* TODO error */
        EndDialog(dlg, 0);
        return;
    }

    style = GetWindowLong(btnLicense, GWL_STYLE);
    SetWindowLong(btnLicense, GWL_STYLE, style | BS_OWNERDRAW);
    SendMessage(btnLicense,
                WM_SETFONT,
                (WPARAM) linkFont,
                (LPARAM) MAKELPARAM(TRUE, 0));

    layoutDlg();
    centerWndToParent(dlg);
    SetFocus(GetDlgItem(dlg, IDOK));
}

void onDrawLicenseBtn(DRAWITEMSTRUCT *dis)
{
    HDC dc;
    RECT *rc;
    wchar_t content[256];
    COLORREF prevClr;

    dc = dis->hDC;
    rc = &dis->rcItem;

    FillRect(dc, rc, (HBRUSH) (COLOR_3DFACE + 1));
    GetWindowTextW(btnLicense, content, BUFLEN(content));
    prevClr = SetTextColor(dc, RGB(0, 0, 255));
    DrawTextW(dc, content, wcslen(content), rc,
              DT_CENTER | DT_SINGLELINE);
    SetTextColor(dc, prevClr);

    if (dis->itemState & ODS_FOCUS)
        DrawFocusRect(dc, rc);
}

void onOpenLicense(void)
{
    wchar_t *path;

    if (!(path = combinePaths(getPluginDir(), L"doc\\" LICENSE_FILENAME)))
    {
        /* TODO error */
        goto fail_path;
    }

    /* Try real hard to avoid Notepad++ asking to create the file if it doesn't
    ** exist.
    **/

    if (GetFileAttributesW(path) == INVALID_FILE_ATTRIBUTES)
    {
        DWORD err = GetLastError();

        if (err == ERROR_FILE_NOT_FOUND
            || err == ERROR_PATH_NOT_FOUND)
        {
            /* TODO error */
            goto fail_not_found;
        }
        else
        {
            /* TODO error */
            goto fail_attribs;
        }
    }

    EndDialog(aboutDlg, IDC_BT_LICENSE);

    if (!sendNppMsg(NPPM_DOOPEN, 0, (LPARAM) path))
    {
        /* TODO error */
        goto fail_open;
    }

    freeStr(path);
    return;

fail_open:
fail_attribs:
fail_not_found:
    freeStr(path);
fail_path:
    return;
}
