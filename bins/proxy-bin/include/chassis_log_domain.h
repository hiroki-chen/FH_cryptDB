#ifndef __CHASSIS_LOG_DOMAIN_H__
#define __CHASSIS_LOG_DOMAIN_H__

/* previously known as _extended_logger_t */

#include <glib.h>

#include "chassis-exports.h"

#include "chassis_log_backend.h" /* the backends like syslog, file, stderr, ... */

/**
 * CHASSIS_LOG_DEFAULT_DOMAIN:
 *
 * default log domain (all)
 *
 */
#define CHASSIS_LOG_DEFAULT_DOMAIN ""

typedef struct chassis_log_domain chassis_log_domain_t;
/**
 * chassis_log_domain_t:
 * @name: the full name of this logger
 * @min_level: the minimum log level for this logger
 * @effective_level: the effective log level, calculated from min_level and its parent's min_leve
 * @is_implicit: this logger hasn't been explicitly set, but is part of a hierarchy chai
 * @is_autocreated: this logger has been created in response to a write to a non-existing logger
 * @backend: backend for this logger, essentially the fd and write function
 * @parent: our parent in the hierarchy, NULL for the root logger
 * @children: the list of loggers directly below us in the hierarchy
 *
 * A logger describes the attributes of a point in the logging hierarchy, such as the effective log level
 * and the backend the messages go to.
 */
struct chassis_log_domain {
	gchar *name;
	GLogLevelFlags min_level;
	GLogLevelFlags effective_level;
	gboolean is_implicit;
	gboolean is_autocreated;
	chassis_log_backend_t *backend;
	
	struct chassis_log_domain *parent;
	GPtrArray *children;
};

CHASSIS_API chassis_log_domain_t* chassis_log_domain_new(const gchar *domain_name, GLogLevelFlags min_level, chassis_log_backend_t *backend);
CHASSIS_API void chassis_log_domain_free(chassis_log_domain_t* domain);

CHASSIS_API void chassis_log_domain_log(chassis_log_domain_t* domain, GLogLevelFlags level, const gchar *message);

#endif
