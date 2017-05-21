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
#include "exec_def.h"
#include "mem.h"
#include "plugin.h"
#include "queue_dlg.h"
#include "resource.h"
#include "util.h"

/** TODO */
#define PLACEHOLDER L"<n/a>"

/** TODO */
#define POS_ENTRIES_CHUNK 32

typedef struct
{
    HWND handle;
    HWND lvQueue;
    HWND btnMode;
    HWND btnAbort;
    HWND btnClose;
    HWND btnCancel;
    LONG minWidth;
    LONG minHeight;
    int clientWidth;
    int clientHeight;
    bool waitForAll;
    bool closed;
    bool canceled;
    bool cancelable;
    int maxPosDigits;
    /*const wchar_t *title;*/
    /*const wchar_t *msg;*/
    wchar_t *posEntries;
    size_t posEntryLen;
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

static BOOL CALLBACK dlgProc(HWND dlg, UINT msg, WPARAM wp, LPARAM lp);
static void onInitDlg(HWND dlg);
static void onSize(LONG clientWidth, LONG clientHeight);
static void onGetDispInfo(NMLVDISPINFO *dispInfo);
static void onAbort(void);
static void onClose(void);
static void onCancel(void);
static void onQueueAdd(bool foreground);
static void onQueueRemove(bool foreground);
static void onQueueStatusUpdate(void);
static void layoutDlg(void);
static void enlargePosEntries(unsigned int count);
static void compactPosEntries(void);
static int countPosDigits(int pos);
static void formatPos(wchar_t *buf, int pos);
static bool tryCloseDlg(void);
static bool tryCancelDlg(void);
static void disableClosing(void);
static void appendToTitle(const wchar_t *suffix);
static void updateAbortBtn(void);

static Dialog *dlg;

INT_PTR openQueueDlg(HWND parent,
                     /*const wchar_t *title,*/
                     /*const wchar_t *msg,*/
                     bool waitForAll,
                     bool autoClose,
                     bool cancelable)
{
    INT_PTR res;

    if (!(dlg = allocMem(sizeof *dlg)))
    {
        /* TODO error */
        goto fail_mem;
    }

    dlg->waitForAll = waitForAll;
    dlg->closed = autoClose;
    dlg->canceled = false;
    dlg->cancelable = cancelable;
    dlg->posEntries = NULL;

    res = DialogBox(getPluginInstance(), MAKEINTRESOURCE(IDD_QUEUE), parent,
                    dlgProc);

    freeMem(dlg);
    dlg = NULL;

    if (res == -1)
    {
        /* TODO error */
        goto fail_dlg;
    }

    return res;

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

BOOL CALLBACK dlgProc(HWND handle, UINT msg, WPARAM wp, LPARAM lp)
{
    switch (msg)
    {
    case WM_INITDIALOG:
        onInitDlg(handle);
        return TRUE;

    case WM_DESTROY:
        freeStr(dlg->posEntries);
        DLGPROC_RESULT(handle, 0);

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
            if (!dlg->cancelable && !dlg->closed)
                onClose();

            DLGPROC_RESULT(handle, 0);
        case IDC_BT_CLOSE:
            onClose();
            DLGPROC_RESULT(handle, 0);
        case IDC_BT_CANCEL:
            onCancel();
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
                updateAbortBtn();
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
    int maxDigits;

    dlg->handle = handle;
    dlg->lvQueue = GetDlgItem(handle, IDC_LV_QUEUE);
    dlg->btnMode = GetDlgItem(handle, IDC_BT_MODE);
    dlg->btnAbort = GetDlgItem(handle, IDC_BT_ABORT);
    dlg->btnClose = GetDlgItem(handle, IDC_BT_CLOSE);
    dlg->btnCancel = GetDlgItem(handle, IDC_BT_CANCEL);
    dlg->posEntries = NULL;
    dlg->posEntryCnt = 0;

    maxDigits = countPosDigits(INT_MAX);

    if (maxDigits > SIZE_MAX - 1)
    {
        dlg->posEntryLen = 0;
        dlg->maxPosEntries = 0;
    }
    else
    {
        dlg->posEntryLen = maxDigits + 1;
        dlg->maxPosEntries = SIZE_MAX / dlg->posEntryLen;
        if (dlg->maxPosEntries > INT_MAX)
            dlg->maxPosEntries = INT_MAX;
    }

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

    ListView_SetItemCount(dlg->lvQueue, dlg->queueSize);

    if (dlg->closed)
        disableClosing();

    if (dlg->cancelable)
        ShowWindow(dlg->btnClose, SW_HIDE);
    else
        ShowWindow(dlg->btnCancel, SW_HIDE);

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
        positionWnd(dlg->btnCancel, btnCloseLeft, btnCloseTop),
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
            item->pszText = dlg->posEntries + pos * dlg->posEntryLen;
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
    int positions[2];
    int selectedCnt;
    int abortedCnt;
    int focusedItem;
    int posInList;
    int posInQueue;
    int ii;

    assert(BUFLEN(positions) > 1);

    selectedCnt = ListView_GetSelectedCount(dlg->lvQueue);
    abortedCnt = 0;
    posInList = -1;
    focusedItem = ListView_GetNextItem(dlg->lvQueue, -1, LVIS_FOCUSED);

    do
    {
        for (ii = 0; ii < BUFLEN(positions) - 1 && ii < selectedCnt; ii++)
        {
            posInList = ListView_GetNextItem(dlg->lvQueue,
                                             posInList,
                                             LVNI_SELECTED);
            posInQueue = posInList - abortedCnt;
            positions[ii] = posInQueue;
        }

        positions[ii] = -1;

        abortExecs(positions, &dlg->queueSize, &dlg->foregroundCnt);

        abortedCnt += ii;
        selectedCnt -= ii;
    }
    while (selectedCnt);

    if (!(dlg->canceled && tryCancelDlg())
        && !(dlg->closed && tryCloseDlg()))
    {
        compactPosEntries();

        /* IMPORTANT: Adjust the count before deselecting all items. */

        ListView_SetItemCount(dlg->lvQueue, dlg->queueSize);
        ListView_SetItemState(dlg->lvQueue, -1, 0,
                              LVIS_SELECTED | LVIS_FOCUSED);

        if (dlg->queueSize && focusedItem != -1)
        {
            focusedItem = (unsigned int) focusedItem <
                          dlg->queueSize ? focusedItem : dlg->queueSize - 1;

            ListView_SetItemState(dlg->lvQueue,
                                  focusedItem,
                                  LVIS_SELECTED | LVIS_FOCUSED,
                                  LVIS_SELECTED | LVIS_FOCUSED);
            ListView_EnsureVisible(dlg->lvQueue, focusedItem, FALSE);
            SetFocus(dlg->lvQueue);
        }

        updateAbortBtn();

        if (dlg->queueSize)
            updateQueue();
    }
}

void onClose(void)
{
    if (!tryCloseDlg())
    {
        dlg->closed = true;
        disableClosing();
        appendToTitle(L"(closing)");
    }
}

void onCancel(void)
{
    if (!tryCancelDlg())
    {
        dlg->canceled = true;
        EnableWindow(dlg->btnCancel, FALSE);
        appendToTitle(L"(canceling)");
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
    int ii;

    dlg->queueSize--;
    dlg->foregroundCnt -= foreground;

    if ((dlg->canceled && tryCancelDlg())
        || (dlg->closed && tryCloseDlg()))
    {
        return;
    }

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
    unsigned int diff;
    wchar_t *entries;
    size_t len;
    size_t ii;

    assert(count);
    assert(POS_ENTRIES_CHUNK > 0);
    assert(POS_ENTRIES_CHUNK <= UINT_MAX);

    if (count <= dlg->posEntryCnt)
    {
        /* No need to do anything, enough entries are available. */
        return;
    }
    if (dlg->posEntryCnt == dlg->maxPosEntries)
    {
        /* TODO warning
        **
        ** Cannot grow further.
        */

        return;
    }

    /* Calculate how many entries to add to count to make it a factor of
    ** POS_ENTRIES_CHUNK.
    */

    diff = POS_ENTRIES_CHUNK - (1 + (count - 1) % POS_ENTRIES_CHUNK);

    if (diff > dlg->maxPosEntries
        || count > dlg->maxPosEntries - diff)
    {
        count = dlg->maxPosEntries;
    }
    else
        count += diff;

    len = count * dlg->posEntryLen;

    if (!(entries = reallocStr(dlg->posEntries, len)))
    {
        /* TODO warning
        **
        ** We cannot allocate enough entries, but that's not a fatal issue
        ** because a placeholder will be shown instead of the actual number.
        */

        return;
    }

    for (ii = dlg->posEntryCnt; ii < count; ii++)
        formatPos(entries + ii * dlg->posEntryLen, ii);

    dlg->posEntries = entries;
    dlg->posEntryCnt = count;
}

void compactPosEntries(void)
{
    wchar_t *entries;
    size_t count;
    size_t len;

    assert(POS_ENTRIES_CHUNK > 0);

    /* The initial chunk is never released from memory so don't bother. */

    if (dlg->posEntryCnt <= dlg->queueSize
        || dlg->posEntryCnt <= POS_ENTRIES_CHUNK)
    {
        return;
    }

    if (dlg->queueSize <= POS_ENTRIES_CHUNK)
        count = POS_ENTRIES_CHUNK;
    else
    {
        /* Find out the next multiple of POS_ENTRIES_CHUNK, but do not compact
        ** if the current number of rules fits within the last allocated chunk.
        */

        count = dlg->queueSize - 1;
        count -= count % POS_ENTRIES_CHUNK;

        if (count >= dlg->posEntryCnt - POS_ENTRIES_CHUNK)
            return;

        count += POS_ENTRIES_CHUNK;
    }

    len = count * dlg->posEntryLen;

    if (!(entries = reallocStr(dlg->posEntries, len)))
    {
        /* TODO warning
        **
        ** Don't take any other action other than issuing a warning. The failed
        ** reallocation is not really fatal (yet).
        */

        return;
    }

    dlg->posEntries = entries;
    dlg->posEntryCnt = count;
}

int countPosDigits(int pos)
{
    int cnt;

    cnt = 0;

    do
    {
        cnt++;
    }
    while (pos /= 10);

    return cnt;
}

void formatPos(wchar_t *buf, int pos)
{
    int digitCnt;

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
    if ((dlg->waitForAll && !dlg->queueSize)
        || (!dlg->waitForAll && !dlg->foregroundCnt))
    {
        EndDialog(dlg->handle, IDC_BT_CLOSE);
        return true;
    }
    return false;
}

bool tryCancelDlg(void)
{
    if (!dlg->foregroundCnt)
    {
        EndDialog(dlg->handle, IDC_BT_CANCEL);
        return true;
    }
    return false;
}

void disableClosing(void)
{
    UINT flags;
    flags = MF_BYCOMMAND | MF_DISABLED | MF_GRAYED;
    EnableMenuItem(GetSystemMenu(dlg->handle, FALSE), SC_CLOSE, flags);
    EnableWindow(dlg->btnClose, FALSE);
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

    if (IsWindowEnabled(dlg->btnAbort) != enabled)
        EnableWindow(dlg->btnAbort, enabled);
}
