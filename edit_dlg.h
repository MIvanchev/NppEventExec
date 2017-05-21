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
#ifndef __EDIT_DLG_H__
#define __EDIT_DLG_H__

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Opens a modal dialog which allows the user to edit the attributes of a
 * rule.
 * \param parent the parent window of the edit dialog.
 * \param rule the rule that the user will be editing.
 * \return 0 if the rule was <b>NOT<\b> changed by the user.
 * \return 1 if the rule was changed by the user.
 * \return -1 upon an error.
 */
int openEditDlg(HWND parent, Rule *rule);

#ifdef __cplusplus
}
#endif

#endif /* __EDIT_DLG_H__ */
