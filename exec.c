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
#include "rule.h"
#include "Scintilla.h"
#include "exec.h"
#include "resource.h"
#include "util.h"
#include "Notepad_plus_msgs.h"
#include "nppexec_msgs.h"

/** TODO */
#define UPDATE_INTERVAL_IN_MS 100

/** TODO */
#define DLG_MIN_WIDTH 480

/** TODO */
#define DLG_MIN_HEIGHT 320

/** TODO */
#define PLACEHOLDER L"<n/a>";

typedef enum
{
    STATE_QUEUED,
    STATE_WAITING,
    STATE_EXECUTING
} RuleExecState;

typedef struct _RuleExec
{
    const Rule *rule;
    wchar_t *args;
    uptr_t bufferId;
    RuleExecState state;
    struct _RuleExec *next;
} RuleExec;

typedef enum
{
    COL_POS,
    COL_RULE,
    COL_STATE,
    COL_PATH,
    COL_BACKGROUND
} Column;

typedef struct _ColumnData
{
    RuleExec *exec;
    wchar_t *pos;
    wchar_t *path;
    struct _ColumnData *next;
} ColumnData;

typedef struct
{
    HWND handle;
    HWND lvQueue;
    HWND btnMode;
    HWND btnAbort;
    HWND btnClose;
    int clientW;
    int clientH;
    ColumnData *colDataFirst;
    ColumnData *colDataLast;
    bool userAction;
} Dialog;

static wchar_t* createArgs(uptr_t bufferId, const wchar_t *path);
static bool launchQueueDlg(bool userAction);
static void removeExec(RuleExec *prevExec, RuleExec *exec);
static RuleExec* getExecAt(int pos);
static void CALLBACK timerProc(HWND wnd, UINT msg, UINT timerId, DWORD sysTime);
static void updateQueue(void);
static void layoutDlg(void);
static void setCloseable(bool closeable);
static BOOL CALLBACK dlgProc(HWND dlg, UINT msg, WPARAM wp, LPARAM lp);
static void removeColData(RuleExec *exec);
static void addColData(RuleExec *exec, int execPos);
static ColumnData* getColumnData(RuleExec *exec);
static void onInitDlg(HWND dlg);
static void onSize(int newW, int newH);
static void onGetDispInfo(NMLVDISPINFO *dispInfo);
static void onAbort(void);
static void setAbortBtnState(void);

static Dialog *queueDlg;
static RuleExec *execs;
static RuleExec *lastExec;
static int numExecs;
static int numForeground;
static UINT timerId;

int execRule(uptr_t bufferId, const wchar_t *path, const Rule *rule)
{
    RuleExec *exec;
    RuleExec *prevExec;

    if (numExecs == INT_MAX)
    {
        /* TODO error */
        goto fail_too_many_execs;
    }
    if (!(exec = allocMem(sizeof(RuleExec))))
    {
        /* TODO error */
        goto fail_alloc;
    }
    if (!(exec->args = createArgs(bufferId, path)))
    {
        /* TODO error */
        goto fail_args;
    }

    exec->bufferId = bufferId;
    exec->rule = rule;
    exec->state = STATE_QUEUED;
    exec->next = NULL;

    if (!execs)
    {
        if (!(timerId = SetTimer(NULL, 0, UPDATE_INTERVAL_IN_MS, timerProc)))
        {
            /* TODO error */
            goto fail_timer;
        }

        execs = exec;
        lastExec = exec;
        /* Get rid of a 'maybe uninitialized warning. */
        prevExec = NULL;
    }
    else
    {
        prevExec = lastExec;
        lastExec->next = exec;
        lastExec = exec;
    }

    numExecs++;
    numForeground += !rule->background;

    if (queueDlg)
    {
        addColData(exec, numExecs);
        ListView_SetItemCount(queueDlg->lvQueue, numExecs);
        if (numForeground == 1 && !rule->background)
            setCloseable(false);
    }
    else if (!rule->background && !launchQueueDlg(false))
    {
        /* TODO launch dialog */
        goto fail_dlg;
    }

    return 0;

fail_dlg:
    numForeground--;
    numExecs--;

    if (!(lastExec = prevExec))
        execs = NULL;

fail_timer:
    freeStr(exec->args);
fail_args:
    freeMem(exec);
fail_alloc:
fail_too_many_execs:
    return 1;
}

int openQueueDlg(void)
{
    return !launchQueueDlg(true);
}

wchar_t* createArgs(uptr_t bufferId, const wchar_t *path)
{
    wchar_t *args;
    uptr_t numDigits;
    uptr_t temp;
    size_t pathLen;
    size_t size;

    numDigits = 0;

    for (temp = bufferId; temp; temp /= 10)
        numDigits++;

    pathLen = wcslen(path);

    /* The initial size accounts for 2 pairs of quotes, 1 space and the '\0'. */

    size = 6;

    if (numDigits > SIZE_MAX - size || pathLen > SIZE_MAX - size - numDigits)
    {
        /* TODO error */
        goto fail_args_too_long;
    }

    size += numDigits + pathLen;

    if (!(args = allocStr(size)))
    {
        /* TODO error */
        goto fail_alloc;
    }

    StringCchPrintfW(args, size, L"\"%ld\" \"%ls\"", (long) bufferId, path);

    return args;

fail_alloc:
fail_args_too_long:

    return NULL;
}

bool launchQueueDlg(bool userAction)
{
    int res;

    if (!(queueDlg = allocMem(sizeof(Dialog))))
    {
        /* TODO error */
        goto fail_mem;
    }

    queueDlg->userAction = userAction;

    res = DialogBox(getPluginInstance(),
                    MAKEINTRESOURCE(IDD_QUEUE),
                    getNppWnd(),
                    dlgProc);
    if (res == -1)
    {
        /* TODO error */
        goto fail_dlg;
    }

    return true;

fail_dlg:
    freeMem(queueDlg);
    queueDlg = NULL;
fail_mem:

    return false;
}

void removeExec(RuleExec *prevExec, RuleExec *exec)
{
    if (prevExec)
        prevExec->next = exec->next;
    else
        execs = exec->next;

    if (!exec->next)
        lastExec = prevExec;

    numExecs--;
    numForeground -= !exec->rule->background;

    freeMem(exec->args);
    freeMem(exec);
}

RuleExec* getExecAt(int pos)
{
    RuleExec *exec;

    exec = execs;

    for (; pos; pos--)
        exec = exec->next;

    return exec;
}

void CALLBACK timerProc(HWND wnd, UINT msg, UINT timerId, DWORD sysTime)
{
    int prevNumExecs;
    RuleExecState prevState;
    bool colDataAvail;
    bool foreground;
    UINT state;
    UINT lastState;

    prevNumExecs = numExecs;
    prevState = execs->state;
    foreground = !execs->rule->background;
    if (queueDlg && queueDlg->colDataFirst)
        colDataAvail = queueDlg->colDataFirst->exec == execs;
    else
        colDataAvail = false;

    updateQueue();

    /* Update the dialog's state. */

    if (queueDlg)
    {
        /* Close the dialog if possible, otherwise update only what needs to be
        ** updated.
        */

        if (!numForeground && !queueDlg->userAction)
            EndDialog(queueDlg->handle, 0);
        else
        {
            if (!numForeground && foreground)
                setCloseable(true);

            if (prevNumExecs != numExecs)
            {
                if (colDataAvail)
                    removeColData(NULL);

                /* IMPORTANT: Set the new count before modifying the item states
                ** or else the list might try to access the now deleted last
                ** item in the event handler.
                */

                lastState = ListView_GetItemState(queueDlg->lvQueue,
                                                  prevNumExecs - 1,
                                                  LVNI_SELECTED
                                                  | LVNI_FOCUSED);

                ListView_SetItemCount(queueDlg->lvQueue, numExecs);

                for (int ii = 1; ii < numExecs; ii++)
                {
                    state = ListView_GetItemState(queueDlg->lvQueue,
                                                  ii,
                                                  LVNI_SELECTED | LVNI_FOCUSED);

                    ListView_SetItemState(queueDlg->lvQueue,
                                          ii - 1,
                                          state,
                                          LVNI_SELECTED | LVNI_FOCUSED);
                }

                if (numExecs)
                {
                    ListView_SetItemState(queueDlg->lvQueue,
                                          numExecs - 1,
                                          lastState,
                                          LVNI_SELECTED | LVNI_FOCUSED);
                }
            }
            else if (prevState != execs->state)
                ListView_Update(queueDlg->lvQueue, 0);
        }
    }

    /* Stop processing the queue if it's empty. */

    if (!numExecs)
        KillTimer(NULL, timerId);
}

void updateQueue(void)
{
    RuleExec *exec;
    NpeNppExecParam npep;
    DWORD state;

    exec = execs;

    if (exec->state == STATE_EXECUTING)
    {
        sendNppExecMsg(NPEM_GETSTATE, &state);
        if (state != NPE_STATEREADY)
            return;

        removeExec(NULL, execs);

        if (!numExecs)
            return;

        exec = execs;
        exec->state = STATE_WAITING;
    }

    npep.szScriptName = exec->rule->cmd;
    npep.szScriptArguments = exec->args;
    npep.dwResult = 0;
    sendNppExecMsg(NPEM_NPPEXEC, &npep);
    exec->state = npep.dwResult ==
                  NPE_EXECUTE_OK ? STATE_EXECUTING : STATE_WAITING;
}

void layoutDlg(void)
{
    RECT clientRc;
    RECT btnModeRc;
    RECT btnCloseRc;
    LONG lvQueueLeft;
    LONG lvQueueTop;
    LONG lvQueueWidth;
    LONG lvQueueHeight;
    LONG btnModeLeft;
    LONG btnModeTop;
    LONG btnAbortLeft;
    LONG btnAbortTop;
    LONG btnCloseLeft;
    LONG btnCloseTop;

    setWndSize(queueDlg->handle, DLG_MIN_WIDTH, DLG_MIN_HEIGHT);

    GetClientRect(queueDlg->handle, &clientRc);
    GetWindowRect(queueDlg->btnMode, &btnModeRc);
    GetWindowRect(queueDlg->btnClose, &btnCloseRc);

    queueDlg->clientW = clientRc.right - clientRc.left;
    queueDlg->clientH = clientRc.bottom - clientRc.top;
    btnCloseLeft = queueDlg->clientW / 2
                   - (btnCloseRc.right - btnCloseRc.left) / 2;
    btnCloseTop = queueDlg->clientH - 5 - (btnCloseRc.bottom - btnCloseRc.top);
    btnModeLeft = 5;
    btnModeTop = queueDlg->clientH - 5
                 - (btnModeRc.bottom - btnModeRc.top) - 35;
    btnAbortLeft = btnModeLeft + btnModeRc.right - btnModeRc.left + 5;
    btnAbortTop = btnModeTop;
    lvQueueLeft = 5;
    lvQueueTop = 5;
    lvQueueWidth = queueDlg->clientW - 5 - lvQueueLeft;
    lvQueueHeight = btnModeTop - 5 - lvQueueTop;

    setWndPos(queueDlg->lvQueue, lvQueueLeft, lvQueueTop);
    setWndSize(queueDlg->lvQueue, lvQueueWidth, lvQueueHeight);
    setWndPos(queueDlg->btnMode, btnModeLeft, btnModeTop);
    setWndPos(queueDlg->btnAbort, btnAbortLeft, btnAbortTop);
    setWndPos(queueDlg->btnClose, btnCloseLeft, btnCloseTop);

    sizeListViewColumns(queueDlg->lvQueue, (ListViewColumnSize[]) {
        {COL_POS, 0.08},
        {COL_RULE, 0.25},
        {COL_STATE, 0.15},
        {COL_PATH, 0.40},
        {COL_BACKGROUND, 0.12},
        {-1}
    });
}

void setCloseable(bool closeable)
{
    UINT flags;
    flags = MF_BYCOMMAND | (closeable ? 0 : (MF_DISABLED | MF_GRAYED));
    EnableMenuItem(GetSystemMenu(queueDlg->handle, FALSE), SC_CLOSE, flags);
    EnableWindow(queueDlg->btnClose, closeable ? TRUE : FALSE);
}

void setAbortBtnState(void)
{
    int lastItem;
    int currItem;
    RuleExec *exec;
    BOOL enabled;

    currItem = ListView_GetNextItem(queueDlg->lvQueue, -1, LVNI_SELECTED);
    if (currItem != -1)
    {
        lastItem = 0;
        exec = execs;

        /* Consider the button enabled unless an active execution is selected as well. */

        enabled = TRUE;

        do
        {
            for (; lastItem < currItem; lastItem++)
                exec = exec->next;

            if (exec->state == STATE_EXECUTING)
            {
                enabled = FALSE;
                break;
            }

            currItem = ListView_GetNextItem(queueDlg->lvQueue,
                                            currItem,
                                            LVNI_SELECTED);
        }
        while (currItem != -1);
    }
    else
        enabled = FALSE;

    if (IsWindowEnabled(queueDlg->btnAbort) != enabled)
        EnableWindow(queueDlg->btnAbort, enabled);
}

BOOL CALLBACK dlgProc(HWND dlg, UINT msg, WPARAM wp, LPARAM lp)
{
    switch (msg)
    {
    case WM_INITDIALOG:
        onInitDlg(dlg);
        return TRUE;

    case WM_DESTROY:
        while (queueDlg->colDataFirst)
            removeColData(NULL);

        freeMem(queueDlg);
        queueDlg = NULL;
        DLGPROC_RESULT(dlg, 0);

    case WM_GETMINMAXINFO:
        ((MINMAXINFO*) lp)->ptMinTrackSize.x = DLG_MIN_WIDTH;
        ((MINMAXINFO*) lp)->ptMinTrackSize.y = DLG_MIN_HEIGHT;
        DLGPROC_RESULT(dlg, 0);

    case WM_SIZE:
        onSize(LOWORD(lp), HIWORD(lp));
        DLGPROC_RESULT(dlg, 0);

    case WM_CLOSE:
        EndDialog(dlg, 0);
        DLGPROC_RESULT(dlg, 0);

    case WM_SYSKEYDOWN:
        if (wp == SC_CLOSE)
        {
            EndDialog(dlg, 0);
            DLGPROC_RESULT(dlg, 0);
        }

        break;

    case WM_COMMAND:
        switch (LOWORD(wp))
        {
        case IDC_BT_ABORT:
            onAbort();
            DLGPROC_RESULT(dlg, 0);
        case IDCANCEL:
            EndDialog(dlg, 0);
            DLGPROC_RESULT(dlg, 0);
        }

        break;

    case WM_NOTIFY:
        switch (LOWORD(wp))
        {
        case IDC_LV_QUEUE:
            switch (((NMHDR*) lp)->code)
            {
            case LVN_ITEMCHANGED:
                setAbortBtnState();
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

void addColData(RuleExec *exec, int execPos)
{
    ColumnData *data;
    wchar_t *pos;
    wchar_t *path;
    int numDigits;
    int num;
    size_t numUnits;

    numDigits = 1;

    for (num = execPos; num; num /= 10)
        numDigits++;

    if (numDigits > SIZE_MAX - 1)
    {
        /* TODO error */
        goto fail_pos_too_large;
    }

    numUnits = sendNppMsg(NPPM_GETFULLPATHFROMBUFFERID,
                          exec->bufferId,
                          (LPARAM) NULL);
    if (!(pos = allocStr(numDigits + 1)))
    {
        /* TODO error */
        goto fail_alloc_pos;
    }

    /* We assume we can fit the path in memory, because it has been done before.
    ** Technically it's risky.
    */

    if (!(path = allocStr(numUnits + 1)))
    {
        /* TODO error */
        goto fail_alloc_path;
    }

    StringCchPrintfW(pos, numUnits + 1, L"%d", execPos);
    sendNppMsg(NPPM_GETFULLPATHFROMBUFFERID, exec->bufferId, (LPARAM) path);

    if (!(data = allocMem(sizeof(ColumnData))))
    {
        /* TODO error */
        goto fail_alloc_data;
    }

    data->exec = exec;
    data->pos = pos;
    data->path = path;
    data->next = NULL;

    if (!queueDlg->colDataFirst)
    {
        queueDlg->colDataFirst = data;
        queueDlg->colDataLast = data;
    }
    else
    {
        queueDlg->colDataLast->next = data;
        queueDlg->colDataLast = queueDlg->colDataLast->next;
    }

    return;

fail_alloc_data:
    freeStr(path);
fail_alloc_path:
    freeStr(pos);
fail_alloc_pos:
fail_pos_too_large:
    return;
}

void removeColData(RuleExec *exec)
{
    ColumnData *data;
    ColumnData *prev;

    prev = NULL;
    data = queueDlg->colDataFirst;

    if (exec)
    {
        while (data->exec != exec)
        {
            prev = data;
            data = data->next;
        }

        if (!data)
            return;
    }

    freeStr(data->path);

    for (; data->next; prev = data, data = data->next)
    {
        data->exec = data->next->exec;
        data->path = data->next->path;
    }

    if (!(queueDlg->colDataLast = prev))
        queueDlg->colDataFirst = NULL;
    else
        queueDlg->colDataLast->next = NULL;

    freeStr(data->pos);
    freeMem(data);
}

ColumnData* getColumnData(RuleExec *exec)
{
    ColumnData *data;
    for (data = queueDlg->colDataFirst;
         data && data->exec != exec;
         data = data->next)
        ;
    return data;
}

void onInitDlg(HWND dlg)
{
    RuleExec *exec;
    int pos;

    queueDlg->handle = dlg;
    queueDlg->lvQueue = GetDlgItem(dlg, IDC_LV_QUEUE);
    queueDlg->btnMode = GetDlgItem(dlg, IDC_BT_MODE);
    queueDlg->btnAbort = GetDlgItem(dlg, IDC_BT_ABORT);
    queueDlg->btnClose = GetDlgItem(dlg, IDCANCEL);
    queueDlg->colDataFirst = NULL;

    for (exec = execs, pos = 1; exec; exec = exec->next, pos++)
        addColData(exec, pos);

    ListView_SetExtendedListViewStyleEx(queueDlg->lvQueue,
                                        LVS_EX_FULLROWSELECT,
                                        LVS_EX_FULLROWSELECT);

    addListViewColumns(queueDlg->lvQueue, (ListViewColumn[]) {
        {COL_POS, L"#"},
        {COL_RULE, L"Rule"},
        {COL_STATE, L"State"},
        {COL_PATH, L"Path"},
        {COL_BACKGROUND, L"Background?"},
        {-1}
    });

    ListView_SetItemCount(queueDlg->lvQueue, numExecs);

    setCloseable(!numForeground);
    EnableWindow(queueDlg->btnAbort, FALSE);
    EnableWindow(queueDlg->btnMode, FALSE);

    /* Not set in the RC file because of a weird resource compiler bug when
    ** concatenating strings in the DIALOG template (only MSVC).
    */

    SetWindowText(dlg, PLUGIN_NAME L": Queue");

    layoutDlg();
    centerWndToParent(queueDlg->handle);
}

void onSize(int newW, int newH)
{
    int deltaWidth;
    int deltaHeight;
    RECT rc;

    deltaWidth = newW - queueDlg->clientW;
    deltaHeight = newH - queueDlg->clientH;
    queueDlg->clientW = newW;
    queueDlg->clientH = newH;

    offsetCtrlSize(queueDlg->lvQueue, deltaWidth, deltaHeight);
    offsetCtrlPos(queueDlg->handle, queueDlg->btnMode, 0, deltaHeight);
    offsetCtrlPos(queueDlg->handle, queueDlg->btnAbort, 0, deltaHeight);

    GetWindowRect(queueDlg->btnClose, &rc);
    MapWindowPoints(NULL, queueDlg->handle, (POINT*) &rc, 2);
    setWndPos(queueDlg->btnClose,
              newW / 2 - (rc.right - rc.left) / 2,
              rc.top + deltaHeight);
}

void onGetDispInfo(NMLVDISPINFO *dispInfo)
{
    LVITEM *item;
    RuleExec *exec;
    const Rule *rule;
    ColumnData *colData;

    item = &dispInfo->item;
    exec = getExecAt(item->iItem);
    rule = exec->rule;

    switch (item->iSubItem)
    {
    case COL_POS:
        colData = getColumnData(exec);
        item->pszText = colData ? colData->pos : PLACEHOLDER;
        break;
    case COL_RULE:
        item->pszText = rule->name;
        break;
    case COL_STATE:
        switch (exec->state)
        {
        case STATE_QUEUED:
            item->pszText = L"Queued";
            break;
        case STATE_WAITING:
            item->pszText = L"Waiting";
            break;
        case STATE_EXECUTING:
            item->pszText = L"Executing";
            break;
        }
        break;
    case COL_PATH:
        colData = getColumnData(exec);
        item->pszText = colData ? colData->path : PLACEHOLDER;
        break;
    case COL_BACKGROUND:
        item->pszText = BOOL_TO_STR_YES_NO(rule->background);
        break;
    }
}

void onAbort(void)
{
    int prevItem;
    int currItem;
    RuleExec *prevExec;
    RuleExec *currExec;
    RuleExec *nextExec;
    int numAborted;

    prevExec = NULL;
    currExec = execs;
    prevItem = 0;
    currItem = -1;
    numAborted = 0;

    while ((currItem = ListView_GetNextItem(queueDlg->lvQueue,
                                            currItem,
                                            LVNI_SELECTED)) != -1)
    {
        for (; prevItem < currItem - numAborted; prevItem++)
        {
            prevExec = currExec;
            currExec = currExec->next;
        }

        nextExec = currExec->next;
        removeColData(currExec);
        removeExec(prevExec, currExec);
        currExec = nextExec;

        /* We need this, because we only update the list view after all
        ** executions have been aborted.
        */

        numAborted++;
    }

    if (!numForeground && !queueDlg->userAction)
        EndDialog(queueDlg->handle, 0);
    else
    {
        /* IMPORTANT: Adjust the count before deselecting all items. */
        ListView_SetItemCount(queueDlg->lvQueue, numExecs);
        ListView_SetItemState(queueDlg->lvQueue, -1, 0,
                              LVIS_SELECTED);
        SetFocus(queueDlg->lvQueue);
        EnableWindow(queueDlg->btnAbort, FALSE);
    }
}
