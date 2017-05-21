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
#define PLUGIN_LICENSE_PATH L"doc\\" PLUGIN_NAME L"_license.txt"

/** TODO doc */
#define ICONS_LICENSE_PATH L"doc\\" L"icon_set_license.txt"

/** TODO doc */
#define URL_AHASOFT L"http://www.aha-soft.com/"

/** TODO doc */
#define URL_ICON_SET \
    L"http://www.small-icons.com/packs/16x16-free-application-icons.htm"

static void layoutDlg(void);
static INT_PTR CALLBACK aboutDlgProc(HWND dlg, UINT msg, WPARAM wp, LPARAM lp);
static void onInitDlg(HWND dlg);
static void onDrawItem(DRAWITEMSTRUCT *dis);
static void onOpenPluginLicense(void);
static void onOpenIconsLicense(void);
static void layoutLine(LONG clientWidth, int *ctrlIds);
static LONG getContentWidth(HWND ctrl);
static void openDocument(const wchar_t *relativePath);
static void openUrl(const wchar_t *url);

static HWND aboutDlg;
static HCURSOR handCursor;
static HFONT linkFont;

void openAboutDlg(void)
{
    INT_PTR ret;

    ret = DialogBox(getPluginInstance(), MAKEINTRESOURCE(IDD_ABOUT),
                    getNppWnd(), aboutDlgProc);
    if (ret == -1)
    {
        /* TODO error */
        return;
    }
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
        onDrawItem((DRAWITEMSTRUCT*) lp);
        DLGPROC_RESULT(dlg, TRUE);

    case WM_SETCURSOR:
        /* We know BS_OWNERDRAW != SS_OWNERDRAW. */

        if ((HWND) wp != aboutDlg
            && GetWindowLong((HWND) wp, GWL_STYLE) & BS_OWNERDRAW)
        {
            SetCursor(handCursor);
            DLGPROC_RESULT(dlg, TRUE);
        }

        break;

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
        case IDC_BT_WEB_PAGE:
            openUrl(PLUGIN_PAGE);
            DLGPROC_RESULT(dlg, 0);
        case IDC_BT_LICENSE:
            onOpenPluginLicense();
            DLGPROC_RESULT(dlg, 0);
        case IDC_BT_ICONS_SET:
            openUrl(URL_ICON_SET);
            DLGPROC_RESULT(dlg, 0);
        case IDC_BT_ICONS_AHASOFT:
            openUrl(URL_AHASOFT);
            DLGPROC_RESULT(dlg, 0);
        case IDC_BT_ICONS_LICENSE:
            onOpenIconsLicense();
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

    aboutDlg = dlg;

    /* Not set in the RC file because of a weird resource compiler bug when
    ** concatenating strings in the DIALOG template (only MSVC).
    */

    SetWindowText(GetDlgItem(dlg, IDC_ST_VERSION),
                  PLUGIN_NAME L" " VERSION_STRING);

    SetWindowText(dlg, PLUGIN_NAME L": About");

    handCursor = LoadCursor(NULL, IDC_HAND);

    font = (HFONT) SendMessage(aboutDlg, WM_GETFONT, 0, 0);
    GetObject(font, sizeof(LOGFONT), &lf);
    lf.lfUnderline = TRUE;

    if (!(linkFont = CreateFontIndirect(&lf)))
    {
        /* TODO error */
        EndDialog(dlg, 0);
        return;
    }

#define SET_LINK_FONT(id)                                                \
    SendMessage(GetDlgItem(aboutDlg, id), WM_SETFONT, (WPARAM) linkFont, \
                (LPARAM) MAKELPARAM(TRUE, 0))                            \

    SET_LINK_FONT(IDC_BT_WEB_PAGE);
    SET_LINK_FONT(IDC_BT_LICENSE);
    SET_LINK_FONT(IDC_BT_ICONS_SET);
    SET_LINK_FONT(IDC_BT_ICONS_AHASOFT);
    SET_LINK_FONT(IDC_BT_ICONS_LICENSE);

    layoutDlg();
    centerWndToParent(dlg);
    SetFocus(GetDlgItem(dlg, IDOK));
}

void onDrawItem(DRAWITEMSTRUCT *dis)
{
    HWND ctrl;
    HDC dc;
    RECT *rc;
    RECT rc1;
    wchar_t content[256];
    COLORREF prevClr;

    ctrl = dis->hwndItem;
    dc = dis->hDC;
    rc = &dis->rcItem;

    CopyRect(&rc1, rc);
    rc1.top += 1;
    rc1.bottom -= 1;

    FillRect(dc, rc, (HBRUSH) (COLOR_3DFACE + 1));
    GetWindowTextW(ctrl, content, BUFLEN(content));

    if (dis->CtlType == ODT_BUTTON)
    {
        prevClr = SetTextColor(dc, RGB(0, 0, 255));
        DrawTextW(dc, content, wcslen(content), &rc1,
                  DT_CENTER | DT_SINGLELINE);
        SetTextColor(dc, prevClr);
    }
    else
    {
        DrawTextW(dc, content, wcslen(content), &rc1,
                  DT_CENTER | DT_SINGLELINE);
    }

    if (dis->itemState & ODS_FOCUS)
        DrawFocusRect(dc, rc);
}

void onOpenPluginLicense(void)
{
    openDocument(PLUGIN_LICENSE_PATH);
}

void onOpenIconsLicense(void)
{
    openDocument(ICONS_LICENSE_PATH);
}

void layoutDlg(void)
{
    RECT rc;
    LONG clientWidth;

    GetClientRect(aboutDlg, &rc);
    clientWidth = rc.right - rc.left;

    layoutLine(clientWidth, (int[]) {IDC_ST_VERSION, -1 });
    layoutLine(clientWidth, (int[]) {IDC_ST_COPYRIGHT, -1});
    layoutLine(clientWidth, (int[]) {IDC_BT_WEB_PAGE, -1});
    layoutLine(clientWidth, (int[]) {IDC_ST_LICENSE, IDC_BT_LICENSE, -1});
    layoutLine(clientWidth, (int[]) {IDC_ST_ICONS_USING, IDC_BT_ICONS_SET, -1});
    layoutLine(clientWidth, (int[]) {IDC_ST_ICONS_BY, IDC_BT_ICONS_AHASOFT,
                                     IDC_ST_ICONS_DISTRIBUTED, -1});
    layoutLine(clientWidth, (int[]) {IDC_BT_ICONS_LICENSE, -1});
}

void layoutLine(LONG clientWidth, int *ctrlIds)
{
    int *ctrlId;
    HWND ctrl;
    RECT rc;
    LONG pos;
    LONG width;
    LONG lineWidth;
    bool url;

    lineWidth = 0;

    for (ctrlId = ctrlIds; *ctrlId != -1; ctrlId++)
    {
        ctrl = GetDlgItem(aboutDlg, *ctrlId);
        /* We know BS_OWNERDRAW != SS_OWNERDRAW. */
        url = GetWindowLong(ctrl, GWL_STYLE) & BS_OWNERDRAW;
        width = getContentWidth(ctrl) + url * 2;
        GetWindowRect(ctrl, &rc);
        setWndSize(ctrl, width, rc.bottom - rc.top);
        lineWidth += width;
    }

    pos = clientWidth / 2 - lineWidth / 2;

    for (ctrlId = ctrlIds; *ctrlId != -1; ctrlId++)
    {
        ctrl = GetDlgItem(aboutDlg, *ctrlId);
        GetWindowRect(ctrl, &rc);
        MapWindowPoints(NULL, aboutDlg, (POINT*) &rc, 2);
        setWndPos(ctrl, pos, rc.top);

        pos += rc.right - rc.left;
    }
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

void openDocument(const wchar_t *relativePath)
{
    wchar_t *path;

    if (!(path = combinePaths(getPluginDir(), relativePath)))
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

    EndDialog(aboutDlg, 1);

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

void openUrl(const wchar_t *url)
{
    HINSTANCE res;

    res = ShellExecuteW(NULL,
                        L"open",
                        url,
                        NULL,
                        NULL,
                        SW_SHOWNORMAL);

    /* Some more Win32 nonsense. ShellExecute returns a numeric return code as
    ** an HINSTANCE.
    */

    if ((int) res <= 32)
    {
        errorMsgBox(aboutDlg, L"Failed to open the URL \"%s\".", url);
        return;
    }
}
