/* $%BEGINLICENSE%$
 Copyright (c) 2008, 2010, Oracle and/or its affiliates. All rights reserved.

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
#ifndef __NETWORK_MYSQLD_PACKET__
#define __NETWORK_MYSQLD_PACKET__

#include <glib.h>

#include "network-exports.h"

#include "network-mysqld-proto.h"
#include "network-mysqld.h"

/**
 * network_mysqld_protocol_t:
 * @NETWORK_MYSQLD_PROTOCOL_VERSION_PRE41: pre-4.1 protocol
 * @NETWORK_MYSQLD_PROTOCOL_VERSION_41: 4.1 and later
 *
 * protocol version
 */
typedef enum {
	NETWORK_MYSQLD_PROTOCOL_VERSION_PRE41,
	NETWORK_MYSQLD_PROTOCOL_VERSION_41
} network_mysqld_protocol_t;

/**
 * network_mysqld_com_query_result_t:
 *
 * tracking the state of the response of a COM_QUERY packet
 */
typedef struct {
	/*< private >*/
	enum {
		PARSE_COM_QUERY_INIT,
		PARSE_COM_QUERY_FIELD,
		PARSE_COM_QUERY_RESULT,
		PARSE_COM_QUERY_LOCAL_INFILE_DATA,
		PARSE_COM_QUERY_LOCAL_INFILE_RESULT
	} state;

	guint16 server_status;
	guint16 warning_count;
	guint64 affected_rows;
	guint64 insert_id;

	gboolean was_resultset;
	gboolean binary_encoded;

	guint64 rows;
	guint64 bytes;

	guint8  query_status;
} network_mysqld_com_query_result_t;

NETWORK_API network_mysqld_com_query_result_t *network_mysqld_com_query_result_new(void);
NETWORK_API void network_mysqld_com_query_result_free(network_mysqld_com_query_result_t *udata);
#ifndef CHASSIS_DISABLE_DEPRECATED
NETWORK_API int network_mysqld_com_query_result_track_state(network_packet *packet, network_mysqld_com_query_result_t *udata) G_GNUC_DEPRECATED;
NETWORK_API gboolean network_mysqld_com_query_result_is_load_data(network_mysqld_com_query_result_t *udata) G_GNUC_DEPRECATED;
#endif
NETWORK_API gboolean network_mysqld_com_query_result_is_local_infile(network_mysqld_com_query_result_t *udata);
NETWORK_API int network_mysqld_proto_get_com_query_result(network_packet *packet, network_mysqld_com_query_result_t *query, gboolean use_binary_row_data);

/**
 * network_mysqld_com_stmt_prepare_result_t:
 *
 * tracking the response of a COM_STMT_PREPARE command
 *
 * depending on the kind of statement that was prepare we will receive 0-2 EOF packets
 */
typedef struct {
	/*< private >*/
	gboolean first_packet;
	gint     want_eofs;
} network_mysqld_com_stmt_prepare_result_t;

NETWORK_API network_mysqld_com_stmt_prepare_result_t *network_mysqld_com_stmt_prepare_result_new(void);
NETWORK_API void network_mysqld_com_stmt_prepare_result_free(network_mysqld_com_stmt_prepare_result_t *udata);
NETWORK_API int network_mysqld_proto_get_com_stmt_prepare_result(network_packet *packet, network_mysqld_com_stmt_prepare_result_t *udata);

/**
 * network_mysqld_com_init_db_result_t:
 *
 * tracking the response of a COM_INIT_DB command
 *
 * we have to track the default internally can only accept it
 * if the server side OK'ed it
 */
typedef struct {
	/*< private >*/
	GString *db_name;
} network_mysqld_com_init_db_result_t;

NETWORK_API network_mysqld_com_init_db_result_t *network_mysqld_com_init_db_result_new(void);
NETWORK_API void network_mysqld_com_init_db_result_free(network_mysqld_com_init_db_result_t *com_init_db);
NETWORK_API int network_mysqld_com_init_db_result_track_state(network_packet *packet, network_mysqld_com_init_db_result_t *udata);
NETWORK_API int network_mysqld_proto_get_com_init_db_result(network_packet *packet, 
		network_mysqld_com_init_db_result_t *udata,
		network_mysqld_con *con
		);

NETWORK_API int network_mysqld_proto_get_query_result(network_packet *packet, network_mysqld_con *con);
NETWORK_API int network_mysqld_con_command_states_init(network_mysqld_con *con, network_packet *packet);

NETWORK_API GList *network_mysqld_proto_get_fielddefs(GList *chunk, GPtrArray *fields);

/**
 * network_mysqld_ok_packet_t:
 * @affected_rows: affected rows
 * @insert_id: insert id
 * @server_status: server status
 * @warnings: number of warnings
 * @msg: message
 *
 * OK packet
 */
typedef struct {
	guint64 affected_rows;
	guint64 insert_id;
	guint16 server_status;
	guint16 warnings;

	gchar *msg;
} network_mysqld_ok_packet_t;

NETWORK_API network_mysqld_ok_packet_t *network_mysqld_ok_packet_new(void);
NETWORK_API void network_mysqld_ok_packet_free(network_mysqld_ok_packet_t *ok_packet);

NETWORK_API int network_mysqld_proto_get_ok_packet(network_packet *packet, network_mysqld_ok_packet_t *ok_packet);
NETWORK_API int network_mysqld_proto_append_ok_packet(GString *packet, network_mysqld_ok_packet_t *ok_packet);

/**
 * network_mysqld_err_packet_t:
 * @errmsg: error message
 * @sqlstate: SQL state
 * @errcode: error code
 * @version: protovol version
 *
 * ERR packet
 */
typedef struct {
	GString *errmsg;
	GString *sqlstate;

	guint16 errcode;
	network_mysqld_protocol_t version;
} network_mysqld_err_packet_t;

NETWORK_API network_mysqld_err_packet_t *network_mysqld_err_packet_new(void);
NETWORK_API network_mysqld_err_packet_t *network_mysqld_err_packet_new_pre41(void);
NETWORK_API void network_mysqld_err_packet_free(network_mysqld_err_packet_t *err_packet);

NETWORK_API int network_mysqld_proto_get_err_packet(network_packet *packet, network_mysqld_err_packet_t *err_packet);
NETWORK_API int network_mysqld_proto_append_err_packet(GString *packet, network_mysqld_err_packet_t *err_packet);

/**
 * network_mysqld_eof_packet_t:
 * @server_status: status flags
 * @warnings: warning count
 *
 * EOF packet
 */
typedef struct {
	guint16 server_status;
	guint16 warnings;
} network_mysqld_eof_packet_t;

NETWORK_API network_mysqld_eof_packet_t *network_mysqld_eof_packet_new(void);
NETWORK_API void network_mysqld_eof_packet_free(network_mysqld_eof_packet_t *eof_packet);

NETWORK_API int network_mysqld_proto_get_eof_packet(network_packet *packet, network_mysqld_eof_packet_t *eof_packet);
NETWORK_API int network_mysqld_proto_append_eof_packet(GString *packet, network_mysqld_eof_packet_t *eof_packet);

/**
 * network_mysqld_auth_challenge:
 * @protocol_version: protocol version
 * @server_version_str: server version string
 * @server_version: server version 
 * @thread_id: connection id
 * @auth_plugin_data: auth plugin data
 * @capabilities: capability flags
 * @charset: character set
 * @server_status: status flags
 * @auth_plugin_name: auth plugin name
 *
 * auth challenage
 */
struct network_mysqld_auth_challenge {
	guint8    protocol_version;
	gchar    *server_version_str;
	guint32   server_version;
	guint32   thread_id;
	GString  *auth_plugin_data;
	guint32   capabilities;
	guint8    charset;
	guint16   server_status;
	GString  *auth_plugin_name;
};

NETWORK_API network_mysqld_auth_challenge *network_mysqld_auth_challenge_new(void);
NETWORK_API void network_mysqld_auth_challenge_free(network_mysqld_auth_challenge *shake);
NETWORK_API int network_mysqld_proto_get_auth_challenge(network_packet *packet, network_mysqld_auth_challenge *shake);
NETWORK_API int network_mysqld_proto_append_auth_challenge(GString *packet, network_mysqld_auth_challenge *shake);
NETWORK_API void network_mysqld_auth_challenge_set_challenge(network_mysqld_auth_challenge *shake);
NETWORK_API network_mysqld_auth_challenge *network_mysqld_auth_challenge_copy(network_mysqld_auth_challenge *src);

/**
 * network_mysqld_auth_response:
 * @client_capabilities: client capabilities
 * @server_capabilities: server capabilities
 * @max_packet_size: max packet size
 * @charset: client character set
 * @username: username
 * @auth_plugin_data: auth plugin data
 * @auth_plugin_name: auth plugin name
 * @database: initial database
 *
 * auth response
 *
 */
struct network_mysqld_auth_response {
	guint32  client_capabilities;
	guint32  server_capabilities;
	guint32  max_packet_size;
	guint8   charset;
	GString *username;
	GString *auth_plugin_data;
	GString *database;
	GString *auth_plugin_name;
};

NETWORK_API network_mysqld_auth_response *network_mysqld_auth_response_new(guint server_capabilities);
NETWORK_API void network_mysqld_auth_response_free(network_mysqld_auth_response *auth);
NETWORK_API int network_mysqld_proto_append_auth_response(GString *packet, network_mysqld_auth_response *auth);
NETWORK_API int network_mysqld_proto_get_auth_response(network_packet *packet, network_mysqld_auth_response *auth);
NETWORK_API network_mysqld_auth_response *network_mysqld_auth_response_copy(network_mysqld_auth_response *src);


/**
 * network_mysqld_stmt_prepare_packet_t:
 * @stmt_text: statement text
 *
 * COM_STMT_PREPARE packet
 */
typedef struct {
	GString *stmt_text;
} network_mysqld_stmt_prepare_packet_t;

NETWORK_API network_mysqld_stmt_prepare_packet_t *network_mysqld_stmt_prepare_packet_new();
NETWORK_API void network_mysqld_stmt_prepare_packet_free(network_mysqld_stmt_prepare_packet_t *stmt_prepare_packet);
NETWORK_API int network_mysqld_proto_get_stmt_prepare_packet(network_packet *packet, network_mysqld_stmt_prepare_packet_t *stmt_prepare_packet);
NETWORK_API int network_mysqld_proto_append_stmt_prepare_packet(GString *packet, network_mysqld_stmt_prepare_packet_t *stmt_prepare_packet);

/**
 * network_mysqld_stmt_prepare_ok_packet_t:
 * @stmt_id: statement id
 * @num_columns: number of columns
 * @num_params: number of parameters
 * @warnings: warning count
 *
 * COM_STMT_PREPARE OK response packet
 */
typedef struct {
	guint32 stmt_id;
	guint16 num_columns;
	guint16 num_params;
	guint16 warnings;
} network_mysqld_stmt_prepare_ok_packet_t;

NETWORK_API network_mysqld_stmt_prepare_ok_packet_t *network_mysqld_stmt_prepare_ok_packet_new(void);
NETWORK_API void network_mysqld_stmt_prepare_ok_packet_free(network_mysqld_stmt_prepare_ok_packet_t *stmt_prepare_ok_packet);
NETWORK_API int network_mysqld_proto_get_stmt_prepare_ok_packet(network_packet *packet, network_mysqld_stmt_prepare_ok_packet_t *stmt_prepare_ok_packet);
NETWORK_API int network_mysqld_proto_append_stmt_prepare_ok_packet(GString *packet, network_mysqld_stmt_prepare_ok_packet_t *stmt_prepare_ok_packet);

/**
 * network_mysqld_stmt_execute_packet_t:
 * @stmt_id: statement id
 * @flags: flags
 * @iteration_count: iteration count
 * @new_params_bound: TRUE if new parameters where bound
 * @params: (element-type network_mysqld_type): parameters
 *
 * COM_STMT_EXECUTE
 */
typedef struct {
	guint32 stmt_id;
	guint8  flags;
	guint32 iteration_count;
	guint8 new_params_bound;
	GPtrArray *params;
} network_mysqld_stmt_execute_packet_t;

NETWORK_API network_mysqld_stmt_execute_packet_t *network_mysqld_stmt_execute_packet_new(void);
NETWORK_API void network_mysqld_stmt_execute_packet_free(network_mysqld_stmt_execute_packet_t *stmt_execute_packet);
NETWORK_API int network_mysqld_proto_get_stmt_execute_packet(network_packet *packet, network_mysqld_stmt_execute_packet_t *stmt_execute_packet, guint param_count);
NETWORK_API int network_mysqld_proto_append_stmt_execute_packet(GString *packet, network_mysqld_stmt_execute_packet_t *stmt_execute_packet, guint param_count);
NETWORK_API int network_mysqld_proto_get_stmt_execute_packet_stmt_id(network_packet *packet, guint32 *stmt_id);


/**
 * network_mysqld_resultset_row_t:
 *
 * array of fields
 */
typedef GPtrArray network_mysqld_resultset_row_t;

NETWORK_API network_mysqld_resultset_row_t *network_mysqld_resultset_row_new(void);
NETWORK_API void network_mysqld_resultset_row_free(network_mysqld_resultset_row_t *row);
NETWORK_API int network_mysqld_proto_get_binary_row(network_packet *packet, network_mysqld_proto_fielddefs_t *coldefs, network_mysqld_resultset_row_t *row);
NETWORK_API GList *network_mysqld_proto_get_next_binary_row(GList *chunk, network_mysqld_proto_fielddefs_t *coldefs, network_mysqld_resultset_row_t *row);

/**
 * network_mysqld_stmt_close_packet_t:
 * @stmt_id: statement id
 *
 * COM_STMT_CLOSE packet
 */
typedef struct {
	guint32 stmt_id;
} network_mysqld_stmt_close_packet_t;

NETWORK_API network_mysqld_stmt_close_packet_t *network_mysqld_stmt_close_packet_new(void);
NETWORK_API void network_mysqld_stmt_close_packet_free(network_mysqld_stmt_close_packet_t *stmt_close_packet);
NETWORK_API int network_mysqld_proto_get_stmt_close_packet(network_packet *packet, network_mysqld_stmt_close_packet_t *stmt_close_packet);
NETWORK_API int network_mysqld_proto_append_stmt_close_packet(GString *packet, network_mysqld_stmt_close_packet_t *stmt_close_packet);

#endif
