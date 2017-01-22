/*
This file is part of NppEventExec.
Copyright (C) 2016 Mihail Ivanchev

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
#include "mem.h"
#include "plugin.h"
#include "rule.h"
#include "util.h"
#include "Notepad_plus_msgs.h"

/** TODO doc */
#define FILENAME PLUGIN_NAME L"_rules.csv"

static Rule* copyRule(const Rule *copy);

int readRules(Rule **rules)
{
    wchar_t *path;
    DWORD attribs;
    Rule *list;
    Rule *last;
    Rule *rule;
    int res;
    int numRules;

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
    if (csvOpen(path, 1, 6))
    {
        /* TODO error */
        goto fail_open;
    }

    list = NULL;
    /* Get rid of a 'maybe uninitialized warning. */
    last = NULL;

    numRules = 0;

    for(;; )
    {
        if ((res = csvHasChars()) == -1)
        {
            /* TODO error */
            goto fail_chars;
        }
        if (!res)
            break;
        if (numRules == INT_MAX)
        {
            /* TODO error */
            goto fail_too_many_rules;
        }
        if (!(rule = allocMem(sizeof(Rule))))
        {
            /* TODO error */
            goto fail_rule;
        }
        if (csvGetEvent(&rule->event))
        {
            /* TODO error */
            goto fail_event;
        }
        if ((rule->enabled = csvGetBool()) == -1)
        {
            /* TODO error */
            goto fail_enabled;
        }
        if (!(rule->name = csvGetString(0, RULE_MAX_NAME_LEN)))
        {
            /* TODO error */
            goto fail_name;
        }
        if (!(rule->regex = csvGetString(1, RULE_MAX_REGEX_LEN)))
        {
            /* TODO error */
            goto fail_regex;
        }
        if (!(rule->cmd = csvGetString(0, RULE_MAX_CMD_LEN)))
        {
            /* TODO error */
            goto fail_cmd;
        }
        if ((rule->background = csvGetBool()) == -1)
        {
            goto fail_background;
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

        numRules++;
    }

    csvClose();
    freeStr(path);
    *rules = list;
    return 0;

fail_background:
fail_cmd:
    freeStr(rule->regex);
fail_regex:
    freeStr(rule->name);
fail_name:
fail_enabled:
fail_event:
    freeMem(rule);
fail_rule:
fail_too_many_rules:
fail_chars:
    freeRules(list);
    csvClose();
fail_open:
    freeStr(path);
fail_attribs:
fail_path:
    return 1;
}

void freeRules(Rule *rules)
{
    Rule *rule;
    Rule *next;

    rule = rules;
    while (rule)
    {
        next = rule->next;
        freeStr(rule->name);
        freeStr(rule->regex);
        freeStr(rule->cmd);
        freeMem(rule);
        rule = next;
    }
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
