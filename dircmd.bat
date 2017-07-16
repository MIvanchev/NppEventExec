@echo off

REM This file is part of NppEventExec
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

rem
rem An auxiliary shell script supplementing the project's makefile the purpose
rem of which is to privde an access to the internal shell commands mkdir and
rem rmdir so the makefile doesn't have to depend on external programs.
rem

goto cmd_%1

:cmd_mkdir
@echo on
mkdir %2
@echo off
if %errorlevel% neq 0 exit /b %errorlevel%
goto done

:cmd_rmdir
@echo on
rmdir /q /s %2
@echo off
if %errorlevel% neq 0 exit /b %errorlevel%
goto done

:done
