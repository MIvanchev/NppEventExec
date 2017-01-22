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
#ifndef __RULE_H__
#define __RULE_H__

typedef struct _Rule
{
    int enabled;
    int background;
    unsigned int event;
    wchar_t *name;
    wchar_t *regex;
    wchar_t *cmd;
    struct _Rule *next;
} Rule;

/** The maximum length of the rule's name in characters. */
#define RULE_MAX_NAME_LEN 256

/** The maximum length of the rule's regex in characters. */
#define RULE_MAX_REGEX_LEN 256

/** The maximum legth of the rule's command in characters. */
#define RULE_MAX_CMD_LEN 2048

#ifdef __cplusplus
extern "C" {
#endif

int readRules(Rule **rules);
void freeRules(Rule *rules);
Rule* copyRules(const Rule *rules, Rule **last);
Rule* getRuleAt(Rule *rule, int pos);
int getRuleCount(const Rule *rules);

#ifdef DEBUG
void printRules(Rule *rules);
#endif

#ifdef __cplusplus
}
#endif

#endif /* __RULE_H__ */
