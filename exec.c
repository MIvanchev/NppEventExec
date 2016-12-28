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
    LRESULT bufferId;
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
    HWND txtQueue;
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

static wchar_t* createArgs(LRESULT bufferId, const wchar_t *path);
static bool launchQueueDlg(bool userAction);
static RuleExec* getExecAt(size_t pos);
static void CALLBACK timerProc(HWND wnd, UINT msg, UINT timerId, DWORD sysTime);
static void updateQueue(void);
static void layoutDlg(void);
static void layoutColumns(void);
static void setColumnWidth(int totalWidth,
                           int *remainingWidth,
                           Column col,
                           double widthInPercent);
static void setCloseable(bool closeable);
static void addColumn(Column column, wchar_t *title);
static BOOL CALLBACK dlgProc(HWND dlg, UINT msg, WPARAM wp, LPARAM lp);
static void removeColData(void);
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

int execRule(LRESULT bufferId, const wchar_t *path, const Rule *rule)
{
    RuleExec *exec;
    RuleExec *prevExec;

    if (numExecs == INT_MAX)
    {
        /* TODO error */
        goto fail_too_many_execs;
    }
    else if (!(exec = allocMem(sizeof(RuleExec))))
    {
        /* TODO error */
        goto fail_alloc;
    }
    else if (!(exec->args = createArgs(bufferId, path)))
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

wchar_t* createArgs(LRESULT bufferId, const wchar_t *path)
{
    wchar_t *args;
    unsigned long numDigits;
    size_t pathLen;
    size_t size;
    int sign;

    if (bufferId > LONG_MAX)
    {
        /* TODO error */
        goto fail_id_too_large;
    }

    sign = bufferId < 0;
    numDigits = countDigits(bufferId);
    pathLen = wcslen(path);

    /* The initial size accounts for the sign, 2 pairs of quotes, 1 space
    ** inbetween and '\0'.
    */

    size = sign + 6;

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
fail_id_too_large:

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

RuleExec* getExecAt(size_t pos)
{
    RuleExec *re;

    for (re = execs; pos; re = re->next, pos--)
        ;

    return re;
}

void CALLBACK timerProc(HWND wnd, UINT msg, UINT timerId, DWORD sysTime)
{
    int prevNumExecs;
    DWORD prevState;
    bool colDataAvail;
    bool foreground;

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
                    removeColData();

                ListView_SetItemCount(queueDlg->lvQueue, numExecs);
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

        execs = exec->next;
        numExecs--;
        numForeground -= !exec->rule->background;
        freeMem(exec->args);
        freeMem(exec);

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
    RECT rc;
    int dlgWidth;
    int dlgHeight;
    int btnBottom;
    int btnLeft;
    int lvQueueTop;
    int lvQueueBottom;

    dlgWidth = DLG_MIN_WIDTH;
    dlgHeight = DLG_MIN_HEIGHT;

    setWndSize(queueDlg->handle, dlgWidth, dlgHeight);
    GetClientRect(queueDlg->handle, &rc);
    queueDlg->clientW = rc.right - rc.left;
    queueDlg->clientH = rc.bottom - rc.top;

    GetWindowRect(queueDlg->txtQueue, &rc);
    setWndPos(queueDlg->txtQueue, 5, 5);
    setWndSize(queueDlg->txtQueue, queueDlg->clientW - 10, rc.bottom - rc.top);

    lvQueueTop = 5 + rc.bottom - rc.top + 5;

    GetWindowRect(queueDlg->btnClose, &rc);
    setWndPos(queueDlg->btnClose,
              queueDlg->clientW / 2 - (rc.right - rc.left) / 2,
              queueDlg->clientH - 5 - (rc.bottom - rc.top));

    btnBottom = queueDlg->clientH - 5 - (rc.bottom - rc.top) - 35;
    GetWindowRect(queueDlg->btnMode, &rc);
    setWndPos(queueDlg->btnMode, 5, btnBottom - (rc.bottom - rc.top));

    lvQueueBottom = btnBottom - (rc.bottom - rc.top) - 5;

    btnLeft = 5 + (rc.right - rc.left) + 5;
    GetWindowRect(queueDlg->btnAbort, &rc);
    setWndPos(queueDlg->btnAbort, btnLeft, btnBottom - (rc.bottom - rc.top));

    setWndPos(queueDlg->lvQueue, 5, lvQueueTop);
    setWndSize(queueDlg->lvQueue,
               queueDlg->clientW - 10,
               lvQueueBottom - lvQueueTop);

    layoutColumns();
}

void layoutColumns(void)
{
    RECT rc;
    int totalWidth;
    int remainingWidth;

    GetClientRect(queueDlg->lvQueue, &rc);

    totalWidth = rc.right - rc.left;
    remainingWidth = totalWidth;

    setColumnWidth(totalWidth, &remainingWidth, COL_POS, 0.08);
    setColumnWidth(totalWidth, &remainingWidth, COL_RULE, 0.25);
    setColumnWidth(totalWidth, &remainingWidth, COL_STATE, 0.15);
    setColumnWidth(totalWidth, &remainingWidth, COL_PATH, 0.40);
    ListView_SetColumnWidth(queueDlg->lvQueue, COL_BACKGROUND, remainingWidth);
}

void setColumnWidth(int totalWidth,
                    int *remainingWidth,
                    Column col,
                    double widthInPercent)
{
    int width;

    width = totalWidth * widthInPercent;
    if (width > *remainingWidth)
        width = *remainingWidth;

    *remainingWidth -= width;

    ListView_SetColumnWidth(queueDlg->lvQueue, col, width);
}

void setCloseable(bool closeable)
{
    UINT flags;
    flags = MF_BYCOMMAND | (closeable ? 0 : (MF_DISABLED | MF_GRAYED));
    EnableMenuItem(GetSystemMenu(queueDlg->handle, FALSE), SC_CLOSE, flags);
    EnableWindow(queueDlg->btnClose, closeable ? TRUE : FALSE);
}

void addColumn(Column column, wchar_t *title)
{
    LVCOLUMN col;

    col.mask = LVCF_WIDTH | LVCF_TEXT | LVCF_SUBITEM;
    col.iSubItem = column;
    col.pszText = title;
    col.cx = 0;

    ListView_InsertColumn(queueDlg->lvQueue, col.iSubItem, &col);
}

void setAbortBtnState(void)
{
    int currItem;
    int lastItem;
    RuleExec *re;

    lastItem = -1;
    currItem = -1;
    re = execs;

    currItem = ListView_GetNextItem(queueDlg->lvQueue, -1, LVNI_SELECTED);
    if (currItem == -1)
    {
        EnableWindow(queueDlg->btnAbort, FALSE);
        return;
    }

    while (currItem != -1)
    {
        for (; lastItem < currItem; lastItem++, re = re->next)
        {
            if (re->state == STATE_EXECUTING)
            {
                EnableWindow(queueDlg->btnAbort, FALSE);
                return;
            }
        }

        currItem = ListView_GetNextItem(queueDlg->lvQueue,
                                        currItem,
                                        LVNI_SELECTED);
    }

    EnableWindow(queueDlg->btnAbort, TRUE);
}

BOOL CALLBACK dlgProc(HWND dlg, UINT msg, WPARAM wp, LPARAM lp)
{
    MINMAXINFO *mmi;

    switch (msg)
    {
    case WM_INITDIALOG:
        onInitDlg(dlg);
        return TRUE;

    case WM_DESTROY:
        while (queueDlg->colDataFirst)
            removeColData();

        freeMem(queueDlg);
        queueDlg = NULL;
        DLGPROC_RESULT(dlg, 0);

    case WM_GETMINMAXINFO:
        mmi = (MINMAXINFO*) lp;
        mmi->ptMinTrackSize.x = DLG_MIN_WIDTH;
        mmi->ptMinTrackSize.y = DLG_MIN_HEIGHT;
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

void removeColData(void)
{
    ColumnData *data;
    wchar_t *prevPos;
    wchar_t *nextPos;

    freeStr(queueDlg->colDataFirst->path);
    freeStr(queueDlg->colDataLast->pos);

    prevPos = queueDlg->colDataFirst->pos;

    for (data = queueDlg->colDataFirst; data && data->next; data = data->next)
    {
        nextPos = data->next->pos;
        data->next->pos = prevPos;
        prevPos = nextPos;
    }

    data = queueDlg->colDataFirst;
    queueDlg->colDataFirst = queueDlg->colDataFirst->next;
    freeMem(data);
}

void addColData(RuleExec *exec, int execPos)
{
    ColumnData *data;
    wchar_t *pos;
    wchar_t *path;
    unsigned long numDigits;
    size_t numUnits;

    numDigits = countDigits(execPos);
    if (numDigits > SIZE_MAX - 1)
    {
        /* TODO error */
        goto fail_too_many_digits;
    }

    numUnits = sendNppMsg(NPPM_GETFULLPATHFROMBUFFERID,
                          exec->bufferId,
                          (LPARAM) NULL);
    /* We assume we can fit the path in memory, because it has been done before ;) */
    if (!(pos = allocStr(numDigits + 1)))
    {
        /* TODO error */
        goto fail_alloc_pos;
    }
    else if (!(path = allocStr(numUnits + 1)))
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
fail_too_many_digits:
    return;
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
    queueDlg->txtQueue = GetDlgItem(dlg, IDC_ST_QUEUE);
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

    addColumn(COL_POS, L"#");
    addColumn(COL_RULE, L"Rule");
    addColumn(COL_STATE, L"State");
    addColumn(COL_PATH, L"Path");
    addColumn(COL_BACKGROUND, L"Background?");

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

    offsetCtrlSize(queueDlg->txtQueue, deltaWidth, 0);
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
        item->pszText = rule->background ? L"yes" : L"no";
        break;
    }
}

void onAbort(void)
{
    /* TODO */
}
