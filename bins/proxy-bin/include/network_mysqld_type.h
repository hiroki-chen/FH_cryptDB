#ifndef __NETWORK_MYSQLD_TYPE_H__
#define __NETWORK_MYSQLD_TYPE_H__

#ifdef _WIN32
/* mysql.h needs SOCKET defined */
#include <winsock2.h>
#endif
#include <mysql.h>
#include <glib.h>

#include "network-mysqld-proto.h"

#include "network-exports.h"

/**
 * network_mysqld_type_date_t:
 * @year: year
 * @month: month
 * @day: day
 * @hour: hour
 * @min: minute
 * @sec: seconds
 * @nsec: nano seconds
 *
 * struct for the MYSQL_TYPE_DATE and friends
 */
typedef struct {
	guint16 year;
	guint8  month;
	guint8  day;
	
	guint8  hour;
	guint8  min;
	guint8  sec;

	guint32 nsec; /* the nano-second part */
} network_mysqld_type_date_t;

/**
 * NETWORK_MYSQLD_TYPE_DATE_MIN_BUF_LEN:
 *
 * min length of a buffer for a DATE
 */
#define NETWORK_MYSQLD_TYPE_DATE_MIN_BUF_LEN (sizeof("2010-10-27"))
/**
 * NETWORK_MYSQLD_TYPE_DATETIME_MIN_BUF_LEN:
 *
 * min length of a buffer for a DATETIME
 */
#define NETWORK_MYSQLD_TYPE_DATETIME_MIN_BUF_LEN (sizeof("2010-10-27 19:27:30.000000001"))
/**
 * NETWORK_MYSQLD_TYPE_TIMESTAMP_MIN_BUF_LEN:
 *
 * min length of a buffer for a TIMESTAMP
 */
#define NETWORK_MYSQLD_TYPE_TIMESTAMP_MIN_BUF_LEN NETWORK_MYSQLD_TYPE_DATETIME_MIN_BUF_LEN

/**
 * network_mysqld_type_time_t:
 * @sign: %TRUE if negative
 * @days: days
 * @hour: hour
 * @min: minute
 * @sec: seconds
 * @nsec: nano seconds
 *
 * struct for the MYSQL_TYPE_TIME 
 */
typedef struct {
	guint8  sign;
	guint32 days;
	
	guint8  hour;
	guint8  min;
	guint8  sec;

	guint32 nsec; /* the nano-second part */
} network_mysqld_type_time_t;

/**
 * NETWORK_MYSQLD_TYPE_TIME_MIN_BUF_LEN:
 *
 * min length of a buffer for a TIME
 */
#define NETWORK_MYSQLD_TYPE_TIME_MIN_BUF_LEN (sizeof("-2147483647 19:27:30.000000001"))

typedef struct _network_mysqld_type_t network_mysqld_type_t;

/**
 * network_mysqld_type_t:
 *
 * base class of all other network_mysqld_type types
 */
struct _network_mysqld_type_t {
	/*< private >*/
	enum enum_field_types type;

	gpointer data;
	void (*free_data)(network_mysqld_type_t *type);
	int (*get_gstring)(network_mysqld_type_t *type, GString *s);
	int (*get_string_const)(network_mysqld_type_t *type, const char **s, gsize *s_len);
	int (*get_string)(network_mysqld_type_t *type, char **s, gsize *len);
	int (*set_string)(network_mysqld_type_t *type, const char *s, gsize s_len);
	int (*get_int)(network_mysqld_type_t *type, guint64 *i, gboolean *is_unsigned);
	int (*set_int)(network_mysqld_type_t *type, guint64 i, gboolean is_unsigned);
	int (*get_double)(network_mysqld_type_t *type, double *d);
	int (*set_double)(network_mysqld_type_t *type, double d);
	int (*get_date)(network_mysqld_type_t *type, network_mysqld_type_date_t *date);
	int (*set_date)(network_mysqld_type_t *type, network_mysqld_type_date_t *date);
	int (*get_time)(network_mysqld_type_t *type, network_mysqld_type_time_t *t);
	int (*set_time)(network_mysqld_type_t *type, network_mysqld_type_time_t *t);

	gboolean is_null;
	gboolean is_unsigned;
}; 


NETWORK_API network_mysqld_type_t *network_mysqld_type_new(enum enum_field_types field_type);
NETWORK_API void network_mysqld_type_free(network_mysqld_type_t *type);

NETWORK_API int network_mysqld_type_get_gstring(network_mysqld_type_t *type, GString *s);
NETWORK_API int network_mysqld_type_get_gstring(network_mysqld_type_t *type, GString *s);
NETWORK_API int network_mysqld_type_get_string_const(network_mysqld_type_t *type, const char **s, gsize *s_len);
NETWORK_API int network_mysqld_type_get_string(network_mysqld_type_t *type, char **s, gsize *s_len);
NETWORK_API int network_mysqld_type_set_string(network_mysqld_type_t *type, const char *s, gsize s_len);
NETWORK_API int network_mysqld_type_get_int(network_mysqld_type_t *type, guint64 *i, gboolean *is_unsigned);
NETWORK_API int network_mysqld_type_set_int(network_mysqld_type_t *type, guint64 i, gboolean is_unsigned);
NETWORK_API int network_mysqld_type_get_double(network_mysqld_type_t *type, double *d);
NETWORK_API int network_mysqld_type_set_double(network_mysqld_type_t *type, double d);
NETWORK_API int network_mysqld_type_get_date(network_mysqld_type_t *type, network_mysqld_type_date_t *date);
NETWORK_API int network_mysqld_type_set_date(network_mysqld_type_t *type, network_mysqld_type_date_t *date);
NETWORK_API int network_mysqld_type_get_time(network_mysqld_type_t *type, network_mysqld_type_time_t *t);
NETWORK_API int network_mysqld_type_set_time(network_mysqld_type_t *type, network_mysqld_type_time_t *t);

#endif
