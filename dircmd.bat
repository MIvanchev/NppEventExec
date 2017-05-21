@echo off

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
