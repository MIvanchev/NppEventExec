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
#include "rule.h"
#include "Scintilla.h"
#include "exec.h"
#include "exec_def.h"
#include "Notepad_plus_msgs.h"
#include "nppexec_msgs.h"
#include "mem.h"
#include "plugin.h"
#include "queue_dlg.h"
#include "resource.h"
#include "util.h"

/** TODO */
#define UPDATE_INTERVAL_IN_MS 100

typedef struct _Exec
{
    const Rule *rule;
    ExecState state;
    struct _Exec *next;
    wchar_t *path;
    wchar_t args[];
} Exec;

static Exec* allocExec(size_t pathLen, uptr_t bufIdDigitCnt, size_t *argsLen);
static bool initArgs(Exec *exec,
                     size_t argsLen,
                     size_t pathLen,
                     const wchar_t *path,
                     uptr_t bufIdDigitCnt,
                     uptr_t bufId);
static uptr_t countBufIdDigits(uptr_t bufId);
static void CALLBACK timerProc(HWND wnd, UINT msg, UINT timerId, DWORD sysTime);
static Exec* getExecAt(int pos);

static struct
{
    Exec *first;
    Exec *last;
    unsigned int size;
    unsigned int foregroundCnt;
    UINT timerId;
} queue;

int execRule(uptr_t bufId, const wchar_t *path, const Rule *rule)
{
    Exec *exec;
    Exec *prevExec;
    uptr_t bufIdDigitCnt;
    size_t pathLen;
    size_t argsLen;

    assert(path);
    assert(rule);

    if (queue.size == INT_MAX)
    {
        /* TODO error */
        goto fail_too_many_execs;
    }

    queue.size++;
    queue.foregroundCnt += !rule->background;

    pathLen = wcslen(path);
    bufIdDigitCnt = countBufIdDigits(bufId);

    if (!(exec = allocExec(pathLen, bufIdDigitCnt, &argsLen)))
    {
        /* TODO error */
        goto fail_alloc;
    }

    if (!initArgs(exec, argsLen, pathLen, path, bufIdDigitCnt, bufId))
    {
        /* TODO error */
        goto fail_args;
    }

    exec->rule = rule;
    exec->state = STATE_QUEUED;
    exec->next = NULL;

    if (!queue.first)
    {
        if (!(queue.timerId
                  = SetTimer(NULL, 0, UPDATE_INTERVAL_IN_MS, timerProc)))
        {
            /* TODO error */
            goto fail_timer;
        }

        /* Get rid of a 'maybe uninitialized' warning. */
        prevExec = NULL;

        queue.first = exec;
        queue.last = exec;

        /* If we're adding a background rule, try to pass it to Notepad++ for
        ** execution immediately. However do NOT schedule a background rule at
        ** this point, because the blocking dialog must be opened and this
        ** could fail. Instead, the block dialog's initialization code is
        ** responsible for updating the queue.
        */

        if (rule->background)
            updateQueue();
    }
    else
    {
        prevExec = queue.last;
        queue.last->next = exec;
        queue.last = exec;
    }

    if (isQueueDlgVisible())
    {
        processQueueEvent(
            rule->background ? QUEUE_ADD_BACKGROUND : QUEUE_ADD_FOREGROUND);
    }
    if (!rule->background
        && openQueueDlg(getNppWnd(), QDLR_FOREGROUND_RULE) == -1)
    {
        /* TODO launch dialog */
        goto fail_dlg;
    }

    return 0;

fail_dlg:

    if (!prevExec)
    {
        stopQueue();
        queue.first = NULL;
        queue.last = NULL;
    }
    else
    {
        queue.last = prevExec;
        queue.last->next = NULL;
    }

fail_timer:
fail_args:
    freeMem(exec);
fail_alloc:
    queue.foregroundCnt -= !rule->background;
    queue.size--;
fail_too_many_execs:
    return 1;
}

void emptyQueue(void)
{
    Exec *exec;
    Exec *next;

    assert(queue.size);
    assert(!queue.foregroundCnt);

    exec = queue.first;

    do
    {
        next = exec->next;
        freeMem(exec);
        exec = next;
    }
    while (exec);

    stopQueue();

    queue.first = NULL;
    queue.last = NULL;
    queue.size = 0;
}

int isQueueEmpty(void)
{
    return !queue.size;
}

void updateQueue(void)
{
    Exec *next;
    NpeNppExecParam npep;
    DWORD state;

    assert(queue.size);

    if (queue.first->state == STATE_EXECUTING)
    {
        sendNppExecMsg(NPEM_GETSTATE, &state);
        if (state != NPE_STATEREADY)
            return;

        queue.size--;
        queue.foregroundCnt -= !queue.first->rule->background;

        next = queue.first->next;
        freeMem(queue.first);
        queue.first = next;

        if (!next)
        {
            stopQueue();
            queue.last = NULL;
            return;
        }

        /* The state is updated below. */
    }

    npep.szScriptName = queue.first->rule->cmd;
    npep.szScriptArguments = queue.first->args;
    npep.dwResult = 0;
    sendNppExecMsg(NPEM_NPPEXEC, &npep);
    queue.first->state = npep.dwResult ==
                         NPE_EXECUTE_OK ? STATE_EXECUTING : STATE_WAITING;
}

void stopQueue(void)
{
    KillTimer(NULL, queue.timerId);
}

unsigned int getQueueSize(unsigned int *foregroundCnt)
{
    *foregroundCnt = queue.foregroundCnt;
    return queue.size;
}

void abortExecs(int *positions)
{
    Exec *prevExec;
    Exec *currExec;
    Exec *nextExec;
    int adjustedPos;
    int ii;

    assert(positions);

    prevExec = NULL;
    currExec = queue.first;
    adjustedPos = 0;

    for (ii = 0; positions[ii] != -1; ii++)
    {
        for (; adjustedPos < positions[ii] - ii; adjustedPos++)
        {
            prevExec = currExec;
            currExec = currExec->next;
        }

        queue.foregroundCnt -= !currExec->rule->background;

        nextExec = currExec->next;
        freeMem(currExec);
        currExec = nextExec;

        if (prevExec)
            prevExec->next = currExec;
        else
            queue.first = currExec;
    }

    if (!currExec && !(queue.last = prevExec))
        stopQueue();

    queue.size -= ii;
}

const wchar_t* getExecRule(unsigned int pos)
{

    assert(pos < queue.size);

    return getExecAt(pos)->rule->name;
}

ExecState getExecState(unsigned int pos)
{

    assert(pos < queue.size);

    return getExecAt(pos)->state;
}

const wchar_t* getExecPath(unsigned int pos)
{

    assert(pos < queue.size);

    return getExecAt(pos)->path;
}

int isExecForeground(unsigned int pos)
{

    assert(pos < queue.size);

    return !getExecAt(pos)->rule->background;
}

bool initArgs(Exec *exec,
              size_t argsLen,
              size_t pathLen,
              const wchar_t *path,
              uptr_t bufIdDigitCnt,
              uptr_t bufId)
{
    assert(argsLen);
    assert(pathLen);
    assert(path);
    assert(bufIdDigitCnt);

    if (bufIdDigitCnt > INT_MAX)
    {
        /* TODO error */
        return false;
    }

    StringCchPrintfW(exec->args,
                     argsLen,
                     L"\"%*s\" \"%s\"%c%s",
                     (int) bufIdDigitCnt,
                     L"",
                     path,
                     L'\0',
                     path);

    /* Copy the buffer ID the hard way, because its type is not supported by
    ** *PrintfW or we don't know which specifier to use.
    */

    do
    {
        exec->args[bufIdDigitCnt--] = L'0' + (bufId % 10);
    }
    while (bufId /= 10, bufIdDigitCnt);

    exec->path = exec->args + wcslen(exec->args);

    return true;
}

Exec* allocExec(size_t pathLen, uptr_t bufIdDigitCnt, size_t *argsLen)
{
    Exec *exec;
    size_t len;

    assert(pathLen);
    assert(bufIdDigitCnt);
    assert(argsLen);

    /* Check if we can fit all the string data we need to store. The magic
        ** number 7 is the extra space required to store 4 double quotes ("),
        ** 1 space and 2 null characters (\0).
        */

    if (pathLen > (SIZE_MAX - 7) / 2
        || bufIdDigitCnt > SIZE_MAX - 7 - 2 * pathLen)
    {
        /* TODO error */
        return NULL;
    }

    len = 7 + 2 * pathLen + bufIdDigitCnt;

    if (len > (SIZE_MAX - sizeof *exec) / sizeof exec->args[0])
    {
        /* TODO error */
        return NULL;
    }

    /* Allocate memory for the struct and the trailing string buffer. */

    if (!(exec = allocMem(sizeof *exec + len * sizeof exec->args[0])))
    {
        /* TODO error */
        return NULL;
    }

    *argsLen = len;

    return exec;
}

uptr_t countBufIdDigits(uptr_t bufId)
{
    uptr_t cnt;

    cnt = 0;

    do
    {
        cnt++;
    }
    while (bufId /= 10);

    return cnt;
}

void CALLBACK timerProc(HWND wnd, UINT msg, UINT timerId, DWORD sysTime)
{
    unsigned int prevCnt;
    ExecState prevState;
    bool background;

    prevCnt = queue.size;
    prevState = queue.first->state;
    background = queue.first->rule->background;

    updateQueue();

    if (prevCnt != queue.size)
    {
        if (isQueueDlgVisible())
        {
            processQueueEvent(background ? QUEUE_REMOVE_BACKGROUND
                              : QUEUE_REMOVE_FOREGROUND);
        }
    }
    else if (prevState != queue.first->state)
    {
        if (isQueueDlgVisible())
            processQueueEvent(QUEUE_STATUS_UPDATE);
    }
}

Exec* getExecAt(int pos)
{
    Exec *exec;

    exec = queue.first;

    for (; pos; pos--)
        exec = exec->next;

    return exec;
}
