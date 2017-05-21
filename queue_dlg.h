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
#ifndef __QUEUE_DLG_H__
#define __QUEUE_DLG_H__

#ifdef __cplusplus
extern "C" {
#endif

/**
 * TODO
 * \param waitForAll TODO
 * \param autoClose set to true if the dialog should automatically close when
 *                  all rules of the required group are executed.
 * \param cancelable TODO
 */
INT_PTR openQueueDlg(HWND parent,
                     /*const wchar_t *title,*/
                     /*const wchar_t *msg,*/
                     bool waitForAll,
                     bool autoClose,
                     bool cancelable);

int isQueueDlgVisible(void);

#ifdef __cplusplus
}
#endif

#endif /* __EXEC_DLG_H__ */
