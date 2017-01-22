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
#ifndef __EXEC__H__
#define __EXEC__H__

#ifdef __cplusplus
extern "C" {
#endif

int execRule(uptr_t bufferId, const wchar_t *path, const Rule *rule);
int openQueueDlg(void);

#ifdef __cplusplus
}
#endif

#endif /* __EXEC__H__ */
