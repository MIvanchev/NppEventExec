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
#include "exec_def.h"
#include "mem.h"
#include "plugin.h"
#include "queue_dlg.h"
#include "resource.h"
#include "util.h"

/**
 * A placeholder which is displayed in the position column for queue entries
 * the position of which cannot be displayed.
 */
#define PLACEHOLDER L"<n/a>"

/**
 * The highest position in the queue for which a number will be shown in the
 * position column; for positions higher than this, a placeholder string will
 * be shown instead of a number.
 *
 * \sa POS_ENTRY_LEN
 */
#define MAX_ENTRY_POS 999

/**
 * The length in code units of a string that can hold the number of the
 * highest position in the queue, for which a number will be shown.
 *
 * \sa MAX_ENTRY_POS
 */
#define POS_ENTRY_LEN 4

/**
 * The least number of position strings which are allocated in one step; the
 * number of allocated position strings is always a multiple of this constant
 * unless the maximum number of position strings is reached.
 *
 * \sa MAX_ENTRY_POS
 */
#define POS_ENTRIES_CHUNK 5

typedef struct
{
    QueueDlgLaunchReason reason;
    HWND handle;
    HWND lvQueue;
    HWND btnMode;
    HWND btnAbort;
    HWND btnClose;
    LONG minWidth;
    LONG minHeight;
    int clientWidth;
    int clientHeight;
    bool closing;
    bool canceled;
    /*const wchar_t *title;*/
    /*const wchar_t *msg;*/
    wchar_t *posEntries;
    size_t posEntryCnt;
    size_t maxPosEntries;
    unsigned int queueSize;
    unsigned int foregroundCnt;
} Dialog;

typedef enum
{
    COL_POS,
    COL_RULE,
    COL_STATE,
    COL_PATH,
    COL_BACKGROUND
} Column;

static INT_PTR CALLBACK dlgProc(HWND dlg, UINT msg, WPARAM wp, LPARAM lp);
static void onInitDlg(HWND dlg);
static void onSize(LONG clientWidth, LONG clientHeight);
static void onGetDispInfo(NMLVDISPINFO *dispInfo);
static void onAbort(void);
static void onClose(void);
static void onItemChanged(NMLISTVIEW *nmlv);
static void onOdsStateChanged(NMLVODSTATECHANGE *nmosc);
static void onQueueAdd(bool foreground);
static void onQueueRemove(bool foreground);
static void onQueueStatusUpdate(void);
static void layoutDlg(void);
static void enlargePosEntries(unsigned int count);
static void compactPosEntries(void);
static unsigned int countPosDigits(unsigned int pos);
static void formatPos(wchar_t *buf, unsigned int pos);
static bool tryCloseDlg(void);
static void deferClosing(void);
static void appendToTitle(const wchar_t *suffix);
static void updateAbortBtn(void);

static Dialog *dlg;

INT_PTR openQueueDlg(HWND parent, QueueDlgLaunchReason reason)
{
    INT_PTR res;
    bool canceled;

    if (!(dlg = allocMem(sizeof *dlg)))
    {
        /* TODO error */
        goto fail_mem;
    }

    dlg->reason = reason;
    dlg->canceled = false;
    dlg->posEntries = NULL;
    dlg->posEntryCnt = 0;

    res = DialogBoxW(getPluginInstance(), MAKEINTRESOURCE(IDD_QUEUE), parent,
                     dlgProc);

    /* Cache the value, because the memory is freed immediately. */

    canceled = dlg->canceled;

    freeStr(dlg->posEntries);
    freeMem(dlg);
    dlg = NULL;

    if (res == -1)
    {
        /* TODO error */
        goto fail_dlg;
    }

    return canceled;

fail_dlg:
fail_mem:

    return -1;
}

int isQueueDlgVisible(void)
{
    return dlg != NULL;
}

void processQueueEvent(QueueEvent event)
{
    switch (event)
    {
    case QUEUE_ADD_FOREGROUND:
    case QUEUE_ADD_BACKGROUND:
        onQueueAdd(event == QUEUE_ADD_FOREGROUND);
        break;
    case QUEUE_REMOVE_FOREGROUND:
    case QUEUE_REMOVE_BACKGROUND:
        onQueueRemove(event == QUEUE_REMOVE_FOREGROUND);
        break;
    case QUEUE_STATUS_UPDATE:
        onQueueStatusUpdate();
        break;
    }
}

INT_PTR CALLBACK dlgProc(HWND handle, UINT msg, WPARAM wp, LPARAM lp)
{
    switch (msg)
    {
    case WM_INITDIALOG:
        onInitDlg(handle);
        return TRUE;

    case WM_GETMINMAXINFO:
        ((MINMAXINFO*) lp)->ptMinTrackSize.x = dlg->minWidth;
        ((MINMAXINFO*) lp)->ptMinTrackSize.y = dlg->minHeight;
        DLGPROC_RESULT(handle, 0);

    case WM_SIZE:
        onSize(LOWORD(lp), HIWORD(lp));
        DLGPROC_RESULT(handle, 0);

    case WM_SYSKEYDOWN:
        if (wp == SC_CLOSE)
        {
            onClose();
            DLGPROC_RESULT(handle, 0);
        }

        break;

    case WM_COMMAND:
        switch (LOWORD(wp))
        {
        case IDC_BT_ABORT:
            onAbort();
            DLGPROC_RESULT(handle, 0);
        case IDCANCEL:
            onClose();
            DLGPROC_RESULT(handle, 0);
        }

        break;

    case WM_NOTIFY:
        switch (LOWORD(wp))
        {
        case IDC_LV_QUEUE:
            switch (((NMHDR*) lp)->code)
            {
            case LVN_ITEMCHANGED:
                onItemChanged((NMLISTVIEW*) lp);
                break;
            case LVN_ODSTATECHANGED:
                onOdsStateChanged((NMLVODSTATECHANGE*) lp);
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
    assert(MAX_ENTRY_POS >= 0);
    assert(MAX_ENTRY_POS <= SIZE_MAX);
    assert(POS_ENTRY_LEN > 0);
    assert(POS_ENTRY_LEN <= SIZE_MAX);
    assert(POS_ENTRIES_CHUNK > 0);
    assert(POS_ENTRIES_CHUNK <= UINT_MAX);

    dlg->handle = handle;
    dlg->lvQueue = GetDlgItem(handle, IDC_LV_QUEUE);
    dlg->btnMode = GetDlgItem(handle, IDC_BT_MODE);
    dlg->btnAbort = GetDlgItem(handle, IDC_BT_ABORT);
    dlg->btnClose = GetDlgItem(handle, IDCANCEL);

    dlg->maxPosEntries = MIN(SIZE_MAX / POS_ENTRY_LEN, MAX_ENTRY_POS);
    dlg->queueSize = getQueueSize(&dlg->foregroundCnt);

    enlargePosEntries(MAX(POS_ENTRIES_CHUNK, dlg->queueSize));

    ListView_SetExtendedListViewStyleEx(dlg->lvQueue,
                                        LVS_EX_FULLROWSELECT,
                                        LVS_EX_FULLROWSELECT);

    addListViewColumns(dlg->lvQueue, (ListViewColumn[]) {
        {COL_POS, L"#"},
        {COL_RULE, L"Rule"},
        {COL_STATE, L"State"},
        {COL_PATH, L"Path"},
        {COL_BACKGROUND, L"Background?"},
        {-1}
    });

    /* If the dialog was opened, because a foreground rule was queued -- and
    ** there are no other rules -- try to submit the rule to Notepad++ for
    ** execution immediately instead of waiting for the first timer event. This
    ** is not done upon the rule's addition, because of the possibility of the
    ** dialog's creation failure and also since the dialog creation's function
    ** does not return until the rule is executed.
    */

    if (dlg->reason == QDLR_FOREGROUND_RULE && dlg->queueSize == 1)
        updateQueue();

    ListView_SetItemCount(dlg->lvQueue, dlg->queueSize);

    switch (dlg->reason)
    {
    case QDLR_PLUGIN_MENU:
        dlg->closing = false;
        break;
    case QDLR_SAVING_CHANGES:
        dlg->closing = true;
        SetWindowTextW(dlg->btnClose, L"Cancel");
        break;
    case QDLR_FOREGROUND_RULE:
    case QDLR_NPP_CLOSING:
        deferClosing();
        break;
    }

    EnableWindow(dlg->btnAbort, FALSE);
    EnableWindow(dlg->btnMode, FALSE);

    layoutDlg();
    centerWndToParent(dlg->handle);
}

void onSize(LONG clientWidth, LONG clientHeight)
{
    LONG offsWidth;
    LONG offsHeight;
    RECT lvQueueRc;
    RECT btnModeRc;
    RECT btnAbortRc;
    RECT btnCloseRc;
    LONG lvQueueWidth;
    LONG lvQueueHeight;
    LONG btnCloseLeft;
    LONG btnCloseTop;

    offsWidth = clientWidth - dlg->clientWidth;
    offsHeight = clientHeight - dlg->clientHeight;
    dlg->clientWidth = clientWidth;
    dlg->clientHeight = clientHeight;

    GetWindowRect(dlg->lvQueue, &lvQueueRc);
    GetWindowRect(dlg->btnMode, &btnModeRc);
    GetWindowRect(dlg->btnAbort, &btnAbortRc);
    GetWindowRect(dlg->btnClose, &btnCloseRc);
    MapWindowPoints(NULL, dlg->handle, (POINT*) &btnModeRc, 2);
    MapWindowPoints(NULL, dlg->handle, (POINT*) &btnAbortRc, 2);
    MapWindowPoints(NULL, dlg->handle, (POINT*) &btnCloseRc, 2);

    lvQueueWidth = lvQueueRc.right - lvQueueRc.left + offsWidth;
    lvQueueHeight = lvQueueRc.bottom - lvQueueRc.top + offsHeight;
    btnModeRc.top += offsHeight;
    btnAbortRc.top += offsHeight;
    btnCloseLeft = clientWidth / 2
                   - (btnCloseRc.right - btnCloseRc.left) / 2;
    btnCloseTop = btnCloseRc.top + offsHeight;

#define sizeWnd(dlg, width, height) \
    { (dlg), NULL, 0, 0, (width), (height), SWP_NOMOVE | SWP_NOZORDER }

#define positionWnd(dlg, px, py) \
    { (dlg), NULL, (px), (py), 0, 0, SWP_NOSIZE | SWP_NOZORDER }

    setWndPosDeferred((SetWindowPosArgs[]) {
        sizeWnd(dlg->lvQueue, lvQueueWidth, lvQueueHeight),
        positionWnd(dlg->btnMode, btnModeRc.left, btnModeRc.top),
        positionWnd(dlg->btnAbort, btnAbortRc.left, btnAbortRc.top),
        positionWnd(dlg->btnClose, btnCloseLeft, btnCloseTop),
        {NULL}
    });
}

void onGetDispInfo(NMLVDISPINFO *dispInfo)
{
    LVITEM *item;
    unsigned int pos;

    item = &dispInfo->item;
    pos = item->iItem;

    switch (item->iSubItem)
    {
    case COL_POS:
        if (pos < dlg->posEntryCnt)
            item->pszText = dlg->posEntries + pos * POS_ENTRY_LEN;
        else
            item->pszText = PLACEHOLDER;

        break;
    case COL_RULE:
        item->pszText = (wchar_t*) getExecRule(pos);
        break;
    case COL_STATE:
        switch (getExecState(pos))
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
        item->pszText = (wchar_t*) getExecPath(pos);
        break;
    case COL_BACKGROUND:
        item->pszText = BOOL_TO_STR_YES_NO(!isExecForeground(pos));
        break;
    }
}

void onAbort(void)
{
    int positions[16];
    int selectedCnt;
    int abortedCnt;
    int focusedItem;
    int posInList;
    int posInQueue;
    bool firstSelected;
    int ii;

    assert(BUFLEN(positions) > 1);

    selectedCnt = ListView_GetSelectedCount(dlg->lvQueue);
    abortedCnt = 0;
    posInList = -1;
    focusedItem = ListView_GetNextItem(dlg->lvQueue, -1, LVIS_FOCUSED);

    firstSelected = false; /* Shut up compiler... */

    do
    {
        for (ii = 0; ii < BUFLEN(positions) - 1 && ii < selectedCnt; ii++)
        {
            posInList = ListView_GetNextItem(dlg->lvQueue,
                                             posInList,
                                             LVNI_SELECTED);
            posInQueue = posInList - abortedCnt;
            positions[ii] = posInQueue;

            firstSelected = !posInList;
        }

        positions[ii] = -1;

        abortExecs(positions);

        abortedCnt += ii;
        selectedCnt -= ii;
    }
    while (selectedCnt);

    dlg->queueSize = getQueueSize(&dlg->foregroundCnt);

    if (!(dlg->closing && tryCloseDlg()))
    {
        compactPosEntries();

        if (dlg->queueSize)
        {
            if (firstSelected)
                updateQueue();

            /* IMPORTANT: Adjust the count before deselecting all items. */

            ListView_SetItemCount(dlg->lvQueue, dlg->queueSize);
            ListView_SetItemState(dlg->lvQueue, -1, 0,
                                  LVIS_SELECTED | LVIS_FOCUSED);

            if (focusedItem != -1)
            {
                focusedItem = (unsigned int) focusedItem <
                              dlg->queueSize ? focusedItem : dlg->queueSize - 1;

                ListView_SetItemState(dlg->lvQueue,
                                      focusedItem,
                                      LVIS_SELECTED | LVIS_FOCUSED,
                                      LVIS_SELECTED | LVIS_FOCUSED);
                ListView_EnsureVisible(dlg->lvQueue, focusedItem, FALSE);
            }
        }
        else
        {
            ListView_SetItemCount(dlg->lvQueue, 0);
            EnableWindow(dlg->btnAbort, FALSE);
        }

        SetFocus(dlg->lvQueue);
    }
}

void onClose(void)
{
    int choice;

    if (dlg->reason == QDLR_SAVING_CHANGES)
    {
        choice = msgBox(MB_YESNO | MB_ICONQUESTION,
                        dlg->handle,
                        L"Cancel",
                        L"Are you sure you want to cancel saving the changes "
                        L"to the rule list?");

        if (choice == IDYES)
        {
            dlg->canceled = true;

            if (!tryCloseDlg())
            {
                deferClosing();
                appendToTitle(L"(canceled)");
            }
        }
    }
    else if (!tryCloseDlg())
    {
        deferClosing();
        appendToTitle(L"(closing)");
    }
}

void onItemChanged(NMLISTVIEW *nmlv)
{
    if ((nmlv->uNewState & LVIS_SELECTED) != (nmlv->uOldState & LVIS_SELECTED))
        updateAbortBtn();
}

void onOdsStateChanged(NMLVODSTATECHANGE *nmosc)
{
    if ((nmosc->uNewState & LVIS_SELECTED)
        != (nmosc->uOldState & LVIS_SELECTED))
    {
        updateAbortBtn();
    }
}

void onQueueAdd(bool foreground)
{
    dlg->queueSize++;
    dlg->foregroundCnt += foreground;
    enlargePosEntries(dlg->queueSize);
    ListView_SetItemCount(dlg->lvQueue, dlg->queueSize);
}

void onQueueRemove(bool foreground)
{
    int lastState;
    int state;
    unsigned int ii;

    dlg->queueSize--;
    dlg->foregroundCnt -= foreground;

    if (dlg->closing && tryCloseDlg())
        return;

    compactPosEntries();

    /* IMPORTANT: Set the new count before modifying the item states
    ** or else the list might try to access the now deleted last
    ** item in the event handler.
    */

    if (dlg->queueSize)
    {
        lastState = ListView_GetItemState(dlg->lvQueue,
                                          dlg->queueSize,
                                          LVNI_SELECTED | LVNI_FOCUSED);
    }
    else
    {
        lastState = 0; /* Shut up MSVC... */
    }

    ListView_SetItemCount(dlg->lvQueue, dlg->queueSize);

    for (ii = 1; ii < dlg->queueSize; ii++)
    {
        state = ListView_GetItemState(dlg->lvQueue,
                                      ii,
                                      LVNI_SELECTED | LVNI_FOCUSED);

        ListView_SetItemState(dlg->lvQueue,
                              ii - 1,
                              state,
                              LVNI_SELECTED | LVNI_FOCUSED);
    }

    if (dlg->queueSize)
    {
        ListView_SetItemState(dlg->lvQueue,
                              dlg->queueSize - 1,
                              lastState,
                              LVNI_SELECTED | LVNI_FOCUSED);
    }
}

void onQueueStatusUpdate(void)
{
    ListView_Update(dlg->lvQueue, 0);
}

void layoutDlg(void)
{
    RECT dlgRc;
    RECT clientRc;

    GetWindowRect(dlg->handle, &dlgRc);
    GetClientRect(dlg->handle, &clientRc);
    dlg->minWidth = dlgRc.right - dlgRc.left;
    dlg->minHeight = dlgRc.bottom - dlgRc.top;
    dlg->clientWidth = clientRc.right - clientRc.left;
    dlg->clientHeight = clientRc.bottom - clientRc.top;

    sizeListViewColumns(dlg->lvQueue, (ListViewColumnSize[]) {
        {COL_POS, 0.08},
        {COL_RULE, 0.25},
        {COL_STATE, 0.15},
        {COL_PATH, 0.40},
        {COL_BACKGROUND, 0.12},
        {-1}
    });
}

void enlargePosEntries(unsigned int count)
{
    wchar_t *entries;
    size_t newEntryCnt;
    size_t ii;

    assert(count);
    assert(POS_ENTRIES_CHUNK > 0);
    assert(POS_ENTRIES_CHUNK <= UINT_MAX);

    if (count <= dlg->posEntryCnt
        || dlg->posEntryCnt == dlg->maxPosEntries)
    {
        return;
    }

    if (count % POS_ENTRIES_CHUNK)
    {
        count -= count % POS_ENTRIES_CHUNK;

        if (count > UINT_MAX - POS_ENTRIES_CHUNK)
            count = UINT_MAX;
        else
            count += POS_ENTRIES_CHUNK;
    }

    if (count > dlg->maxPosEntries)
        newEntryCnt = dlg->maxPosEntries;
    else
        newEntryCnt = count;

    if (!(entries = reallocStr(dlg->posEntries, newEntryCnt * POS_ENTRY_LEN)))
    {
        /* TODO warning
        **
        ** We cannot allocate enough entries, but that's not a fatal issue
        ** because a placeholder will be shown instead of the actual number.
        */

        return;
    }

    for (ii = dlg->posEntryCnt; ii < newEntryCnt; ii++)
        formatPos(entries + ii * POS_ENTRY_LEN, (unsigned int) ii);

    dlg->posEntries = entries;
    dlg->posEntryCnt = newEntryCnt;
}

void compactPosEntries(void)
{
    wchar_t *entries;
    size_t newEntryCnt;

    assert(POS_ENTRIES_CHUNK > 0);
    assert(POS_ENTRIES_CHUNK <= SIZE_MAX);

    /* The initial chunk is never released from memory so don't bother. */

    if (dlg->posEntryCnt <= dlg->queueSize
        || dlg->posEntryCnt <= POS_ENTRIES_CHUNK)
    {
        return;
    }

    if (dlg->queueSize <= POS_ENTRIES_CHUNK)
        newEntryCnt = POS_ENTRIES_CHUNK;
    else
    {
        /* Find out the next multiple of POS_ENTRIES_CHUNK, but do not compact
        ** if the current number of rules fits within the last allocated chunk.
        */

        newEntryCnt = dlg->queueSize - 1;
        newEntryCnt -= newEntryCnt % POS_ENTRIES_CHUNK;

        if (newEntryCnt >= dlg->posEntryCnt - POS_ENTRIES_CHUNK)
            return;

        newEntryCnt += POS_ENTRIES_CHUNK;
    }

    if (!(entries = reallocStr(dlg->posEntries, newEntryCnt * POS_ENTRY_LEN)))
    {
        /* TODO warning
        **
        ** Don't take any other action other than issuing a warning. The failed
        ** reallocation is not really fatal (yet).
        */

        return;
    }

    dlg->posEntries = entries;
    dlg->posEntryCnt = newEntryCnt;
}

unsigned int countPosDigits(unsigned int pos)
{
    unsigned int cnt;

    cnt = 0;

    do
    {
        cnt++;
    }
    while (pos /= 10);

    return cnt;
}

void formatPos(wchar_t *buf, unsigned int pos)
{
    unsigned int digitCnt;

    /* Avoid the signed overflow. */

    if (pos < INT_MAX)
        pos++;

    digitCnt = countPosDigits(pos);

    buf[digitCnt] = L'\0';

    do
    {
        buf[--digitCnt] = L'0' + (pos % 10);
    }
    while (pos /= 10);
}

bool tryCloseDlg(void)
{
    switch (dlg->reason)
    {
    case QDLR_SAVING_CHANGES:
        if ((dlg->canceled && !dlg->foregroundCnt) || !dlg->queueSize)
        {
            EndDialog(dlg->handle, IDCANCEL);
            return true;
        }

        return false;

    case QDLR_NPP_CLOSING:
        if (!dlg->queueSize)
        {
            EndDialog(dlg->handle, IDCANCEL);
            return true;
        }

        return false;

    default:
        break;
    }

    if (!dlg->foregroundCnt)
    {
        EndDialog(dlg->handle, IDCANCEL);
        return true;
    }

    return false;
}

void deferClosing(void)
{
    UINT flags;
    flags = MF_BYCOMMAND | MF_DISABLED | MF_GRAYED;
    EnableMenuItem(GetSystemMenu(dlg->handle, FALSE), SC_CLOSE, flags);
    EnableWindow(dlg->btnClose, FALSE);
    dlg->closing = true;
}

void appendToTitle(const wchar_t *suffix)
{
    wchar_t title[1024];
    wchar_t buf[1024];

    GetWindowTextW(dlg->handle, title, BUFLEN(title));
    StringCbPrintfW(buf, sizeof(buf), L"%s %s", title, suffix);
    SetWindowTextW(dlg->handle, buf);
}

void updateAbortBtn(void)
{
    int curr;
    BOOL enabled;

    curr = ListView_GetNextItem(dlg->lvQueue, -1, LVNI_SELECTED);
    enabled = curr != -1 && (curr || getExecState(curr) != STATE_EXECUTING);
    EnableWindow(dlg->btnAbort, enabled);
}
