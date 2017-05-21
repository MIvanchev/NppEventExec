/*
This file is part of NppEventExec.
Copyright (C) 2016-2017 Mihail Ivanchev

NppEventExec is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

NppEventExec is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with NppEventExec. If not, see <http://www.gnu.org/licenses/>.
*/
#include "base.h"
#include "event_map.h"
#include "csv.h"
#include "match.h"
#include "mem.h"
#include "plugin.h"
#include "rule.h"
#include "util.h"
#include "Notepad_plus_msgs.h"

/** TODO doc */
#define FILENAME PLUGIN_NAME L"_rules.csv"

typedef struct
{
    wchar_t *header;
    int (*reader)(Rule*);
    int (*writer)(Rule*);
} CsvField;

static int readEvent(Rule *rule);
static int writeEvent(Rule *rule);
static int readEnabled(Rule *rule);
static int writeEnabled(Rule *rule);
static int readName(Rule *rule);
static int writeName(Rule *rule);
static int readRegex(Rule *rule);
static int writeRegex(Rule *rule);
static int readCmd(Rule *rule);
static int writeCmd(Rule *rule);
static int readBackground(Rule *rule);
static int writeBackground(Rule *rule);

static CsvField fields[] = {
    { L"Event", readEvent, writeEvent },
    { L"Enabled?", readEnabled, writeEnabled},
    { L"Name", readName, writeName },
    { L"Regex", readRegex, writeRegex },
    { L"Command", readCmd, writeCmd },
    { L"Background?", readBackground, writeBackground }
};

int readRules(Rule **rules)
{
    wchar_t *path;
    DWORD attribs;
    Rule *list;
    Rule *last;
    Rule *rule;
    int ruleCnt;
    size_t ii;

    assert(rules);

    if (!(path = combinePaths(getPluginConfigDir(), FILENAME)))
    {
        /* TODO error */
        goto fail_path;
    }
    if ((attribs = GetFileAttributesW(path)) == INVALID_FILE_ATTRIBUTES)
    {
        if (GetLastError() == ERROR_FILE_NOT_FOUND)
        {
            freeStr(path);
            *rules = NULL;
            return 0;
        }

        /* TODO error */
        goto fail_attribs;
    }

    if (csvOpen(path, BUFLEN(fields), 1))
    {
        /* TODO error */
        goto fail_open;
    }

    list = NULL;
    /* Get rid of a 'maybe uninitialized warning. */
    last = NULL;

    ruleCnt = 0;

    while (csvHasData())
    {
        if (ruleCnt == INT_MAX)
        {
            /* TODO error */
            goto fail_too_many_rules;
        }
        if (!(rule = allocMem(sizeof(Rule))))
        {
            /* TODO error */
            goto fail_rule;
        }

        rule->name = NULL;
        rule->regex = NULL;
        rule->cmd = NULL;

        for (ii = 0; ii < BUFLEN(fields); ii++)
        {
            if (fields[ii].reader(rule))
                goto fail_read;
        }

        rule->next = NULL;

        if (!list)
        {
            list = rule;
            last = rule;
        }
        else
        {
            last->next = rule;
            last = last->next;
        }

        ruleCnt++;
    }

    csvClose();
    freeStr(path);
    *rules = list;
    return 0;

fail_read:
    freeStr(rule->name);
    freeStr(rule->regex);
    freeStr(rule->cmd);
    freeMem(rule);
fail_rule:
fail_too_many_rules:
    freeRules(list);
    csvClose();
fail_open:
    freeStr(path);
fail_attribs:
fail_path:
    return 1;
}

int readEvent(Rule *rule)
{
    return csvReadEvent(&rule->event);
}

int writeEvent(Rule *rule)
{
    return csvWriteEvent(rule->event);
}

int readEnabled(Rule *rule)
{
    int res;

    if ((res = csvReadBool()) < 0)
    {
        /* TODO error */
        return 1;
    }

    rule->enabled = res;

    return 0;
}

int writeEnabled(Rule *rule)
{
    return csvWriteBool(rule->enabled, BOOL_YES_NO);
}

int readName(Rule *rule)
{
    wchar_t *res;
    size_t unitCnt;
    size_t charCnt;

    if (!(res = csvReadString(&unitCnt, &charCnt)))
    {
        /* TODO error */
        return 1;
    }
    if (!charCnt)
    {
        /* TODO error */
        freeStr(res);
        return 1;
    }
    if (IS_SPACE(*res))
    {
        /* TODO error */
        freeStr(res);
        return 1;
    }
    if (IS_SPACE(res[unitCnt]))
    {
        /* TODO error */
        freeStr(res);
        return 1;
    }

    rule->name = res;
    return 0;
}

int writeName(Rule *rule)
{
    return csvWriteString(rule->name);
}

int readRegex(Rule *rule)
{
    wchar_t *res;
    size_t unitCnt;
    size_t charCnt;

    if (!(res = csvReadString(&unitCnt, &charCnt)))
    {
        /* TODO error */
        return 1;
    }
    if (!isValidRegex(res))
    {
        /* TODO error */
        freeStr(res);
        return 1;
    }

    rule->regex = res;
    return 0;
}

int writeRegex(Rule *rule)
{
    return csvWriteString(rule->regex);
}

int readCmd(Rule *rule)
{
    wchar_t *res;
    size_t unitCnt;
    size_t charCnt;

    if (!(res = csvReadString(&unitCnt, &charCnt)))
    {
        /* TODO error */
        return 1;
    }
    if (!charCnt)
    {
        /* TODO error */
        freeStr(res);
        return 1;
    }
    if (IS_SPACE(*res))
    {
        /* TODO error */
        freeStr(res);
        return 1;
    }

    rule->cmd = res;
    return 0;
}

int writeCmd(Rule *rule)
{
    return csvWriteString(rule->cmd);
}

int readBackground(Rule *rule)
{
    int res;

    if ((res = csvReadBool()) < 0)
    {
        /* TODO error */
        return 1;
    }

    rule->background = res;

    return 0;
}

int writeBackground(Rule *rule)
{
    return csvWriteBool(rule->background, BOOL_YES_NO);
}

int writeRules(Rule *rules)
{
    wchar_t tmpDirPath[MAX_PATH + 1];
    wchar_t *tmpPath;
    wchar_t *path;
    size_t len;
    Rule *rule;
    size_t ii;

    assert(rules);

    if (!(len = GetTempPathW(BUFLEN(tmpDirPath), tmpDirPath)))
    {
        /* TODO error */
        goto fail_tmp_dir;
    }

    tmpDirPath[len - 1] = L'\0';

    if (!(tmpPath = combinePaths(tmpDirPath, FILENAME)))
    {
        /* TODO error */
        goto fail_tmp_path;
    }
    if (csvCreate(tmpPath, BUFLEN(fields)))
    {
        /* TODO error */
        goto fail_create;
    }

    for (ii = 0; ii < BUFLEN(fields); ii++)
    {
        if (csvWriteString(fields[ii].header))
            goto fail_write;
    }

    for (rule = rules; rule; rule = rule->next)
    {
        for (ii = 0; ii < BUFLEN(fields); ii++)
        {
            if (fields[ii].writer(rule))
                goto fail_write;
        }
    }

    if (csvFlush())
    {
        /* TODO error */
        goto fail_flush;
    }

    csvClose();

    if (!(path = combinePaths(getPluginConfigDir(), FILENAME)))
    {
        /* TODO error */
        goto fail_path;
    }
    if (!CopyFileW(tmpPath, path, FALSE))
    {
        /* TODO error */
        goto fail_copy;
    }
    if (!DeleteFileW(tmpPath))
    {
        /* TODO warning */
    }

    freeStr(tmpPath);
    freeStr(path);

    return 0;

fail_copy:
    freeStr(path);
fail_path:
fail_flush:
fail_write:
    csvClose();
fail_create:
    freeStr(tmpPath);
fail_tmp_path:
fail_tmp_dir:
    return 1;
}

void freeRule(Rule *rule)
{
    freeStr(rule->name);
    freeStr(rule->regex);
    freeStr(rule->cmd);
    freeMem(rule);
}

void freeRules(Rule *rules)
{
    Rule *rule;
    Rule *next;

    rule = rules;
    while (rule)
    {
        next = rule->next;
        freeRule(rule);
        rule = next;
    }
}

Rule* copyRule(const Rule *rule)
{
    Rule *copy;

    if (!(copy = allocMem(sizeof(Rule))))
    {
        /* TODO error */
        goto fail_rule;
    }
    if (!(copy->name = copyStr(rule->name)))
    {
        /* TODO error */
        goto fail_name;
    }
    if (!(copy->regex = copyStr(rule->regex)))
    {
        /* TODO error */
        goto fail_regex;
    }
    if (!(copy->cmd = copyStr(rule->cmd)))
    {
        /* TODO error */
        goto fail_cmd;
    }

    copy->event = rule->event;
    copy->enabled = rule->enabled;
    copy->background = rule->background;
    copy->next = NULL;

    return copy;

fail_cmd:
    freeStr(copy->regex);
fail_regex:
    freeStr(copy->name);
fail_name:
    freeMem(copy);
fail_rule:
    return NULL;
}

Rule* copyRules(const Rule *rules, Rule **last)
{
    Rule *copiedRules;
    Rule *copiedLast;
    Rule *copy;
    const Rule *rule;

    copiedRules = NULL;
    copiedLast = NULL;

    for (rule = rules; rule; rule = rule->next)
    {
        if (!(copy = copyRule(rule)))
        {
            /* TODO error */
            freeRules(copiedRules);
            return NULL;
        }

        if (!copiedRules)
        {
            copiedRules = copy;
            copiedLast = copiedRules;
        }
        else
        {
            copiedLast->next = copy;
            copiedLast = copiedLast->next;
        }
    }

    *last = copiedLast;
    return copiedRules;
}

Rule* getRuleAt(Rule *rules, int pos)
{
    Rule *rule;

    rule = rules;

    for (; pos; pos--)
        rule = rule->next;

    return rule;
}

int getRuleCount(const Rule *rules)
{
    int cnt;

    cnt = 0;

    for (; rules; rules = rules->next)
        cnt++;

    return cnt;
}

#ifdef DEBUG
void printRules(Rule *rules)
{
    Rule *rr;

    wprintf(
        L"************************************ RULES ************************************\r\n");
    wprintf(L"\r\n");

    for (rr = rules; rr; rr = rr->next)
    {
        wprintf(L"Event:      %ls\r\n", getEventMapEntry(rr->event)->name);
        wprintf(L"Enabled:    %ls\r\n", rr->enabled ? L"true" : L"false");
        wprintf(L"Name:       %ls\r\n", rr->name);
        wprintf(L"Regex:      %ls\r\n", rr->regex);
        wprintf(L"Command:    %ls\r\n", rr->cmd);
        wprintf(L"Background: %ls\r\n", rr->background ? L"true" : L"false");

        if (rr->next)
            wprintf(L"\r\n");
    }

    wprintf(L"\r\n");
    wprintf(
        L"*******************************************************************************\r\n");
}
#endif /* ifdef DEBUG */
