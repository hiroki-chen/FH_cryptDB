#ifndef __CHASSIS_LOG_BACKEND_H__
#define __CHASSIS_LOG_BACKEND_H__

#include <glib.h>
#include <time.h> /* for time_t */

#ifndef _WIN32
#include <sys/types.h> 
#include <unistd.h>    /* should bring in uid_t and gid_t ... but Linux wants <sys/types.h> too */
#else
#include <windows.h>   /* HANDLE */
#endif

#include "chassis-exports.h"

/**
 * chassis_log_backend_resolution_t:
 * @CHASSIS_LOG_BACKEND_RESOLUTION_SEC: seconds
 * @CHASSIS_LOG_BACKEND_RESOLUTION_MS: milli seconds
 *
 * log timestamp resolution
 */
typedef enum {
	CHASSIS_LOG_BACKEND_RESOLUTION_SEC,
	CHASSIS_LOG_BACKEND_RESOLUTION_MS
} chassis_log_backend_resolution_t;

/**
 * CHASSIS_LOG_BACKEND_RESOLUTION_DEFAULT:
 *
 * default timestamp resolution
 */
#define CHASSIS_LOG_BACKEND_RESOLUTION_DEFAULT	CHASSIS_LOG_BACKEND_RESOLUTION_SEC

/**
 * CHASSIS_LOG_LEVEL_BROADCAST:
 *
 * log-level for g_log() to log on all backends and all levels
 */
#define CHASSIS_LOG_LEVEL_BROADCAST (1 << G_LOG_LEVEL_USER_SHIFT)

/* forward decl, so we can use it in the function ptr */
typedef struct chassis_log_backend chassis_log_backend_t;

/**
 * chassis_log_backend_write_func_t:
 * @backend: a #chassis_log_backend_t
 * @level: log level of the @message
 * @message: message to log
 * @len: length of @message
 *
 * write @message to @backend
 */
typedef void (*chassis_log_backend_write_func_t)(chassis_log_backend_t *backend, GLogLevelFlags level, const gchar *message, gsize len);
/**
 * chassis_log_backend_open_func_t:
 * @backend: a #chassis_log_backend_t
 * @gerr: a #GError if error-reporting is wanted, %NULL otherwise
 *
 * open a the backend storage
 *
 * Returns: %TRUE on success, %FALSE on error
 */
typedef gboolean (*chassis_log_backend_open_func_t)(chassis_log_backend_t *backend, GError **gerr);
/**
 * chassis_log_backend_close_func_t:
 * @backend: a #chassis_log_backend_t
 * @gerr: a #GError if error-reporting is wanted, %NULL otherwise
 *
 * close the backend storage
 *
 * Returns: %TRUE on success, %FALSE on error
 */
typedef gboolean (*chassis_log_backend_close_func_t)(chassis_log_backend_t *backend, GError **gerr);
#ifndef _WIN32
/**
 * chassis_log_backend_chown_func_t:
 * @backend: a #chassis_log_backend_t:
 * @uid: UID
 * @gid: GID
 * @gerr: a #GError if error-reporting is wanted, %NULL otherwise
 *
 * change the ownership of the backend
 *
 * Returns: %TRUE on success, %FALSE on error
 */
typedef gboolean (*chassis_log_backend_chown_func_t)(chassis_log_backend_t *backend, uid_t uid, gid_t gid, GError **gerr);
#endif

/**
 * chassis_log_backend_t:
 *
 * A logger backend encapsulates the ultimate backend of a log message and its writing.
 * 
 * Currently it supports file-based logs or anything that doesn't need extra information, like syslog.
 */
struct chassis_log_backend {
	/*< private >*/
	chassis_log_backend_write_func_t log_func;	/**< function that actually writes the message */
	chassis_log_backend_open_func_t open_func;	/**< function that opens the backend */
	chassis_log_backend_close_func_t close_func;	/**< function that closes the backend */
#ifndef _WIN32
	chassis_log_backend_chown_func_t chown_func;	/**< function that chown()s the backend */
#endif
	gboolean supports_reopen;
	gboolean needs_timestamp;
	gboolean needs_compress;
	chassis_log_backend_resolution_t log_ts_resolution;	/*<< timestamp resolution (sec, ms) */

	gchar *name;

	/* file backend specific */
	gchar *file_path;				/**< absolute path to the log file */
	gint fd;					/**< file descriptor for this log file */

#ifdef _WIN32
	/* eventlog specific */
	HANDLE event_source_handle;
#endif

	GMutex *fd_lock;				/**< lock to serialize log message writing */

	GString *log_str;				/**< a reusable string for the log message to write */

	GString *last_msg;				/**< a copy of the last message we have written, used to coalesce messages */
	time_t last_msg_ts;				/**< the timestamp of when we have last written a message */
	guint last_msg_count;				/**< a repeat count to track how many messages we have coalesced */
	GHashTable *last_loggers;			/**< a list of the loggers we coalesced messages for, in order of appearance */
};

CHASSIS_API chassis_log_backend_t *chassis_log_backend_new(void);
CHASSIS_API void chassis_log_backend_free(chassis_log_backend_t *backend);
CHASSIS_API gboolean chassis_log_backend_reopen(chassis_log_backend_t *backend, GError **gerr);
CHASSIS_API void chassis_log_backend_log(chassis_log_backend_t *backend, gchar *logger_name, GLogLevelFlags level, const gchar *message);

CHASSIS_API void chassis_log_backend_write(chassis_log_backend_t *backend, GLogLevelFlags level, gchar *message, gsize len);
CHASSIS_API void chassis_log_backend_lock(chassis_log_backend_t *backend);
CHASSIS_API void chassis_log_backend_unlock(chassis_log_backend_t *backend);
CHASSIS_API gboolean chassis_log_backend_open(chassis_log_backend_t *backend, GError **error);
CHASSIS_API gboolean chassis_log_backend_close(chassis_log_backend_t *backend, GError **error);

#ifndef _WIN32
CHASSIS_API gboolean chassis_log_backend_chown(chassis_log_backend_t *backend,
		uid_t uid, gid_t gid,
		GError **error);
#endif

CHASSIS_API chassis_log_backend_t *chassis_log_backend_file_new(const gchar *filename);
CHASSIS_API chassis_log_backend_t *chassis_log_backend_stderr_new(void);
CHASSIS_API chassis_log_backend_t *chassis_log_backend_syslog_new(void);
CHASSIS_API chassis_log_backend_t *chassis_log_backend_eventlog_new(void);

CHASSIS_API int chassis_log_backend_resolution_set(chassis_log_backend_t *backend, chassis_log_backend_resolution_t res);
CHASSIS_API chassis_log_backend_resolution_t chassis_log_backend_resolution_get(chassis_log_backend_t *backend);

#endif
