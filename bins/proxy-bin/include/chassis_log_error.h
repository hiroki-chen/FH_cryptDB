#ifndef __CHASSIS_LOG_ERROR_H__
#define __CHASSIS_LOG_ERROR_H__

#include <glib.h>

/**
 * chassis_log_error_t:
 * @CHASSIS_LOG_ERROR_NO_GROUP: no group
 * @CHASSIS_LOG_ERROR_NO_BACKENDS: no backeds
 * @CHASSIS_LOG_ERROR_NO_LOGLEVEL: no log level
 * @CHASSIS_LOG_ERROR_INVALID_LOGLEVEL: invalid loglevel
 * @CHASSIS_LOG_ERROR_UNKNOWN_BACKEND: unknown backend
 * @CHASSIS_LOG_ERROR_CHOWN: chown
 * @CHASSIS_LOG_ERROR_OPEN: open
 * @CHASSIS_LOG_ERROR_CLOSE: close
 *
 * errors for the CHASSIS_LOG_ERROR domain
 */
typedef enum {
	CHASSIS_LOG_ERROR_NO_GROUP,
	CHASSIS_LOG_ERROR_NO_BACKENDS,
	CHASSIS_LOG_ERROR_NO_LOGLEVEL,
	CHASSIS_LOG_ERROR_INVALID_LOGLEVEL,
	CHASSIS_LOG_ERROR_UNKNOWN_BACKEND,
	CHASSIS_LOG_ERROR_CHOWN,
	CHASSIS_LOG_ERROR_OPEN,
	CHASSIS_LOG_ERROR_CLOSE
} chassis_log_error_t;

/**
 * CHASSIS_LOG_ERROR:
 *
 * error domain for chassis-log's errors
 */
#define CHASSIS_LOG_ERROR chassis_log_error()

GQuark
chassis_log_error(void);

#endif
