/* $%BEGINLICENSE%$
 Copyright (c) 2010, Oracle and/or its affiliates. All rights reserved.

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

#ifndef __CHASSIS_FRONTEND_H__
#define __CHASSIS_FRONTEND_H__

/**
 * CHASSIS_NEWLINE:
 *
 * newline string on the current platform
 */
#ifdef WIN32
#define CHASSIS_NEWLINE "\r\n"
#else
#define CHASSIS_NEWLINE "\n"
#endif

#include <glib.h>

#include "chassis-exports.h"
#include "chassis-options.h"


CHASSIS_API int chassis_frontend_init_glib(void);
CHASSIS_API int chassis_frontend_init_win32(void);
CHASSIS_API int chassis_frontend_init_basedir(const char *prg_name, char **_base_dir);

CHASSIS_API GKeyFile *chassis_frontend_open_config_file(const char *filename, GError **gerr);

CHASSIS_API int chassis_frontend_init_plugin_dir(char **_plugin_dir, const char *base_dir);
CHASSIS_API int chassis_frontend_init_lua_path(const char *set_path, const char *base_dir, char **lua_subdirs);
CHASSIS_API int chassis_frontend_init_lua_cpath(const char *set_path, const char *base_dir, char **lua_subdirs);

CHASSIS_API int chassis_frontend_init_base_options(GOptionContext *option_ctx,
		int *argc_p, char ***argv_p,
		int *print_version,
		char **config_file,
		GError **gerr);

CHASSIS_API int chassis_frontend_load_plugins(GPtrArray *plugins,
		const gchar *plugin_dir,
		gchar **plugin_names);

CHASSIS_API int chassis_frontend_init_plugins(GPtrArray *plugins,
		GOptionContext *option_ctx,
		int *argc_p, char ***argv_p,
		GKeyFile *keyfile,
		const char *keyfile_section_name,
		const char *base_dir,
		GError **gerr);

CHASSIS_API int chassis_frontend_print_version(void);

CHASSIS_API int chassis_frontend_print_plugin_versions(GPtrArray *plugins);

CHASSIS_API void chassis_frontend_print_lua_version(void);

CHASSIS_API int chassis_frontend_log_plugin_versions(GPtrArray *plugins);

CHASSIS_API int chassis_frontend_write_pidfile(const char *pid_file, GError **gerr);

CHASSIS_API int chassis_options_set_cmdline_only_options(chassis_options_t *opts,
		int *print_version,
		char **config_file);

#endif
