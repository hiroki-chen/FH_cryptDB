/* $%BEGINLICENSE%$
 Copyright (c) 2007, 2010, Oracle and/or its affiliates. All rights reserved.

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
 

#ifndef _CHASSIS_PLUGIN_H_
#define _CHASSIS_PLUGIN_H_

#include <glib.h>
#include <gmodule.h>

#include "chassis-mainloop.h"
#include "chassis-exports.h"

/**
 * CHASSIS_PLUGIN_MAGIC:
 *
 * magic of current plugin interface
 *
 * chassis_plugin_new() sets the magic in its #chassis_plugin.magic at compile time. 
 * chassis_plugin_load() reads it back and compares the plugin's magic against the current
 * magic to check if the interfaces match.
 *
 * See: chassis_plugin_load()
 */
/* current magic is 0.8.0-1 */
#define CHASSIS_PLUGIN_MAGIC 0x00080001L

/**
 * chassis_plugin_stats_t:
 *
 * The private stats structure of a plugin. This is opaque to the rest of the code,
 * we can only get a copy of it in a hash.
 *
 * See: #chassis_plugin_stats.get_stats()
 */
typedef struct chassis_plugin_stats chassis_plugin_stats_t;

/**
 * chassis_plugin_config:
 *
 * opaque config of a plugin
 */
typedef struct chassis_plugin_config chassis_plugin_config;

/**
 * chassis_plugin:
 * @magic: a magic token to verify that the plugin API matches
 * @option_grp_name: name of the option group (used in --help-$option-grp
 * @name: user visible name of this plugin
 * @version: the plugin's version number
 * @module: the plugin handle when loaded
 * @stats: contains the plugin-specific statistics
 * @new_stats: handler function to initialize the plugin-specific stats
 * @free_stats: handler function to dealloc the plugin-specific stats
 * @get_stats: handler function to retrieve the plugin-specific stats
 * @config: contains the plugin-specific config data
 * @init: handler function to allocate/initialize a chassis_plugin_config struct
 * @destroy: handler function used to deallocate the chassis_plugin_config
 * @get_options: get options of the plugin
 * @apply_config: handler function to set the argument values in the plugin's config
 * @get_global_state: handler function to retrieve the plugin's global state
 *
 * the chassis plugin
 */
typedef struct chassis_plugin {
	glong     magic;

	gchar    *option_grp_name;
	gchar    *name;
	gchar    *version;
	GModule  *module;
	
	chassis_plugin_stats_t *stats;

	chassis_plugin_stats_t *(*new_stats)(void);
	void (*free_stats)(chassis_plugin_stats_t *user_data);
	GHashTable *(*get_stats)(chassis_plugin_stats_t *user_data);

	chassis_plugin_config *config;

	chassis_plugin_config *(*init)(void);
	void     (*destroy)(chassis_plugin_config *user_data);
	GOptionEntry * (*get_options)(chassis_plugin_config *user_data);
	int      (*apply_config)(chassis *chas, chassis_plugin_config * user_data);
	void*    (*get_global_state)(chassis_plugin_config *user_data, const char* member);
} chassis_plugin;

#ifndef CHASSIS_DISABLE_DEPRECATED
CHASSIS_API chassis_plugin *chassis_plugin_init(void) G_GNUC_DEPRECATED;
#endif
CHASSIS_API chassis_plugin *chassis_plugin_new(void);
CHASSIS_API chassis_plugin *chassis_plugin_load(const gchar *name);
CHASSIS_API void chassis_plugin_free(chassis_plugin *p);
CHASSIS_API GOptionEntry * chassis_plugin_get_options(chassis_plugin *p);

CHASSIS_API chassis_plugin* chassis_plugin_for_name(chassis *chas, gchar* plugin_name);

#endif
