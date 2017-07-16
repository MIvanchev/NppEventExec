@echo off

rem This file is part of NppEventExec
rem Copyright (C) 2016-2017 Mihail Ivanchev
rem
rem This program is free software: you can redistribute it and/or modify
rem it under the terms of the GNU General Public License as published by
rem the Free Software Foundation, either version 3 of the License, or
rem (at your option) any later version.
rem
rem This program is distributed in the hope that it will be useful,
rem but WITHOUT ANY WARRANTY; without even the implied warranty of
rem MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
rem GNU General Public License for more details.
rem
rem You should have received a copy of the GNU General Public License
rem along with this program. If not, see <http://www.gnu.org/licenses/>.

set CRITERION_INC_PATH=..\..\..\..\Libs\C\Criterion\include
set CRITERION_LIB_PATH=..\..\..\..\Libs\C\Criterion\build
set EXE=tests.exe

gcc -g -DDEBUG -I%CRITERION_INC_PATH% -I.. -L%CRITERION_LIB_PATH% -o %EXE% csv.c test.c ..\csv.c ..\mem.c ..\util.c ..\event_map.c ..\utf8.c -lcriterion
if %errorlevel% neq 0 exit /b %errorlevel%

%EXE% --ascii %1 %2 %3 %4 %5 %6 %7 %8 %9

set RESULT=%errorlevel%

del %EXE%
exit /b %RESULT%
