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
  

#ifndef _CHASSIS_LOG_H_
#define _CHASSIS_LOG_H_

#include <glib.h>

#include "chassis-exports.h"

#include "chassis_log_domain.h" /* the log-domains like 'chassis.network' */
#include "chassis_log_backend.h" /* the backends like syslog, file, stderr, ... */

/**
 * chassis_log_t:
 * @domains: (element-type gchar chassis_log_domain_t): a map of all available domains, keyed by domain name
 * @backends: (element-type gchar chassis_log_backend_t): a map of all available backends, keyed by file name
 *
 * The log stores the available hierarchical domains and manages operations on them.
 * 
 * It supports registration and rotation of domains and force-broadcasting a message to all logs.
 * 
 * Note: This is currently not threadsafe when manipulating the available domains!
 *
 * Since: 0.9.0
 */
typedef struct {
	/*< private >*/
	gboolean rotate_logs;

	/*< public >*/
	GHashTable *domains;
	GHashTable *backends;
} chassis_log_t;

#ifndef CHASSIS_DISABLE_DEPRECATED
/**
 * chassis_log:
 *
 * the log
 *
 * Deprecated: 0.9.0: use chassis_log instead
 */
typedef chassis_log_t chassis_log
#ifndef __GTK_DOC_IGNORE__
	G_GNUC_DEPRECATED
#endif
;

CHASSIS_API chassis_log_t *chassis_log_init(void) G_GNUC_DEPRECATED;
#endif
CHASSIS_API chassis_log_t *chassis_log_new(void);
CHASSIS_API GLogLevelFlags chassis_log_level_string_to_level(const gchar *level);
CHASSIS_API void chassis_log_free(chassis_log_t *log);
CHASSIS_API void chassis_log_set_logrotate(chassis_log_t *log);
CHASSIS_API const char *chassis_log_skip_topsrcdir(const char *message);

CHASSIS_API gboolean chassis_log_register_backend(chassis_log_t *log, chassis_log_backend_t *backend);

CHASSIS_API gboolean chassis_log_register_domain(chassis_log_t *log, chassis_log_domain_t *domain);
CHASSIS_API void chassis_log_unregister_domain(chassis_log_t *log, chassis_log_domain_t *domain);
CHASSIS_API chassis_log_domain_t* chassis_log_get_domain(chassis_log_t *log, const gchar *domain_name);
CHASSIS_API void chassis_log_reopen(chassis_log_t* log);
CHASSIS_API void chassis_log_force_log_all(chassis_log_t* log, const gchar *message);
CHASSIS_API GLogLevelFlags chassis_log_get_effective_level(chassis_log_t *log, const gchar *domain_name);

CHASSIS_API void chassis_log_func(const gchar *log_domain, GLogLevelFlags log_level, const gchar *message, gpointer user_data);

/* utility functions */
CHASSIS_API gchar** chassis_log_extract_hierarchy_names(const gchar *domain_name, gsize *len);

CHASSIS_API gboolean chassis_log_load_config(chassis_log_t *log, const gchar *file_name, GError **gerr);

#ifndef _WIN32
CHASSIS_API gboolean chassis_log_chown(chassis_log_t *log, uid_t uid, gid_t gid, GError **gerr);
#endif

CHASSIS_API int chassis_log_set_default(chassis_log_t *log, const char *log_filename, GLogLevelFlags log_lvl);

/*@}*/

#endif
