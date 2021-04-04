/* $%BEGINLICENSE%$
 Copyright (c) 2009, Oracle and/or its affiliates. All rights reserved.

 This program is free software; you can redistribute it and/or
 modify it under the terms of the GNU General Public License as
 published by the Free Software Foundation; version 2 of the
 License.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with this program; if not, write to the Free Software
 Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 02110-1301  USA

 $%ENDLICENSE%$ */
 

#ifndef _GLIB_EXT_STRING_LEN_H_
#define _GLIB_EXT_STRING_LEN_H_

/**
 * SECTION:string-len
 * @short_description: simple macros to get the data and length of a "string"
 *
 * Quite often you need to pass a string and its length to a function.
 *
 * While you could use strlen() all to time alot easier to gather its length at compile time instead.
 *
 * |[
 * GString *str;
 * ]|
 *
 * can either use strlen()
 *
 * |[
 * g_string_append_len(str, "foo", strlen("foo"));
 * ]|
 * or
 * |[
 * g_string_append_len(str, "foo", sizeof("foo") - 1);
 * ]|
 *
 * which is what the macro C() does:
 *
 * |[
 * g_string_append_len(str, C("foo"));
 * ]|
 *
 * In the case of #GString the length is also known at runtime by #GString.len which the macro S() builds on.
 */

/**
 * C:
 * @x: string
 *
 * get value and length of a static string
 */
#define C(x) x, x ? sizeof(x) - 1 : 0

/**
 * S:
 * @x: a #GString
 *
 * get value and length of a #GString
 */
#define S(x) (x) ? (x)->str : NULL, (x) ? (x)->len : 0

#endif
