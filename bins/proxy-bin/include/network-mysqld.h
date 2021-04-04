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
 

#ifndef _NETWORK_MYSQLD_H_
#define _NETWORK_MYSQLD_H_

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef HAVE_SYS_TIME_H
/*
 * event.h needs struct timeval and doesn't include sys/time.h itself
 */
#include <sys/time.h>
#endif

#include <sys/types.h>

#ifndef _WIN32
#include <unistd.h>
#else
#include <windows.h>
#include <winsock2.h>
#endif

#include <mysql.h>

#include <glib.h>

#include "network-exports.h"

#include "network-socket.h"
#include "network-conn-pool.h"
#include "chassis-plugin.h"
#include "chassis-mainloop.h"
#include "chassis-timings.h"
#include "lua-scope.h"
#include "network-backend.h"
#include "lua-registry-keys.h"

typedef struct network_mysqld_con network_mysqld_con; /* forward declaration */

/**
 * NETWORK_MYSQLD_CON_TRACK_TIME:
 * @con: (type network_mysqld_con): connection
 * @name: (type char *): name of the data point
 *
 * helper to track the timestamps around the connection
 */

#undef NETWORK_MYSQLD_WANT_CON_TRACK_TIME
#ifdef NETWORK_MYSQLD_WANT_CON_TRACK_TIME
#define NETWORK_MYSQLD_CON_TRACK_TIME(con, name) chassis_timestamps_add(con->timestamps, name, __FILE__, __LINE__)
#else
#define NETWORK_MYSQLD_CON_TRACK_TIME(con, name) 
#endif

/**
 * network_mysqld_plugin_func:
 * @chas: chassis
 * @con: currect connection
 *
 * a connection hook
 */
typedef network_socket_retval_t (*network_mysqld_plugin_func)(chassis *chas, network_mysqld_con *con);

/**
 * NETWORK_MYSQLD_PLUGIN_PROTO:
 * @x: function name
 *
 * The prototype for plugin callback functions.
 * 
 * Some plugins don't use the global "chas" pointer, thus it is marked "unused" for GCC.
 */
#define NETWORK_MYSQLD_PLUGIN_PROTO(x) static network_socket_retval_t x(chassis G_GNUC_UNUSED *chas, network_mysqld_con *con)

/**
 * network_mysqld_hooks:
 * @con_init: Called when a new client connection to MySQL Proxy was created.
 * @con_connect_server: Called when MySQL Proxy needs to establish a connection to a backend server
 * @con_read_handshake: Called when MySQL Proxy has read the handshake packet from the serve
 * @con_send_handshake: Called when MySQL Proxy wants to send the handshake packet to the client
 * @con_read_auth: Called when MySQL Proxy has read the authentication packet from the clien
 * @con_send_auth: Called when MySQL Proxy wants to send the authentication packet to the server
 * @con_read_auth_result: Called when MySQL Proxy has read the authentication result from the backend server, in response to con_send_auth. 
 * @con_send_auth_result: Called when MySQL Proxy wants to send the authentication response packet to the client
 * @con_read_query: Called when MySQL Proxy receives a COM_QUERY packet from a client.
 * @con_read_query_result: Called when MySQL Proxy receives a result set from the server.
 * @con_send_query_result:  Called when MySQL Proxy sends a result set to the client.
 * @con_timer_elapsed: Called when an internal timer has elapsed
 * @con_cleanup: Called when either side of a connection was either closed or some network error occurred.
 * @con_read_local_infile_data: read local infile data
 * @con_send_local_infile_data: send local infile data
 * @con_read_local_infile_result: read local infile result
 * @con_send_local_infile_result: send local infile result
 * @con_read_auth_old_password: read old password
 * @con_send_auth_old_password: send old password
 * @con_timeout: network timeouts
 *
 * The function pointers to plugin callbacks for each customizable state in the MySQL Protocol.
 
 * Any of these callbacks can be %NULL, in which case the default pass-through behavior will be used.
 * 
 * The function prototype is defined by #NETWORK_MYSQLD_PLUGIN_PROTO, which is used in each plugin to define the callback.
 */
typedef struct {
	/*
	 * Called when a new client connection to MySQL Proxy was created.
	 */
	network_mysqld_plugin_func con_init;
	/*
	 * Called when MySQL Proxy needs to establish a connection to a backend server
	 *
	 * Returning a handshake response packet from this callback will cause the con_read_handshake step to be skipped.
	 * The next state then is con_send_handshake.
	 */
	network_mysqld_plugin_func con_connect_server;
	/*
	 * Called when MySQL Proxy has read the handshake packet from the server.
	 */
	network_mysqld_plugin_func con_read_handshake;
	/*
	 * Called when MySQL Proxy wants to send the handshake packet to the client.
	 * 
	 * @note No known plugins actually implement this step right now, but rather return a handshake challenge from con_init instead.
	 */
	network_mysqld_plugin_func con_send_handshake;
	/*
	 * Called when MySQL Proxy has read the authentication packet from the client.
	 */
	network_mysqld_plugin_func con_read_auth;
	/*
	 * Called when MySQL Proxy wants to send the authentication packet to the server.
	 * 
	 * @note No known plugins actually implement this step.
	 */
	network_mysqld_plugin_func con_send_auth;
	/*
	 * Called when MySQL Proxy has read the authentication result from the backend server, in response to con_send_auth.
	 */
	network_mysqld_plugin_func con_read_auth_result;
	/*
	 * Called when MySQL Proxy wants to send the authentication response packet to the client.
	 * 
	 * @note No known plugins implement this callback, but the default implementation deals with the important case that
	 * the authentication response used the pre-4.1 password hash method, but the client didn't.
	 * @see network_mysqld_con::auth_result_state
	 */
	network_mysqld_plugin_func con_send_auth_result;
	/*
	 * Called when MySQL Proxy receives a COM_QUERY packet from a client.
	 */
	network_mysqld_plugin_func con_read_query;
	/*
	 * Called when MySQL Proxy receives a result set from the server.
	 */
	network_mysqld_plugin_func con_read_query_result;
	/*
	 * Called when MySQL Proxy sends a result set to the client.
	 * 
	 * The proxy plugin, for example, uses this state to inject more queries into the connection, possibly in response to a
	 * result set received from a server.
	 * 
	 * This callback should not cause multiple result sets to be sent to the client.
	 * @see network_mysqld_con_lua_injection::sent_resultset
	 */
	network_mysqld_plugin_func con_send_query_result;
	/*
	 * Called when an internal timer has elapsed.
	 * 
	 * This state is meant to give a plugin the opportunity to react to timers.
	 * @note This state is currently unused, as there is no support for setting up timers.
	 * @deprecated Unsupported, there is no way to set timers right now. Might be removed in 1.0.
	 */
	network_mysqld_plugin_func con_timer_elapsed;
	/*
	 * Called when either side of a connection was either closed or some network error occurred.
	 * 
	 * Usually this is called because a client has disconnected. Plugins might want to preserve the server connection in this case
	 * and reuse it later. In this case the connection state will be ::CON_STATE_CLOSE_CLIENT.
	 * 
	 * When an error on the server connection occurred, this callback is usually used to close the client connection as well.
	 * In this case the connection state will be ::CON_STATE_CLOSE_SERVER.
	 * 
	 * @note There are no two separate callback functions for the two possibilities, which probably is a deficiency.
	 */
	network_mysqld_plugin_func con_cleanup;

	network_mysqld_plugin_func con_read_local_infile_data;
	network_mysqld_plugin_func con_send_local_infile_data;
	network_mysqld_plugin_func con_read_local_infile_result;
	network_mysqld_plugin_func con_send_local_infile_result;
	network_mysqld_plugin_func con_read_auth_old_password;
	network_mysqld_plugin_func con_send_auth_old_password;

	network_mysqld_plugin_func con_timeout;
} network_mysqld_hooks;

/**
 * network_mysqld_con_parse:
 * @command: The command indicator from the MySQL Protocol
 * @data: An opaque pointer to a parsed command structure
 * @data_free: A function pointer to the appropriate "free" function of @data
 *
 * A structure containing the parsed packet for a command packet as well as the common parts necessary to find the correct
 * packet parsing function.
 * 
 * The correct parsing function is chose by looking at both the current state as well as the command in this structure.
 * 
 * @todo Currently the plugins are responsible for setting the first two fields of this structure. We have to investigate
 * how we can refactor this into a more generic way.
 */
struct network_mysqld_con_parse {
	enum enum_server_command command;

	gpointer data;
	GDestroyNotify data_free;
};

/**
 * network_mysqld_con_state_t:
 * @CON_STATE_INIT:  A new client connection was established
 * @CON_STATE_CONNECT_SERVER: A connection to a backend is about to be made
 * @CON_STATE_READ_HANDSHAKE: A handshake packet is to be read from a server
 * @CON_STATE_SEND_HANDSHAKE: A handshake packet is to be sent to a client
 * @CON_STATE_READ_AUTH: An authentication packet is to be read from a client
 * @CON_STATE_SEND_AUTH: An authentication packet is to be sent to a server
 * @CON_STATE_READ_AUTH_RESULT: The result of an authentication attempt is to be read from a server
 * @CON_STATE_SEND_AUTH_RESULT:  The result of an authentication attempt is to be sent to a client
 * @CON_STATE_READ_AUTH_OLD_PASSWORD: The authentication method used is for pre-4.1 MySQL servers
 * @CON_STATE_SEND_AUTH_OLD_PASSWORD: The authentication method used is for pre-4.1 MySQL servers
 * @CON_STATE_READ_QUERY: COM_QUERY packets are to be read from a client
 * @CON_STATE_SEND_QUERY: COM_QUERY packets are to be sent to a server
 * @CON_STATE_READ_QUERY_RESULT: Result set packets are to be read from a server
 * @CON_STATE_SEND_QUERY_RESULT: Result set packets are to be sent to a client
 * @CON_STATE_CLOSE_CLIENT: The client connection should be closed
 * @CON_STATE_SEND_ERROR: An unrecoverable error occurred, leads to sending a MySQL ERR packet to the client and closing the client connection
 * @CON_STATE_ERROR: An error occurred (malformed/unexpected packet, unrecoverable network error), internal state
 * @CON_STATE_CLOSE_SERVER: The server connection should be closed
 * @CON_STATE_READ_LOCAL_INFILE_DATA: reading local infile data from client
 * @CON_STATE_SEND_LOCAL_INFILE_DATA: sending local infile data to server
 * @CON_STATE_READ_LOCAL_INFILE_RESULT: read result for local infile data from server
 * @CON_STATE_SEND_LOCAL_INFILE_RESULT: send result for local infile data to client
 *
 * The possible states in the MySQL Protocol.
 * 
 * Not all of the states map directly to plugin callbacks. Those states that have no corresponding plugin callbacks are marked as
 * .
 */
typedef enum { 
	CON_STATE_INIT = 0,
	CON_STATE_CONNECT_SERVER = 1,
	CON_STATE_READ_HANDSHAKE = 2,
	CON_STATE_SEND_HANDSHAKE = 3,
	CON_STATE_READ_AUTH = 4,
	CON_STATE_SEND_AUTH = 5,
	CON_STATE_READ_AUTH_RESULT = 6,
	CON_STATE_SEND_AUTH_RESULT = 7,
	CON_STATE_READ_AUTH_OLD_PASSWORD = 8,
	CON_STATE_SEND_AUTH_OLD_PASSWORD = 9,
	CON_STATE_READ_QUERY = 10,
	CON_STATE_SEND_QUERY = 11,
	CON_STATE_READ_QUERY_RESULT = 12,
	CON_STATE_SEND_QUERY_RESULT = 13,
	
	CON_STATE_CLOSE_CLIENT = 14,
	CON_STATE_SEND_ERROR = 15,
	CON_STATE_ERROR = 16,

	CON_STATE_CLOSE_SERVER = 17,

	/* handling the LOAD DATA LOCAL INFILE protocol extensions */
	CON_STATE_READ_LOCAL_INFILE_DATA = 18,
	CON_STATE_SEND_LOCAL_INFILE_DATA = 19,
	CON_STATE_READ_LOCAL_INFILE_RESULT = 20,
	CON_STATE_SEND_LOCAL_INFILE_RESULT = 21
} network_mysqld_con_state_t;

NETWORK_API const char *network_mysqld_con_state_get_name(network_mysqld_con_state_t state);

/**
 * network_mysqld_con:
 *
 * Encapsulates the state and callback functions for a MySQL protocol-based connection to and from MySQL Proxy.
 * 
 * New connection structures are created by the function responsible for handling the accept on a listen socket, which
 * also is a network_mysqld_con structure, but only has a server set - there is no "client" for connections that we listen on.
 * 
 * The chassis itself does not listen on any sockets, this is left to each plugin. Plugins are free to create any number of
 * connections to listen on, but most of them will only create one and reuse the network_mysqld_con_accept() function to set up an
 * incoming connection.
 * 
 * Each plugin can register callbacks for the various states in the MySQL Protocol, these are set in the member plugins.
 * A plugin is not required to implement any callbacks at all, but only those that it wants to customize. Callbacks that
 * are not set, will cause the MySQL Proxy core to simply forward the received data.
 */
struct network_mysqld_con {
	/*
	 * The current/next state of this connection.
	 * 
	 * When the protocol state machine performs a transition, this variable will contain the next state,
	 * otherwise, while performing the action at state, it will be set to the connection's current state
	 * in the MySQL protocol.
	 * 
	 * Plugins may update it in a callback to cause an arbitrary state transition, however, this may result
	 * reaching an invalid state leading to connection errors.
	 * 
	 * @see network_mysqld_con_handle
	 */
	network_mysqld_con_state_t state;

	/*
	 * The server side of the connection as it pertains to the low-level network implementation.
	 */
	network_socket *server;
	/*
	 * The client side of the connection as it pertains to the low-level network implementation.
	 */
	network_socket *client;

	/*
	 * Function pointers to the plugin's callbacks.
	 * 
	 * Plugins don't need set any of these, but if unset, the plugin will not have the opportunity to
	 * alter the behavior of the corresponding protocol state.
	 * 
	 * @note In theory you could use functions from different plugins to handle the various states, but there is no guarantee that
	 * this will work. Generally the plugins will assume that config is their own chassis_plugin_config (a plugin-private struct)
	 * and violating this constraint may lead to a crash.
	 * @see chassis_plugin_config
	 */
	network_mysqld_hooks plugins;
	
	/*
	 * A pointer to a plugin-private struct describing configuration parameters.
	 * 
	 * @note The actual struct definition used is private to each plugin.
	 */
	chassis_plugin_config *config;

	/*
	 * A pointer back to the global, singleton chassis structure.
	 */
	chassis *srv; /* our srv object */

	/*
	 * A boolean flag indicating that this connection should only be used to accept incoming connections.
	 * 
	 * It does not follow the MySQL protocol by itself and its client network_socket will always be NULL.
	 */
	int is_listen_socket;

	/*
	 * An integer indicating the result received from a server after sending an authentication request.
	 * 
	 * This is used to differentiate between the old, pre-4.1 authentication and the new, 4.1+ one based on the response.
	 */
	guint8 auth_result_state;

	/* track the auth-method-switch state */
	GString *auth_switch_to_method;
	GString *auth_switch_to_data;
	guint32  auth_switch_to_round;
	gboolean auth_next_packet_is_from_server;

	/* Flag indicating if we the plugin doesn't need the resultset itself.
	 * 
	 * If set to TRUE, the plugin needs to see the entire resultset and we will buffer it.
	 * If set to FALSE, the plugin is not interested in the content of the resultset and we'll
	 * try to forward the packets to the client directly, even before the full resultset is parsed.
	 */
	gboolean resultset_is_needed;
	/*
	 * Flag indicating whether we have seen all parts belonging to one resultset.
	 */
	gboolean resultset_is_finished;

	/*
	 * Flag indicating that we have received a COM_QUIT command.
	 * 
	 * This is mainly used to differentiate between the case where the server closed the connection because of some error
	 * or if the client asked it to close its side of the connection.
	 * MySQL Proxy would report spurious errors for the latter case, if we failed to track this command.
	 */
	gboolean com_quit_seen;

	/*
	 * Flag indicating whether we have received all data from load data infile.
	 */
	gboolean local_file_data_is_finished;

	/*
	 * Contains the parsed packet.
	 */
	struct network_mysqld_con_parse parse;

	/*
	 * An opaque pointer to a structure describing extra connection state needed by the plugin.
	 * 
	 * The content and meaning is completely up to each plugin and the chassis will not access this in any way.
	 * 
	 * @note In practice, all current plugins and the chassis assume this to be network_mysqld_con_lua_t.
	 */
	void *plugin_con_state;

	/*
	 * track the timestamps of the processing of the connection
	 *
	 * 
	 */
	chassis_timestamps_t *timestamps;

	/* connection specific timeouts */
	struct timeval connect_timeout;
	struct timeval read_timeout;
	struct timeval write_timeout;
};

#ifndef CHASSIS_DISABLE_DEPRECATED
NETWORK_API network_mysqld_con *network_mysqld_con_init(void) G_GNUC_DEPRECATED;
#endif
NETWORK_API network_mysqld_con *network_mysqld_con_new(void);
NETWORK_API void network_mysqld_con_free(network_mysqld_con *con);

/*
 * should be socket 
 */
NETWORK_API void network_mysqld_con_accept(int event_fd, gshort events, void *user_data); /* event handler for accept() */

NETWORK_API int network_mysqld_con_send_ok(network_socket *con);
NETWORK_API int network_mysqld_con_send_ok_full(network_socket *con, guint64 affected_rows, guint64 insert_id, guint16 server_status, guint16 warnings);
NETWORK_API int network_mysqld_con_send_error(network_socket *con, const gchar *errmsg, gsize errmsg_len);
NETWORK_API int network_mysqld_con_send_error_full(network_socket *con, const char *errmsg, gsize errmsg_len, guint errorcode, const gchar *sqlstate);
NETWORK_API int network_mysqld_con_send_error_pre41(network_socket *con, const gchar *errmsg, gsize errmsg_len);
NETWORK_API int network_mysqld_con_send_error_pre41_full(network_socket *con, const char *errmsg, gsize errmsg_len, guint errorcode);
NETWORK_API int network_mysqld_con_send_resultset(network_socket *con, GPtrArray *fields, GPtrArray *rows);
NETWORK_API void network_mysqld_con_reset_command_response_state(network_mysqld_con *con);

/*
 * should be socket 
 */
NETWORK_API network_socket_retval_t network_mysqld_read(chassis *chas, network_socket *con);
NETWORK_API network_socket_retval_t network_mysqld_write(chassis *chas, network_socket *con);
NETWORK_API network_socket_retval_t network_mysqld_con_get_packet(chassis *chas, network_socket *con);

/*
 * chassis_private:
 * @cons: (element-type network_mysqld_con): connections
 * @sc: lua scope
 * @backend: backends
 *
 * "global" data for network_mysqld_con_handle()
 */
struct chassis_private {
	GPtrArray *cons;

	lua_scope *sc;

	network_backends_t *backends;
};

NETWORK_API int network_mysqld_init(chassis *srv);
NETWORK_API void network_mysqld_add_connection(chassis *srv, network_mysqld_con *con);
NETWORK_API void network_mysqld_con_handle(int event_fd, gshort events, void *user_data);
NETWORK_API int network_mysqld_queue_append(network_socket *sock, network_queue *queue, const char *data, size_t data_len);
NETWORK_API int network_mysqld_queue_append_raw(network_socket *sock, network_queue *queue, GString *data);
NETWORK_API int network_mysqld_queue_reset(network_socket *sock);
NETWORK_API int network_mysqld_queue_sync(network_socket *dst, network_socket *src);

#endif
