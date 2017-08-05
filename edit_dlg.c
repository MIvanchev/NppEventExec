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
#include "util.h"

/** TODO doc */
#define COLOR_ERROR RGB(255, 190, 190)

/** TODO doc */
#define ERROR_LABEL_HEIGHT_PLUS_PADDING 10

/** TODO doc */
#define ERR_MSG_EMPTY_VAL L"The value cannot be empty."

/** TODO doc */
#define ERR_MSG_LEADING_SPACES L"The value cannot begin with spaces."

/** TODO doc */
#define ERR_MSG_TRAILING_SPACES L"The value cannot end with spaces."

/** TODO doc */
#define ERR_MSG_INVALID_REGEX L"The value is not a valid regular expression."

typedef wchar_t* (*ValidateProc)(const wchar_t*);

typedef struct
{
    HDWP hdwp;
    LONG offsName;
    LONG offsRegex;
    LONG offsCmd;
} LayoutDlgData;

typedef enum
{
    VAL_UNCHANGED,
    VAL_CHANGED,
    VAL_VALID,
    VAL_INVALID,
} InputCtrlStatus;

typedef struct sInputCtrl
{
    HWND handle;
    HWND label;
    HWND labelError;
    InputCtrlStatus status;
    bool updated;
    ValidateProc validateProc;
    wchar_t *value;
    wchar_t **target;
    struct sInputCtrl *next;
} InputCtrl;

typedef struct
{
    HWND handle;
    InputCtrl ctrlName;
    InputCtrl ctrlRegex;
    InputCtrl ctrlCmd;
    InputCtrl *ctrls;
    HWND lblEvent;
    HWND cbEvent;
    HWND btnEnabled;
    HWND btnForeground;
    HWND btnApply;
    HWND btnCancel;
    HBRUSH errBrush;
    HFONT errFont;
    size_t invalidCnt;
    bool applied;
    LONG minWidth;
    LONG clientWidth;
    LONG height;
    Rule *rule;
    bool initialized;
} Dialog;

static INT_PTR CALLBACK dlgProc(HWND handle, UINT msg, WPARAM wp, LPARAM lp);
static void onInitDlg(HWND handle);
static void onDestroy(void);
static void onSize(WORD clientWidth);
static void onGetMinMaxInfo(MINMAXINFO *mmi);
static LRESULT onCtlColorEdit(HWND handle, UINT msg, WPARAM wp, LPARAM lp);
static void onEnChange(WPARAM wp, LPARAM lp);
static void onSelChange(void);
static void onBnClicked(void);
static void onClose(void);
static void onApply(void);
static void initCtrl(InputCtrl *ctrl, int labelId, int ctrlId, int labelErrorId,
                     wchar_t **target, ValidateProc validateProc);
static bool createErrorFont(void);
static void layoutDlg(void);
static BOOL CALLBACK layoutDlgProc(HWND wnd, LPARAM lp);
static int validateChanges(void);
static bool extractCtrlContents(InputCtrl *ctrl);
static void applyChanges(void);
static bool validateAndApplyChanges(void);
static void setChangesApplicable(bool applicable);
static bool areChangesApplicable(void);
static wchar_t* validateName(const wchar_t *val);
static wchar_t* validateRegex(const wchar_t *val);
static wchar_t* validateCmd(const wchar_t *val);

static Dialog *dlg;

int openEditDlg(HWND parent, Rule *rule)
{
    INT_PTR res;

    assert(parent);
    assert(rule);

    if (!(dlg = allocMem(sizeof *dlg)))
    {
        /* TODO error */
        return -1;
    }

    dlg->rule = rule;

    res = DialogBox(getPluginInstance(), MAKEINTRESOURCE(IDD_EDIT),
                    parent, dlgProc);

    freeMem(dlg);
    dlg = NULL;

    if (res <= 0)
    {
        /* TODO error */
        return -1;
    }

    return res == IDC_BT_APPLY;
}

INT_PTR CALLBACK dlgProc(HWND handle, UINT msg, WPARAM wp, LPARAM lp)
{
    switch (msg)
    {
    case WM_INITDIALOG:
        onInitDlg(handle);
        return TRUE;

    case WM_DESTROY:
        onDestroy();
        DLGPROC_RESULT(handle, 0);

    case WM_CTLCOLOREDIT:
        return onCtlColorEdit(handle, msg, wp, lp);

    case WM_GETMINMAXINFO:
        onGetMinMaxInfo((MINMAXINFO*) lp);
        DLGPROC_RESULT(handle, 0);

    case WM_SIZE:
        onSize(LOWORD(lp));
        DLGPROC_RESULT(handle, 0);

    case WM_CLOSE:
        onClose();
        DLGPROC_RESULT(handle, 0);

    case WM_SYSCOMMAND:
        if (wp == SC_CLOSE)
        {
            onClose();
            DLGPROC_RESULT(handle, 0);
        }
        break;

    case WM_COMMAND:
        switch (LOWORD(wp))
        {
        case IDCANCEL:
            onClose();
            DLGPROC_RESULT(handle, 0);
        case IDC_BT_APPLY:
            onApply();
            DLGPROC_RESULT(handle, 0);
        }

        switch (HIWORD(wp))
        {
        case EN_CHANGE:
            /* We check here for the dialog's initialization, because setting
            ** the control's initial value also triggers this notification.
            */

            if (dlg->initialized)
                onEnChange(wp, lp);

            DLGPROC_RESULT(handle, 0);
        case CBN_SELCHANGE:
            onSelChange();
            DLGPROC_RESULT(handle, 0);
        case BN_CLICKED:
            onBnClicked();
            DLGPROC_RESULT(handle, 0);
        }

        break;
    } /* switch (msg) */

    return FALSE;
}

void onInitDlg(HWND handle)
{
    RECT rc;
    Rule *rule;
    size_t eventIndex;
    unsigned int ii;

    dlg->handle = handle;
    dlg->initialized = false;

    if (!(dlg->errBrush = CreateSolidBrush(COLOR_ERROR)))
    {
        /* TODO error */
        goto fail_err_brush;
    }
    if (!createErrorFont())
    {
        /* TODO error */
        goto fail_err_font;
    }

    rule = dlg->rule;

    dlg->ctrls = &dlg->ctrlName;
    dlg->ctrlName.next = &dlg->ctrlRegex;
    dlg->ctrlRegex.next = &dlg->ctrlCmd;
    dlg->ctrlCmd.next = NULL;

    initCtrl(&dlg->ctrlName, IDC_ST_NAME, IDC_ED_NAME, IDC_ST_NAME_ERROR,
             &rule->name, validateName);
    initCtrl(&dlg->ctrlRegex, IDC_ST_REGEX, IDC_ED_REGEX, IDC_ST_REGEX_ERROR,
             &rule->regex, validateRegex);
    initCtrl(&dlg->ctrlCmd,
             IDC_ST_COMMAND,
             IDC_ED_COMMAND,
             IDC_ST_COMMAND_ERROR,
             &rule->cmd,
             validateCmd);

    dlg->lblEvent = GetDlgItem(handle, IDC_ST_EVENT);
    dlg->cbEvent = GetDlgItem(handle, IDC_CB_EVENT);
    dlg->btnEnabled = GetDlgItem(handle, IDC_BT_ENABLED);
    dlg->btnForeground = GetDlgItem(handle, IDC_BT_FOREGROUND);
    dlg->btnApply = GetDlgItem(handle, IDC_BT_APPLY);
    dlg->btnCancel = GetDlgItem(handle, IDCANCEL);

    /* The events are already validated on loading. */

    getEventMapEntryIndex(rule->event, &eventIndex);

    if (eventMapSize <= INT_MAX)
    {
        for (ii = 0; ii < eventMapSize; ii++)
            ComboBox_AddString(dlg->cbEvent, eventMap[ii].name);

        ComboBox_SetCurSel(dlg->cbEvent, eventIndex);
    }
    else
    {
        /* TODO warning */
        ComboBox_AddString(dlg->cbEvent, eventMap[eventIndex].name);
        ComboBox_SetCurSel(dlg->cbEvent, 0);
        EnableWindow(dlg->cbEvent, FALSE);
    }

    Button_SetCheck(dlg->btnEnabled, rule->enabled);
    Button_SetCheck(dlg->btnForeground, !rule->background);

    setChangesApplicable(false);

    dlg->invalidCnt = 0;
    dlg->applied = true;

    GetWindowRect(handle, &rc);
    dlg->minWidth = rc.right - rc.left;
    GetClientRect(handle, &rc);
    dlg->clientWidth = rc.right - rc.left;

    layoutDlg();
    centerWndToParent(handle);

    dlg->initialized = true;

    return;

fail_err_font:
    DeleteObject(dlg->errBrush);
fail_err_brush:
    EndDialog(handle, -2);
}

void onDestroy(void)
{
    InputCtrl *ctrl;

    if (dlg->initialized)
    {
        for (ctrl = dlg->ctrls; ctrl; ctrl = ctrl->next)
            freeStr(ctrl->value);

        DeleteObject(dlg->errFont);
        DeleteObject(dlg->errBrush);
    }
}

void onSize(WORD clientWidth)
{
    LONG offs;
    RECT rcApply;
    RECT rcCancel;
    RECT rcInput;
    LONG buttonWidth;
    LONG buttonsWidthWithMargin;
    LONG inputWidth;
    LONG inputHeight;
    LONG btnApplyLeft;
    LONG btnCancelLeft;

    offs = clientWidth - dlg->clientWidth;

    GetWindowRect(dlg->btnApply, &rcApply);
    GetWindowRect(dlg->btnCancel, &rcCancel);
    GetWindowRect(dlg->ctrlName.handle, &rcInput);

    buttonsWidthWithMargin = rcCancel.right - rcApply.left;
    buttonWidth = rcApply.right - rcApply.left;
    inputWidth = rcInput.right - rcInput.left + offs;
    inputHeight = rcInput.bottom - rcInput.top;

    MapWindowPoints(NULL, dlg->handle, (POINT*) &rcApply, 2);

    btnApplyLeft = clientWidth / 2 - buttonsWidthWithMargin / 2;
    btnCancelLeft = btnApplyLeft + buttonsWidthWithMargin - buttonWidth;

#define sizeWnd(dlg, width, height) \
    { (dlg), NULL, 0, 0, (width), (height), SWP_NOMOVE | SWP_NOZORDER }

#define positionWnd(dlg, px, py) \
    { (dlg), NULL, (px), (py), 0, 0, SWP_NOSIZE | SWP_NOZORDER }

    setWndPosDeferred((SetWindowPosArgs[]) {
        sizeWnd(dlg->ctrlName.handle, inputWidth, inputHeight),
        sizeWnd(dlg->ctrlRegex.handle, inputWidth, inputHeight),
        sizeWnd(dlg->ctrlCmd.handle, inputWidth, inputHeight),
        positionWnd(dlg->btnApply, btnApplyLeft, rcApply.top),
        positionWnd(dlg->btnCancel, btnCancelLeft, rcApply.top),
        {NULL}
    });

    dlg->clientWidth = clientWidth;
}

void onGetMinMaxInfo(MINMAXINFO *mmi)
{
    mmi->ptMinTrackSize.x = dlg->minWidth;
    mmi->ptMinTrackSize.y = dlg->height;
    mmi->ptMaxTrackSize.y = mmi->ptMinTrackSize.y;
}

LRESULT onCtlColorEdit(HWND handle, UINT msg, WPARAM wp, LPARAM lp)
{
    InputCtrl *ctrl;

    for (ctrl = dlg->ctrls; ctrl; ctrl = ctrl->next)
    {
        if (ctrl->handle == (HWND) lp)
        {
            if (ctrl->status == VAL_INVALID)
            {
                SetBkColor((HDC) wp, COLOR_ERROR);
                return (LRESULT) dlg->errBrush;
            }

            break;
        }
    }

    return DefWindowProc(dlg->handle, msg, wp, lp);
}

void onEnChange(WPARAM wp, LPARAM lp)
{
    InputCtrl *ctrl;
    InputCtrlStatus prevStatus;

    for (ctrl = dlg->ctrls; ctrl; ctrl = ctrl->next)
    {
        if (ctrl->handle == (HWND) lp)
        {
            prevStatus = ctrl->status;
            ctrl->status = VAL_CHANGED;

            switch (prevStatus)
            {
            case VAL_VALID:
                freeStr(ctrl->value);
                ctrl->value = NULL;
                break;
            case VAL_INVALID:
                ctrl->updated = false;
                SetWindowTextW(ctrl->labelError, L"");
                ShowWindow(ctrl->labelError, SW_HIDE);
                dlg->invalidCnt--;
                layoutDlg();
                break;
            default:
                break;
            }

            break;
        }
    }

    dlg->applied = false;

    if (!dlg->invalidCnt && !areChangesApplicable())
        setChangesApplicable(true);
}

void onSelChange(void)
{
    dlg->applied = false;

    if (!dlg->invalidCnt && !areChangesApplicable())
        setChangesApplicable(true);
}

void onBnClicked(void)
{
    dlg->applied = false;

    if (!dlg->invalidCnt && !areChangesApplicable())
        setChangesApplicable(true);
}

void onClose(void)
{
    int choice;

    if (dlg->applied)
    {
        EndDialog(dlg->handle, IDCANCEL);
        return;
    }

    if (dlg->invalidCnt)
    {
        choice = msgBox(MB_OKCANCEL | MB_ICONQUESTION,
                        dlg->handle,
                        L"Rule modified",
                        L"The changes contain errors; Click OK to leave the rule unchanged and close or click Cancel to fix the errors.");
    }
    else
    {
        choice = msgBox(MB_YESNOCANCEL | MB_ICONQUESTION,
                        dlg->handle,
                        L"Rule modified",
                        L"Do you wish to apply the changes before closing?");
    }

    switch (choice)
    {
    case IDOK:
        EndDialog(dlg->handle, IDCANCEL);
        break;

    case IDYES:

        if (validateAndApplyChanges())
            EndDialog(dlg->handle, IDC_BT_APPLY);

        break;
    case IDNO:
        EndDialog(dlg->handle, IDCANCEL);
        break;

    default:
        /* Do nothing. */
        break;
    }
}

void onApply(void)
{
    if (validateAndApplyChanges())
        EndDialog(dlg->handle, IDC_BT_APPLY);
}

void initCtrl(InputCtrl *ctrl,
              int labelId,
              int ctrlId,
              int labelErrorId,
              wchar_t **target,
              ValidateProc validateProc)
{
    assert(ctrl);
    assert(target);
    assert(validateProc);

    ctrl->handle = GetDlgItem(dlg->handle, ctrlId);
    ctrl->label = GetDlgItem(dlg->handle, labelId);
    ctrl->labelError = GetDlgItem(dlg->handle, labelErrorId);
    ctrl->target = target;
    ctrl->validateProc = validateProc;
    ctrl->status = VAL_UNCHANGED;
    ctrl->updated = false;
    ctrl->value = NULL;

    SendMessage(ctrl->labelError,
                WM_SETFONT,
                (WPARAM) dlg->errFont,
                (LPARAM) MAKELPARAM(TRUE, 0));
    SetWindowTextW(ctrl->handle, *target);
    SetWindowTextW(ctrl->labelError, L"");
    ShowWindow(ctrl->labelError, SW_HIDE);
}

bool createErrorFont(void)
{
    LOGFONT lf;
    HFONT font;

    font = (HFONT) SendMessage(dlg->handle, WM_GETFONT, 0, 0);
    GetObject(font, sizeof lf, &lf);
    lf.lfItalic = TRUE;

    if (!(dlg->errFont = CreateFontIndirect(&lf)))
    {
        /* TODO error */
        return false;
    }

    return true;
}

void layoutDlg(void)
{
    RECT rc;
    LONG dlgWidth;
    LONG dlgHeight;
    int count;
    LayoutDlgData data;

    /* Calculate the control offsets because of the error labels. */

    rc.left = 0;
    rc.top = ERROR_LABEL_HEIGHT_PLUS_PADDING;
    rc.right = 0;
    rc.bottom = 0;
    MapDialogRect(dlg->handle, &rc);

    /* I'm sorry for the mess, mama ;( */

#define INPUT_CTRL_OFFSET(ctrl)                \
    (!(ctrl).updated ? ((ctrl).updated = true, \
                        (rc.top                \
                         * ((ctrl).status == VAL_INVALID ? 1 : -1))) : 0)

    data.offsName = INPUT_CTRL_OFFSET(dlg->ctrlName);
    data.offsRegex = INPUT_CTRL_OFFSET(dlg->ctrlRegex);
    data.offsCmd = INPUT_CTRL_OFFSET(dlg->ctrlCmd);
    data.hdwp = NULL; /* Important for the error handling. */

    /* Calculate the new height of the dialog and resize it. */

    GetWindowRect(dlg->handle, &rc);
    dlgWidth = rc.right - rc.left;
    dlgHeight = rc.bottom - rc.top;
    dlg->height = dlgHeight + data.offsName + data.offsRegex + data.offsCmd;

    SetWindowPos(dlg->handle,
                 NULL,
                 0,
                 0,
                 dlgWidth,
                 dlgHeight,
                 SWP_NOMOVE | SWP_NOZORDER);

    InvalidateRect(dlg->handle, NULL, TRUE);

    /* Reposition the controls and resize the dialog. */

    if ((count = getChildWndCount(dlg->handle)) < 0)
    {
        /* TODO error */
        goto fail_count;
    }

    if (!(data.hdwp = BeginDeferWindowPos(count)))
    {
        /* TODO error */
        goto fail_defer;
    }

    EnumChildWindows(dlg->handle, layoutDlgProc, (LPARAM) &data);

    if (!data.hdwp)
    {
        /* TODO error */
        goto fail_defer_pos;
    }

    EndDeferWindowPos(data.hdwp);

    return;

fail_defer_pos:
fail_defer:
fail_count:

    EnumChildWindows(dlg->handle, layoutDlgProc, (LPARAM) &data);
}

BOOL CALLBACK layoutDlgProc(HWND wnd, LPARAM lp)
{
    RECT rc;
    LayoutDlgData *data;

    data = (LayoutDlgData*) lp;

    GetWindowRect(wnd, &rc);
    MapWindowPoints(NULL, dlg->handle, (POINT*) &rc, 2);

    switch (GetWindowLongPtr(wnd, GWLP_ID))
    {
    case IDC_ST_EVENT:
    case IDC_CB_EVENT:
    case IDC_ST_REGEX:
    case IDC_ED_REGEX:
    case IDC_ST_REGEX_ERROR:
        rc.top += data->offsName;
        break;
    case IDC_ST_COMMAND:
    case IDC_ED_COMMAND:
    case IDC_ST_COMMAND_ERROR:
        rc.top += data->offsName + data->offsRegex;
        break;
    case IDC_BT_ENABLED:
    case IDC_BT_FOREGROUND:
    case IDC_BT_APPLY:
    case IDCANCEL:
        rc.top += data->offsName + data->offsRegex + data->offsCmd;
        break;
    }

    if (!data->hdwp)
        SetWindowPos(wnd, NULL, rc.left, rc.top, 0, 0, SWP_NOSIZE);
    else if (!(data->hdwp = DeferWindowPos(data->hdwp,
                                           wnd,
                                           NULL,
                                           rc.left,
                                           rc.top,
                                           0,
                                           0,
                                           SWP_NOSIZE | SWP_NOZORDER)))
    {
        /* TODO error */
        return FALSE;
    }

    return TRUE;
}

int validateChanges(void)
{
    InputCtrl *ctrl;
    InputCtrl *prev;
    wchar_t *errMsg;
    wchar_t buf[1024];

    assert(!dlg->invalidCnt);

    for (ctrl = dlg->ctrls; ctrl; ctrl = ctrl->next)
    {
        if (ctrl->status == VAL_CHANGED)
        {
            /* In case the contents of the control cannot be extracted all
            ** invalid controls are marked as altered and no error labels are
            ** shown. This implies that they will be revalidated the next time
            ** the changes are applied. Valid and unprocessed controls are left
            ** as they are.
            */

            if (!extractCtrlContents(ctrl))
            {
                for (prev = dlg->ctrls; prev != ctrl; prev = prev->next)
                {
                    if (prev->status == VAL_INVALID)
                    {
                        prev->status = VAL_CHANGED;
                        prev->updated = true;
                        SetWindowTextW(prev->labelError, L"");
                        ShowWindow(prev->labelError, SW_HIDE);
                    }
                }

                dlg->invalidCnt = 0;

                /* TODO error */
                return -1;
            }

            if ((errMsg = ctrl->validateProc(ctrl->value)))
            {
                freeStr(ctrl->value);
                ctrl->value = NULL;
                ctrl->status  = VAL_INVALID;
                ctrl->updated = false;

                StringCbCopyW(buf, sizeof buf, L"^ ");
                StringCbCatW(buf, sizeof buf, errMsg);
                SetWindowTextW(ctrl->labelError, buf);
                ShowWindow(ctrl->labelError, SW_SHOW);
                dlg->invalidCnt++;
            }
            else
                ctrl->status = VAL_VALID;
        }
    }

    if (dlg->invalidCnt)
        layoutDlg();

    return dlg->invalidCnt > 0;
}

bool extractCtrlContents(InputCtrl *ctrl)
{
    unsigned int len;

    assert(ctrl->status == VAL_CHANGED);
    assert(!ctrl->value);

    len = GetWindowTextLengthW(ctrl->handle);

    if (len > SIZE_MAX - 1)
    {
        /* TODO error */
        return false;
    }

    if (!(ctrl->value = allocStr(len + 1)))
    {
        /* TODO error */
        return false;
    }

    GetWindowTextW(ctrl->handle, ctrl->value, len + 1);

    return true;
}

void applyChanges(void)
{
    Rule *rule;
    InputCtrl *ctrl;

    assert(!dlg->invalidCnt);
    assert(!dlg->applied);

    rule = dlg->rule;

    for (ctrl = dlg->ctrls; ctrl; ctrl = ctrl->next)
    {
        if (ctrl->value)
        {
            freeStr(*ctrl->target);
            *ctrl->target = ctrl->value;
            ctrl->value = NULL;
            ctrl->status = VAL_UNCHANGED;
        }
    }

    if (IsWindowEnabled(dlg->cbEvent))
        rule->event = eventMap[ComboBox_GetCurSel(dlg->cbEvent)].event;

    rule->enabled = Button_GetCheck(dlg->btnEnabled) == BST_CHECKED;
    rule->background = Button_GetCheck(dlg->btnForeground) != BST_CHECKED;
}

bool validateAndApplyChanges(void)
{
    int res;

    assert(!dlg->applied);

    if ((res = validateChanges()))
    {
        if (res < 0)
        {
            /* TODO error */
            errorMsgBox(dlg->handle,
                        L"Failed to validate the changes made to the rule.");
        }
        else
            setChangesApplicable(false);

        return false;
    }

    applyChanges();

    return true;
}

void setChangesApplicable(bool applicable)
{
    EnableWindow(dlg->btnApply, applicable);
}

bool areChangesApplicable(void)
{
    return IsWindowEnabled(dlg->btnApply);
}

wchar_t* validateName(const wchar_t *val)
{
    assert(val);

    if (*val == L'\0')
        return ERR_MSG_EMPTY_VAL;
    if (IS_SPACE(*val))
        return ERR_MSG_LEADING_SPACES;
    if (IS_SPACE(val[wcslen(val) - 1]))
        return ERR_MSG_TRAILING_SPACES;

    return NULL;
}

wchar_t* validateRegex(const wchar_t *val)
{
    assert(val);

    return !isValidRegex(val) ? ERR_MSG_INVALID_REGEX : NULL;
}

wchar_t* validateCmd(const wchar_t *val)
{
    assert(val);

    if (*val == L'\0')
        return ERR_MSG_EMPTY_VAL;
    if (IS_SPACE(*val))
        return ERR_MSG_LEADING_SPACES;

    /* Oddly enough, trailing spaces are accepted by NppExec. */

    return NULL;
}
