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
 

#ifndef _CHASSIS_MAINLOOP_H_
#define _CHASSIS_MAINLOOP_H_

#include <glib.h>    /* GPtrArray */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef HAVE_SYS_TIME_H
#include <sys/time.h>  /* event.h needs struct tm */
#endif
#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
#ifdef _WIN32
#include <winsock2.h>
#endif
#include <event.h>     /* struct event_base */

#include "chassis-exports.h"
#include "chassis-log.h"
#include "chassis-stats.h"
#include "chassis-shutdown-hooks.h"

/**
 * chassis_private:
 *
 * "global" data for network_mysqld_con_handle()
 */
typedef struct chassis_private chassis_private;
typedef struct chassis chassis;
typedef struct chassis_event_threads_t chassis_event_threads_t;

/**
 * chassis_priv_shutdown_func:
 * @chas: the chassis
 * @priv: the private data
 *
 * function called when chas is shutdown
 */
typedef void (*chassis_priv_shutdown_func)(chassis *chas, chassis_private *priv);

/**
 * chassis_priv_free_func:
 * @chas: the chassis
 * @priv: the private data
 *
 * function called when private data shall be freed
 */
typedef void (*chassis_priv_free_func)(chassis *chas, chassis_private *priv);

/**
 * chassis:
 * @modules: (element-type chassis_plugin): plugins
 * @base_dir: base directory for all relative paths referenced
 * @user: user to run as
 * @stats: the overall chassis stats, includes lua and glib allocation stats
 * @log: the log
 * @event_thread_count: number of events threas to start
 * @threads: the event threads
 * @shutdown_hooks: the shutdown hooks
 * @priv: local/global data if the chassis users
 * @priv_shutdown: shutdown function for @priv
 * @priv_free: free function for @priv
 *
 * global context of the chassis
 */
struct chassis {
	struct event_base *event_base;
	gchar *event_hdr_version;

	GPtrArray *modules;

	gchar *base_dir;
	gchar *user;

	chassis_private *priv;
	chassis_priv_shutdown_func priv_shutdown;
	chassis_priv_free_func     priv_free;

	chassis_log_t *log;
	
	chassis_stats_t *stats;

	/* network-io threads */
	gint event_thread_count;

	chassis_event_threads_t *threads;

	chassis_shutdown_hooks_t *shutdown_hooks;
};

#ifndef CHASSIS_DISABLE_DEPRECATED
CHASSIS_API chassis *chassis_init(void) G_GNUC_DEPRECATED;
#endif
CHASSIS_API chassis *chassis_new(void);
CHASSIS_API void chassis_free(chassis *chas);
CHASSIS_API int chassis_check_version(const char *lib_version, const char *hdr_version);

CHASSIS_API int chassis_mainloop(void *user_data);

CHASSIS_API void chassis_set_shutdown_location(const gchar* location);
CHASSIS_API gboolean chassis_is_shutdown(void);

/**
 * chassis_set_shutdown:
 *
 * set the chassis into shutdown mode
 *
 * See: chassis_set_shutdown_location()
 */
#define chassis_set_shutdown() chassis_set_shutdown_location(G_STRLOC)

#endif
