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
#ifndef __RULES_H__
#define __RULES_H__

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Opens a modal dialog which allows the user to create, modify, delete, reorder
 * etc. rules.
 * \param activeRules a pointer to a variable holding the rules currently used
 *        by the plugin; the list will be overwritten by the dialog after the
 *        changes are confirmed and all currently scheduled rules are executed
 *        or or aborted.
 * \param rule the rule that the user will be editing.
 * \return 1 when the dialog is closed.
 * \return 0 upon an error.
 * \return -1 upon an error.
 */
int openRulesDlg(Rule **activeRules);

#ifdef __cplusplus
}
#endif

#endif /* __RULES_H__ */