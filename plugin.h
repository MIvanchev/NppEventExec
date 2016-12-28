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
#ifndef __PLUGIN_H__
#define __PLUGIN_H__

#ifdef __cplusplus
extern "C" {
#endif

int isPluginInit(void);
wchar_t* getPluginDir(void);
wchar_t* getPluginConfigDir(void);
HINSTANCE getPluginInstance(void);
LRESULT sendNppMsg(UINT msg, WPARAM wp, LPARAM lp);
LRESULT sendNppExecMsg(long msg, void *info);
HWND getNppWnd(void);

#ifdef __cplusplus
}
#endif

#endif /* __PLUGIN_H__ */
