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
#include "match.h"
#include "mem.h"
#include "plugin.h"
#include "resource.h"
#include "rule.h"
#include "rules_dlg.h"
#include "edit_dlg.h"
#include "util.h"
#include "Notepad_plus_msgs.h"
#include "Scintilla.h"
#include "exec.h"
#include "queue_dlg.h"

/** TODO */
#define DLG_TITLE PLUGIN_NAME L": Rules"

/** TODO */
#define TOOLBAR_BUTTON_COUNT 6

typedef enum
{
    COL_ENABLED,
    COL_EVENT,
    COL_NAME,
    COL_REGEX,
    COL_CMD,
    COL_BACKGROUND
} Column;

typedef struct
{
    Rule **activeRules;
    Rule *rules;
    Rule *lastRule;
    unsigned int ruleCnt;
    bool initialized;
    bool modified;
    HWND handle;
    HWND toolbar;
    HWND lvRules;
    WNDPROC lvRulesProc;
    LONG padding;
    LONG minWidth;
    LONG minHeight;
    LONG clientWidth;
    LONG clientHeight;
    HWND btnReset;
    HWND btnSave;
    HWND btnClose;
    HIMAGELIST ilButtons;
    HIMAGELIST ilButtonsDisabled;
} Dialog;

static INT_PTR CALLBACK dlgProc(HWND dlg, UINT msg, WPARAM wp, LPARAM lp);
static void onInitDlg(HWND dlg);
static void onSize(LONG clientWidth, LONG clientHeight);
static void onGetDispInfo(NMLVDISPINFO *dispInfo);
static void onItemChanged(NMLISTVIEW *info);
static void onReset(void);
static void onSave(void);
static void onMoveUp(void);
static void onMoveDown(void);
static void onAdd(void);
static void onCopy(void);
static void onRemove(void);
static void onEdit(void);
static void addToolbarGap(void);
static void addToolbarBtn(int bmpIndex, int cmdId, int strId);
static void setToolbarBtnEnabled(int btnCmd, bool enabled);
static void layoutDlg(void);
static BOOL CALLBACK layoutDlgProc(HWND wnd, LPARAM lp);
static LRESULT CALLBACK lvRulesProc(HWND wnd, UINT msg, WPARAM wp, LPARAM lp);
static bool resetRules(void);
static bool saveRules(void);
static void deselectAll(void);
static void deselectRule(int pos);
static void selectRule(int pos);
static void setModified(bool modified);
static void askToSaveChanges(void);

static Dialog *dlg;

int openRulesDlg(Rule **activeRules)
{
    INT_PTR res;

    assert(activeRules);

    if (!(dlg = allocMem(sizeof *dlg)))
    {
        /* TODO error */
        return 1;
    }

    dlg->activeRules = activeRules;
    dlg->rules = NULL;
    dlg->initialized = false;

    res = DialogBoxW(getPluginInstance(), MAKEINTRESOURCE(IDD_RULES),
                     getNppWnd(), dlgProc);

    freeMem(dlg);
    dlg = NULL;

    if (res < 0)
    {
        /* TODO error */
        return 1;
    }

    return 0;
}

INT_PTR CALLBACK dlgProc(HWND handle, UINT msg, WPARAM wp, LPARAM lp)
{
    switch (msg)
    {
    case WM_INITDIALOG:
        onInitDlg(handle);
        return TRUE;

    case WM_DESTROY:

        if (dlg->initialized)
        {
            freeRules(dlg->rules);
            ImageList_Destroy(dlg->ilButtons);
            ImageList_Destroy(dlg->ilButtonsDisabled);
        }

        DLGPROC_RESULT(handle, 0);

    case WM_GETMINMAXINFO:
        ((MINMAXINFO*) lp)->ptMinTrackSize.x = dlg->minWidth;
        ((MINMAXINFO*) lp)->ptMinTrackSize.y = dlg->minHeight;
        DLGPROC_RESULT(handle, 0);

    case WM_SIZE:
        onSize(LOWORD(lp), HIWORD(lp));
        DLGPROC_RESULT(handle, 0);

    case WM_CLOSE:
        askToSaveChanges();
        DLGPROC_RESULT(handle, 0);

    case WM_SYSCOMMAND:
        if (wp == SC_CLOSE)
        {
            askToSaveChanges();
            DLGPROC_RESULT(handle, 0);
        }
        break;

    case WM_COMMAND:
        switch (LOWORD(wp))
        {
        case ID_RULE_MOVEUP:
            onMoveUp();
            DLGPROC_RESULT(handle, 0);
        case ID_RULE_MOVEDOWN:
            onMoveDown();
            DLGPROC_RESULT(handle, 0);
        case ID_RULE_ADD:
            onAdd();
            DLGPROC_RESULT(handle, 0);
        case ID_RULE_COPY:
            onCopy();
            DLGPROC_RESULT(handle, 0);
        case ID_RULE_REMOVE:
            onRemove();
            DLGPROC_RESULT(handle, 0);
        case ID_RULE_EDIT:
            onEdit();
            DLGPROC_RESULT(handle, 0);
        case IDC_BT_RESET:
            onReset();
            DLGPROC_RESULT(handle, 0);
        case IDC_BT_SAVE:
            onSave();
            DLGPROC_RESULT(handle, 0);
        case IDCANCEL:
            askToSaveChanges();
            DLGPROC_RESULT(handle, 0);
        }
        break;

    case WM_NOTIFY:
        switch (LOWORD(wp))
        {
        case IDC_LV_RULES:
            switch (((NMHDR*) lp)->code)
            {
            case LVN_ITEMCHANGED:
                onItemChanged((NMLISTVIEW*) lp);
                break;

            case NM_DBLCLK:
            case NM_RETURN:
                onEdit();
                break;

            case LVN_GETDISPINFO:
                onGetDispInfo((NMLVDISPINFO*) lp);
                break;
            }
            break;
        } /* switch (LOWORD(wp)) */
        break;
    } /* switch (msg) */

    return FALSE;
}

void onInitDlg(HWND handle)
{
    HBITMAP bmpBtns;
    HBITMAP bmpBtnsMasked;
    HBITMAP bmpBtnsDisabled;

    dlg->handle = handle;

    if (!(bmpBtns = LoadBitmap(getPluginInstance(),
                               MAKEINTRESOURCE(IDB_BUTTONS))))
    {
        /* TODO error */
        goto fail_btns_bmp;
    }
    if (!(bmpBtnsMasked = LoadBitmap(getPluginInstance(),
                                     MAKEINTRESOURCE(IDB_BUTTONS_MASKED))))
    {
        /* TODO error */
        goto fail_btns_masked_bmp;
    }
    if (!(bmpBtnsDisabled = LoadBitmap(getPluginInstance(),
                                       MAKEINTRESOURCE(IDB_BUTTONS_DISABLED))))
    {
        /* TODO error */
        goto fail_btns_disabled_bmp;
    }
    if (!(dlg->ilButtons
              = ImageList_Create(16, 16, ILC_COLOR24 | ILC_MASK,
                                 TOOLBAR_BUTTON_COUNT, 0)))
    {
        /* TODO error */
        goto fail_btns_img_list;
    }
    if (!(dlg->ilButtonsDisabled
              = ImageList_Create(16, 16, ILC_COLOR24 | ILC_MASK,
                                 TOOLBAR_BUTTON_COUNT, 0)))
    {
        /* TODO error */
        goto fail_btns_disabed_img_list;
    }
    if (ImageList_Add(dlg->ilButtons, bmpBtns, bmpBtnsMasked) == -1)
    {
        /* TODO error */
        goto fail_fill_btns_img_list;
    }
    if (ImageList_Add(dlg->ilButtonsDisabled, bmpBtnsDisabled,
                      bmpBtnsMasked) == -1)
    {
        /* TODO error */
        goto fail_fill_btns_disabled_img_list;
    }

    DeleteObject(bmpBtns);
    DeleteObject(bmpBtnsMasked);
    DeleteObject(bmpBtnsDisabled);
    /* Set one of the pointers to NULL so the error handling code knows the
    ** bitmaps have been deleted.
    */
    bmpBtns = NULL;

    dlg->toolbar = CreateWindowEx(0,
                                  TOOLBARCLASSNAME,
                                  NULL,
                                  WS_CHILD | WS_VISIBLE | TBSTYLE_FLAT | CCS_LEFT
                                  | CCS_VERT | CCS_NODIVIDER | TBSTYLE_TOOLTIPS,
                                  0,
                                  0,
                                  0,
                                  0,
                                  handle,
                                  NULL,
                                  getPluginInstance(),
                                  NULL);
    if (!dlg->toolbar)
    {
        /* TODO error */
        goto fail_toolbar;
    }

    SendMessage(dlg->toolbar, TB_SETIMAGELIST, 0, (LPARAM) dlg->ilButtons);
    SendMessage(dlg->toolbar, TB_SETDISABLEDIMAGELIST, 0,
                (LPARAM) dlg->ilButtonsDisabled);
    SendMessage(dlg->toolbar, TB_BUTTONSTRUCTSIZE, sizeof(TBBUTTON), 0);
    SendMessage(dlg->toolbar, TB_SETMAXTEXTROWS, 0, 0);

    dlg->padding = HIWORD(SendMessage(dlg->toolbar, TB_GETPADDING, 0, 0));

    addToolbarGap();
    addToolbarBtn(0, ID_RULE_MOVEUP, IDS_TOOLTIP_RULE_MOVEUP),
    addToolbarBtn(1, ID_RULE_MOVEDOWN, IDS_TOOLTIP_RULE_MOVEDOWN),
    addToolbarBtn(2, ID_RULE_ADD, IDS_TOOLTIP_RULE_ADD),
    addToolbarBtn(3, ID_RULE_COPY, IDS_TOOLTIP_RULE_COPY),
    addToolbarBtn(4, ID_RULE_REMOVE, IDS_TOOLTIP_RULE_REMOVE),
    addToolbarBtn(5, ID_RULE_EDIT, IDS_TOOLTIP_RULE_EDIT),

    /* Do not call TB_AUTOSIZE for a vertical toolbar. It seems that because of
    ** TBSTATE_WRAP the width of the toolbar is increased for each added
    ** button although it should remain constant. TB_AUTOSIZE is also not
    ** invoked in the "official" example of vertical toolbars:
    ** https://msdn.microsoft.com/en-us/library/windows/desktop/hh298382(v=vs.85).aspx.
    ** This seems to contradict the information found on
    ** https://msdn.microsoft.com/en-us/library/windows/desktop/hh298381(v=vs.85).aspx:
    ** "You must send the TB_AUTOSIZE message after all the items and strings have been
    ** inserted into the control, to cause the toolbar to recalculate its size based on
    ** its content."
    */

    /* SendMessage(toolbar, TB_AUTOSIZE, 0, 0); */

    dlg->lvRules = GetDlgItem(handle, IDC_LV_RULES);
    dlg->lvRulesProc = (WNDPROC) SetWindowLongPtr(dlg->lvRules, GWLP_WNDPROC,
                                                  (LONG_PTR) &lvRulesProc);

    dlg->btnReset = GetDlgItem(handle, IDC_BT_RESET);
    dlg->btnSave = GetDlgItem(handle, IDC_BT_SAVE);
    dlg->btnClose = GetDlgItem(handle, IDCANCEL);

    addListViewColumns(dlg->lvRules, (ListViewColumn[]) {
        {COL_ENABLED, L"Enabled?"},
        {COL_NAME, L"Name"},
        {COL_EVENT, L"Event"},
        {COL_REGEX, L"Regex"},
        {COL_CMD, L"Command"},
        {COL_BACKGROUND, L"Background?"},
        {-1}
    });

    ListView_SetExtendedListViewStyleEx(dlg->lvRules,
                                        LVS_EX_FULLROWSELECT,
                                        LVS_EX_FULLROWSELECT);

    if (!resetRules())
    {
        /* TODO error */
        goto fail_rules;
    }

    layoutDlg();
    centerWndToParent(handle);

    dlg->initialized = true;

    return;

fail_rules:
fail_toolbar:
fail_fill_btns_disabled_img_list:
fail_fill_btns_img_list:
    ImageList_Destroy(dlg->ilButtonsDisabled);
fail_btns_disabed_img_list:
    ImageList_Destroy(dlg->ilButtons);
fail_btns_img_list:
    if (!bmpBtns)
        goto fail_btns_bmp;

    DeleteObject(bmpBtnsDisabled);
fail_btns_disabled_bmp:
    DeleteObject(bmpBtnsMasked);
fail_btns_masked_bmp:
    DeleteObject(bmpBtns);
fail_btns_bmp:
    EndDialog(handle, -2);
}

void onSize(LONG clientWidth, LONG clientHeight)
{
    RECT lvRulesRc;
    RECT btnSaveRc;
    RECT btnResetRc;
    RECT btnCloseRc;
    LONG offsWidth;
    LONG offsHeight;
    LONG lvRulesWidth;
    LONG lvRulesHeight;
    LONG btnCloseLeft;
    LONG btnCloseTop;

    offsWidth = clientWidth - dlg->clientWidth;
    offsHeight = clientHeight - dlg->clientHeight;

    dlg->clientWidth = clientWidth;
    dlg->clientHeight = clientHeight;

    GetWindowRect(dlg->lvRules, &lvRulesRc);
    GetWindowRect(dlg->btnSave, &btnSaveRc);
    GetWindowRect(dlg->btnReset, &btnResetRc);
    GetWindowRect(dlg->btnClose, &btnCloseRc);
    MapWindowPoints(NULL, dlg->handle, (POINT*) &btnSaveRc, 2);
    MapWindowPoints(NULL, dlg->handle, (POINT*) &btnResetRc, 2);
    MapWindowPoints(NULL, dlg->handle, (POINT*) &btnCloseRc, 2);

    lvRulesWidth = lvRulesRc.right - lvRulesRc.left + offsWidth;
    lvRulesHeight = lvRulesRc.bottom - lvRulesRc.top + offsHeight;
    btnSaveRc.top += offsHeight;
    btnResetRc.top += offsHeight;
    btnCloseLeft = clientWidth / 2
                   - (btnCloseRc.right - btnCloseRc.left) / 2;
    btnCloseTop = btnCloseRc.top + offsHeight;

#define sizeWnd(dlg, width, height) \
    { (dlg), NULL, 0, 0, (width), (height), SWP_NOMOVE | SWP_NOZORDER }

#define positionWnd(dlg, px, py) \
    { (dlg), NULL, (px), (py), 0, 0, SWP_NOSIZE | SWP_NOZORDER }

    setWndPosDeferred((SetWindowPosArgs[]) {
        sizeWnd(dlg->lvRules, lvRulesWidth, lvRulesHeight),
        positionWnd(dlg->btnSave, btnSaveRc.left, btnSaveRc.top),
        positionWnd(dlg->btnReset, btnResetRc.left, btnResetRc.top),
        positionWnd(dlg->btnClose, btnCloseLeft, btnCloseTop),
        {NULL}
    });
}

void onGetDispInfo(NMLVDISPINFO *dispInfo)
{
    LVITEM *item;
    Rule *rule;

    item = &dispInfo->item;
    rule = getRuleAt(dlg->rules, item->iItem);

    switch (item->iSubItem)
    {
    case COL_ENABLED:
        item->pszText = BOOL_TO_STR_YES_NO(rule->enabled);
        break;
    case COL_EVENT:
        item->pszText = (wchar_t*) getEventMapEntry(rule->event)->name;
        break;
    case COL_NAME:
        item->pszText = rule->name;
        break;
    case COL_REGEX:
        item->pszText = rule->regex;
        break;
    case COL_CMD:
        item->pszText = rule->cmd;
        break;
    case COL_BACKGROUND:
        item->pszText = BOOL_TO_STR_YES_NO(rule->background);
        break;
    }
}

void onItemChanged(NMLISTVIEW *info)
{
    UINT selectedCnt;
    bool singleSelected;
    bool firstSelected;
    bool lastSelected;

    selectedCnt = ListView_GetSelectedCount(dlg->lvRules);
    singleSelected = selectedCnt == 1 && (info->uNewState & LVIS_SELECTED);
    firstSelected = singleSelected && !info->iItem;
    lastSelected = singleSelected
                   && ((unsigned int) info->iItem == dlg->ruleCnt - 1);

    setToolbarBtnEnabled(ID_RULE_MOVEUP, singleSelected && !firstSelected);
    setToolbarBtnEnabled(ID_RULE_MOVEDOWN, singleSelected && !lastSelected);
    setToolbarBtnEnabled(ID_RULE_COPY,
                         singleSelected && dlg->ruleCnt < INT_MAX);
    setToolbarBtnEnabled(ID_RULE_REMOVE, selectedCnt);
    setToolbarBtnEnabled(ID_RULE_EDIT, singleSelected);
}

void onReset(void)
{
    int choice;

    choice = msgBox(MB_YESNO,
                    dlg->handle,
                    L"Reset",
                    L"Are you sure you want to revert all changes to the "
                    L"rules?");

    if (choice == IDNO)
        return;

    /* This is used to make sure the toolbar buttons have correct states after
    ** the reset.
    */

    deselectAll();

    if (!resetRules())
    {
        errorMsgBox(dlg->handle,
                    L"Failed to revert the changes made to the rules.");
        return;
    }

    if (dlg->ruleCnt)
        selectRule(0);

    SetFocus(dlg->lvRules);
}

void onSave(void)
{
    if (saveRules())
    {
        SetFocus(dlg->lvRules);
        setModified(false);
    }
    else
        SetActiveWindow(dlg->handle);
}

void onMoveUp(void)
{
    int pos;
    Rule *prev[2];
    Rule *rule;

    assert(BUFLEN(prev) >= 2);

    pos = ListView_GetNextItem(dlg->lvRules, -1, LVNI_SELECTED);

    if (pos == 1)
        prev[0] = dlg->rules;
    else
    {
        prev[1] = getRuleAt(dlg->rules, pos - 2);
        prev[0] = prev[1]->next;
    }

    rule = prev[0]->next;
    if (!(prev[0]->next = rule->next))
        dlg->lastRule = prev[0];
    rule->next = prev[0];

    if (pos == 1)
        dlg->rules = rule;
    else
        prev[1]->next = rule;

    setModified(true);
    deselectRule(pos);
    selectRule(pos - 1);
    ListView_EnsureVisible(dlg->lvRules, pos - 1, FALSE);
    SetFocus(dlg->lvRules);
}

void onMoveDown(void)
{
    int pos;
    Rule *prev;
    Rule *next;
    Rule *rule;

    pos = ListView_GetNextItem(dlg->lvRules, -1, LVNI_SELECTED);

    if (!pos)
    {
        prev = NULL; /* Shut up MSVC... */
        rule = dlg->rules;
    }
    else
    {
        prev = getRuleAt(dlg->rules, pos - 1);
        rule = prev->next;
    }

    next = rule->next;
    if (!(rule->next = next->next))
        dlg->lastRule = rule;
    next->next = rule;

    if (!pos)
        dlg->rules = next;
    else
        prev->next = next;

    setModified(true);
    deselectRule(pos);
    selectRule(pos + 1);
    ListView_EnsureVisible(dlg->lvRules, pos + 1, FALSE);
    SetFocus(dlg->lvRules);
}

void onAdd(void)
{
    Rule template;
    Rule *rule;
    Rule *prev;
    int pos;

    template = (Rule) {
        .name = L"New rule",
        .regex = L".*",
        .cmd = L"Sample command",
        .event = NPPN_FILEBEFORESAVE,
        .enabled = 0,
        .background = 0,
        .next = NULL
    };

    if (!(rule = copyRule(&template)))
    {
        /* TODO error */
        return;
    }

    pos = ListView_GetNextItem(dlg->lvRules, -1, LVNI_SELECTED);

    if (pos >= 0)
    {
        prev = getRuleAt(dlg->rules, pos);

        if (!(rule->next = prev->next))
            dlg->lastRule = rule;

        prev->next = rule;
        pos++;
    }
    else
    {
        if (!dlg->rules)
        {
            dlg->rules = rule;
            dlg->lastRule = dlg->rules;
        }
        else
        {
            dlg->lastRule->next = rule;
            dlg->lastRule = dlg->lastRule->next;
        }

        pos = dlg->ruleCnt;
    }

    if (++dlg->ruleCnt == INT_MAX)
        setToolbarBtnEnabled(ID_RULE_ADD, false);

    setModified(true);
    ListView_SetItemCount(dlg->lvRules, dlg->ruleCnt);
    ListView_EnsureVisible(dlg->lvRules, pos, TRUE);
    deselectAll();
    selectRule(pos);
    SetFocus(dlg->lvRules);
}

void onCopy(void)
{
    int pos;
    Rule *rule;
    Rule *copy;

    pos = ListView_GetNextItem(dlg->lvRules, -1, LVNI_SELECTED);
    rule = getRuleAt(dlg->rules, pos);

    if (!(copy = copyRule(rule)))
    {
        /* TODO error */
        return;
    }

    if (!(copy->next = rule->next))
        dlg->lastRule = copy;

    rule->next = copy;

    if (++dlg->ruleCnt == INT_MAX)
        setToolbarBtnEnabled(ID_RULE_ADD, true);

    setModified(true);
    ListView_SetItemCount(dlg->lvRules, dlg->ruleCnt);
    ListView_EnsureVisible(dlg->lvRules, pos + 1, TRUE);
    deselectRule(pos);
    selectRule(pos + 1);
    SetFocus(dlg->lvRules);
}

void onRemove(void)
{
    Rule *prevRule;
    Rule *currRule;
    Rule *nextRule;
    int prevItem;
    int currItem;
    int focused;
    int removedCnt;

    prevRule = NULL;
    currRule = dlg->rules;
    prevItem = 0;
    currItem = -1;
    removedCnt = 0;

    focused = ListView_GetNextItem(dlg->lvRules, -1, LVIS_FOCUSED);

    while ((currItem = ListView_GetNextItem(dlg->lvRules,
                                            currItem,
                                            LVNI_SELECTED)) != -1)
    {
        for (; prevItem < currItem - removedCnt; prevItem++)
        {
            prevRule = currRule;
            currRule = currRule->next;
        }

        if (prevRule)
        {
            if (!(prevRule->next = currRule->next))
                dlg->lastRule = prevRule;
        }
        else
        {
            if (!(dlg->rules = currRule->next))
                dlg->lastRule = NULL;
        }

        nextRule = currRule->next;
        freeRule(currRule);
        currRule = nextRule;

        /* We need this, because we only update the list view after all
        ** rules have been removed.
        */

        removedCnt++;
    }

    ListView_SetItemCount(dlg->lvRules, dlg->ruleCnt - removedCnt);

    if (dlg->ruleCnt == INT_MAX)
        setToolbarBtnEnabled(ID_RULE_ADD, true);

    dlg->ruleCnt -= removedCnt;

    if (dlg->ruleCnt)
    {
        deselectAll();

        if (focused == -1 || (unsigned int) focused >= dlg->ruleCnt)
            focused = dlg->ruleCnt - 1;

        ListView_SetItemState(dlg->lvRules,
                              focused,
                              LVIS_SELECTED | LVIS_FOCUSED,
                              LVIS_SELECTED | LVIS_FOCUSED);
        ListView_EnsureVisible(dlg->lvRules, focused, FALSE);
    }
    else
    {
        setToolbarBtnEnabled(ID_RULE_MOVEUP, false);
        setToolbarBtnEnabled(ID_RULE_MOVEDOWN, false);
        setToolbarBtnEnabled(ID_RULE_COPY, false);
        setToolbarBtnEnabled(ID_RULE_REMOVE, false);
        setToolbarBtnEnabled(ID_RULE_EDIT, false);
    }

    setModified(true);
    SetFocus(dlg->lvRules);
}

void onEdit(void)
{
    int pos;
    int res;
    Rule *rule;

    pos = ListView_GetNextItem(dlg->lvRules, -1, LVNI_SELECTED);
    rule = getRuleAt(dlg->rules, pos);

    if ((res = openEditDlg(dlg->handle, rule)) < 0)
    {
        /* TODO error */
        errorMsgBox(dlg->handle, L"Failed to open the rule editing dialog.");
        return;
    }

    if (res)
    {
        ListView_Update(dlg->lvRules, pos);
        setModified(true);
    }
}

void addToolbarGap(void)
{
    RECT rc;
    TBBUTTON btn;
    HWND wnd;

    /* The method of adding a gap is inspired by
    ** https://msdn.microsoft.com/en-us/library/windows/desktop/hh298391(v=vs.85).aspx.
    */

    GetWindowRect(dlg->toolbar, &rc);

    wnd = CreateWindowEx(0L,
                         L"Static",
                         NULL,
                         WS_CHILD | WS_VISIBLE,
                         0,
                         0,
                         rc.right - rc.left,
                         dlg->padding,
                         dlg->toolbar,
                         (HMENU) IDC_STATIC,
                         getPluginInstance(),
                         0);

    if (!wnd)
    {
        /* TODO warning, but don't consider it an error since having no gap is
        ** not critical.
        */
        return;
    }

    ZeroMemory(&btn, sizeof(TBBUTTON));
    btn.iBitmap = dlg->padding;
    btn.fsState = TBSTATE_ENABLED | TBSTATE_WRAP;
    btn.fsStyle = TBSTYLE_SEP;
    SendMessage(dlg->toolbar, TB_ADDBUTTONS, 1, (LPARAM) &btn);
}

void addToolbarBtn(int bmpIndex, int cmdId, int strId)
{
    TBBUTTON btn;
    LRESULT strIndex;

    strIndex = SendMessage(dlg->toolbar, TB_ADDSTRINGW,
                           (WPARAM) getPluginInstance(), strId);
    if (strIndex == -1)
    {
        /* TODO warning */
    }

    ZeroMemory(&btn, sizeof(TBBUTTON));
    btn.iBitmap = bmpIndex;
    btn.idCommand = cmdId;
    btn.fsState = TBSTATE_WRAP;
    btn.fsStyle = TBSTYLE_AUTOSIZE;
    /* The wonders of the Win32 API, pointers and numbers intermixed freely... */
    btn.iString = strIndex != -1 ? strIndex : (INT_PTR) NULL;
    SendMessage(dlg->toolbar, TB_ADDBUTTONS, 1, (LPARAM) &btn);
}

void setToolbarBtnEnabled(int btnCmd, bool enabled)
{
    LRESULT state;

    state = SendMessage(dlg->toolbar, TB_GETSTATE, (WPARAM) btnCmd, 0);

    if (enabled)
        state |= TBSTATE_ENABLED;
    else
        state &= ~TBSTATE_ENABLED;

    SendMessage(dlg->toolbar, TB_SETSTATE, (WPARAM) btnCmd, (LPARAM) state);
}

void layoutDlg(void)
{
    RECT dlgRc;
    RECT clientRc;
    RECT toolbarRc;
    LONG toolbarWidth;

    GetWindowRect(dlg->handle, &dlgRc);
    GetClientRect(dlg->handle, &clientRc);
    GetWindowRect(dlg->toolbar, &toolbarRc);
    toolbarWidth = toolbarRc.right - toolbarRc.left;
    dlg->clientWidth = clientRc.right + dlg->padding + toolbarWidth;
    dlg->clientHeight = clientRc.bottom + dlg->padding;
    dlg->minWidth = dlgRc.right - dlgRc.left + dlg->padding + toolbarWidth;
    dlg->minHeight = dlgRc.bottom - dlgRc.top + dlg->padding;

    setWndSize(dlg->handle, dlg->minWidth, dlg->minHeight);
    EnumChildWindows(dlg->handle, layoutDlgProc, toolbarWidth);

    sizeListViewColumns(dlg->lvRules, (ListViewColumnSize[]) {
        {COL_ENABLED, 0.08},
        {COL_NAME, 0.20},
        {COL_EVENT, 0.20},
        {COL_REGEX, 0.20},
        {COL_CMD, 0.24},
        {COL_BACKGROUND, 0.08},
        {-1}
    });
}

BOOL CALLBACK layoutDlgProc(HWND wnd, LPARAM lp)
{
    RECT rc;

    GetWindowRect(wnd, &rc);

    switch (GetWindowLongPtr(wnd, GWLP_ID))
    {
    case IDC_LV_RULES:
    case IDC_BT_SAVE:
    case IDC_BT_RESET:
        MapWindowPoints(NULL, dlg->handle, (POINT*) &rc.left, 1);
        setWndPos(wnd, rc.left + (int) lp, rc.top + dlg->padding);
        break;
    case IDCANCEL:
        MapWindowPoints(NULL, dlg->handle, (POINT*) &rc, 2);
        setWndPos(wnd, dlg->clientWidth / 2 - (rc.right - rc.left) / 2, rc.top);
        break;
    }

    return TRUE;
}

LRESULT CALLBACK lvRulesProc(HWND wnd, UINT msg, WPARAM wp, LPARAM lp)
{
    if (msg == WM_GETDLGCODE && wp == VK_RETURN)
        return DLGC_WANTALLKEYS;

    return CallWindowProc(dlg->lvRulesProc, wnd, msg, wp, lp);
}

bool resetRules(void)
{
    Rule *copiedRules;

    if (copyRules(*dlg->activeRules, &copiedRules, &dlg->lastRule))
    {
        /* TODO error */
        return false;
    }

    freeRules(dlg->rules);
    dlg->rules = copiedRules;
    dlg->ruleCnt = getRuleCount(dlg->rules);

    ListView_SetItemCount(dlg->lvRules, dlg->ruleCnt);
    setToolbarBtnEnabled(ID_RULE_ADD, dlg->ruleCnt < INT_MAX);
    EnableWindow(dlg->btnReset, FALSE);
    EnableWindow(dlg->btnSave, FALSE);
    setModified(false);

    return true;
}

bool saveRules(void)
{
    Rule *rules;
    Rule *lastRule;
    INT_PTR res;

    if (!isQueueEmpty())
    {
        if ((res = openQueueDlg(dlg->handle, QDLR_SAVING_CHANGES)) == -1)
        {
            /* TODO error */
            goto fail_dlg;
        }

        if (res == 1)
            return false;
    }

    if (copyRules(dlg->rules, &rules, &lastRule))
    {
        /* TODO error */
        goto fail_copy;
    }

    if (writeRules(dlg->rules))
    {
        /* TODO error */
        goto fail_write;
    }

    freeRules(*dlg->activeRules);
    *dlg->activeRules = rules;

    return true;

fail_write:
    freeRules(rules);
fail_copy:
fail_dlg:
    errorMsgBox(dlg->handle,
                L"Failed to save the changes to the rule list.");

    return false;
}

void deselectAll(void)
{
    ListView_SetItemState(dlg->lvRules, -1, 0, LVIS_SELECTED | LVIS_FOCUSED);
}

void deselectRule(int pos)
{
    ListView_SetItemState(dlg->lvRules, pos, 0, LVIS_SELECTED | LVIS_FOCUSED);
}

void selectRule(int pos)
{
    ListView_SetItemState(dlg->lvRules,
                          pos,
                          LVIS_SELECTED | LVIS_FOCUSED,
                          LVIS_SELECTED | LVIS_FOCUSED);
}

void setModified(bool modified)
{
    wchar_t *title;

    if (!dlg->initialized || dlg->modified != modified)
    {
        dlg->modified = modified;
        title = modified ? DLG_TITLE L" (modified)" : DLG_TITLE;

        SetWindowTextW(dlg->handle, title);
        EnableWindow(dlg->btnReset, modified);
        EnableWindow(dlg->btnSave, modified);
    }
}

void askToSaveChanges(void)
{
    int choice;

    if (!dlg->modified)
    {
        EndDialog(dlg->handle, IDCANCEL);
        return;
    }

    choice = msgBox(MB_YESNOCANCEL | MB_ICONQUESTION,
                    dlg->handle,
                    L"Unsaved changes",
                    L"Do you wish to save the changes made to the rule list "
                    L"before closing?");

    switch (choice)
    {
    case IDYES:

        if (saveRules())
            EndDialog(dlg->handle, IDCANCEL);

        break;
    case IDNO:
        EndDialog(dlg->handle, IDCANCEL);
        break;

    default:
        /* Do nothing. */
        break;
    }
}
