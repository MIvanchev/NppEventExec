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
#ifndef __RESOURCE_H__
#define __RESOURCE_H__

#ifndef VS_VERSION_INFO
#define VS_VERSION_INFO (1)
#endif

#ifndef IDC_STATIC
#define IDC_STATIC (-1)
#endif

#define IDB_BUTTONS          10
#define IDB_BUTTONS_MASKED   11
#define IDB_BUTTONS_DISABLED 12

#define IDD_ABOUT 100
#define IDD_QUEUE 101
#define IDD_RULES 102
#define IDD_EDIT  103

#define IDC_ST_VERSION           1000
#define IDC_BT_WEB_PAGE          1001
#define IDC_ST_COPYRIGHT         1002
#define IDC_ST_LICENSE           1003
#define IDC_BT_LICENSE           1004
#define IDC_ST_ICONS_USING       1005
#define IDC_BT_ICONS_SET         1006
#define IDC_ST_ICONS_BY          1007
#define IDC_BT_ICONS_AHASOFT     1008
#define IDC_ST_ICONS_DISTRIBUTED 1009
#define IDC_BT_ICONS_LICENSE     1010

#define IDC_LV_QUEUE  2000
#define IDC_BT_ABORT  2001
#define IDC_BT_MODE   2002
#define IDC_BT_CLOSE  2003
#define IDC_BT_CANCEL 2004

#define IDC_LV_RULES 3000
#define IDC_BT_SAVE  3001
#define IDC_BT_RESET 3002

#define IDC_ST_NAME          4000
#define IDC_ED_NAME          4001
#define IDC_ST_NAME_ERROR    4002
#define IDC_ST_EVENT         4003
#define IDC_CB_EVENT         4004
#define IDC_ST_REGEX         4005
#define IDC_ED_REGEX         4006
#define IDC_ST_REGEX_ERROR   4007
#define IDC_ST_COMMAND       4008
#define IDC_ED_COMMAND       4009
#define IDC_ST_COMMAND_ERROR 4010
#define IDC_BT_ENABLED       4011
#define IDC_BT_FOREGROUND    4012
#define IDC_BT_APPLY         4013

/* TODO: Check again why the IDs begin at 0x8000 and replace this comment with
** the info.
*/

#define ID_RULE_MOVEUP   0x8000
#define ID_RULE_MOVEDOWN 0x8001
#define ID_RULE_ADD      0x8002
#define ID_RULE_COPY     0x8003
#define ID_RULE_REMOVE   0x8004
#define ID_RULE_EDIT     0x8005

#define IDS_TOOLTIP_RULE_MOVEUP   1
#define IDS_TOOLTIP_RULE_MOVEDOWN 2
#define IDS_TOOLTIP_RULE_ADD      3
#define IDS_TOOLTIP_RULE_COPY     4
#define IDS_TOOLTIP_RULE_REMOVE   5
#define IDS_TOOLTIP_RULE_EDIT     6

#endif /* __RESOURCE_H__ */
