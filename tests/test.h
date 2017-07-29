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
#ifndef __TEST_H__
#define __TEST_H__

#include <assert.h>
#include <criterion/criterion.h>
#include <criterion/new/assert.h>
#include "base.h"

#define HEAD(arg, ...) arg

#define TAIL(...) \
    TAIL_(ONE_OR_MORE(__VA_ARGS__), __VA_ARGS__)

#define TAIL_(arg, ...)  TAIL__(arg, __VA_ARGS__)
#define TAIL__(arg, ...) TAIL_WHEN_ ## arg(__VA_ARGS__)
#define TAIL_WHEN_ONE(...)
#define TAIL_WHEN_MORE(arg, ...) , __VA_ARGS__

#define ONE_OR_MORE(...)                                        \
    LAST(__VA_ARGS__, MORE, MORE, MORE, MORE, MORE, MORE, MORE, \
         MORE, MORE, MORE, MORE, MORE, MORE, MORE, MORE,        \
         MORE, MORE, MORE, MORE, MORE, MORE, MORE, MORE,        \
         MORE, MORE, MORE, MORE, MORE, MORE, MORE, ONE)

#define LAST(arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8,         \
             arg9, arg10, arg11, arg12, arg13, arg14, arg15, arg16,  \
             arg17, arg18, arg19, arg20, arg21, arg22, arg23, arg24, \
             arg25, arg26, arg27, arg28, arg29, arg30, arg31, arg32, ...) arg32

#define ASSERT_FILENAME_ID custAssertFile
#define ASSERT_LINENUM_ID  custAssertLineNum

#define declare_assert(...) static define_assert(__VA_ARGS__)

#define define_assert(...) \
    define_assert_(HEAD(__VA_ARGS__), __VA_ARGS__)
#define define_assert_(arg, ...) define_assert__(arg, __VA_ARGS__)
#define define_assert__(arg, ...)                            \
    void assert_proc_ ## arg(const char *ASSERT_FILENAME_ID, \
                             int ASSERT_LINENUM_ID TAIL(__VA_ARGS__))

#define call_assert_proc(...) \
    call_assert_proc_(HEAD(__VA_ARGS__), __VA_ARGS__)
#define call_assert_proc_(...) call_assert_proc__(__VA_ARGS__)
#define call_assert_proc__(arg, ...) \
    assert_proc_ ## arg(__FILE__, __LINE__ TAIL(__VA_ARGS__))

#define cr_assert_failure(...)                \
    cr_assert_fail_user(ASSERT_FILENAME_ID,   \
                        ASSERT_LINENUM_ID,    \
                        criterion_abort_test, \
                        __VA_ARGS__)

#endif /* __TEST_H__ */
