/* $%BEGINLICENSE%$
 Copyright (c) 2008, 2009, Oracle and/or its affiliates. All rights reserved.

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
 

#ifndef _BACKEND_H_
#define _BACKEND_H_

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "network-conn-pool.h"
#include "chassis-mainloop.h"

#include "network-exports.h"

/**
 * backend_state_t:
 * @BACKEND_STATE_UNKNOWN: unknown
 * @BACKEND_STATE_UP: up
 * @BACKEND_STATE_DOWN: down
 * 
 * state of the backend
 */
typedef enum { 
	BACKEND_STATE_UNKNOWN, 
	BACKEND_STATE_UP, 
	BACKEND_STATE_DOWN
} backend_state_t;

/**
 * backend_type_t:
 * @BACKEND_TYPE_UNKNOWN: unknown
 * @BACKEND_TYPE_RW: read-write
 * @BACKEND_TYPE_RO: read-only
 *
 * backend type
 */
typedef enum { 
	BACKEND_TYPE_UNKNOWN, 
	BACKEND_TYPE_RW, 
	BACKEND_TYPE_RO
} backend_type_t;

/**
 * network_backend_t:
 * @addr: address
 * @state: state of the backend
 * @state_since: state last changed timestamp
 * @type: type of backend
 * @pool: connection pool
 * @connected_clients: number of connected clients
 * @uuid: UUID of the backend
 *
 * a network backend
 */
typedef struct {
	network_address *addr;
   
	backend_state_t state;
	backend_type_t type;

	GTimeVal state_since;

	network_connection_pool *pool;

	guint connected_clients;

	GString *uuid;
} network_backend_t;

#ifndef CHASSIS_DISABLE_DEPRECATED
/**
 * backend_t:
 *
 * backend
 *
 * Deprecated: 0.7.0: use network_backend_t instead
 */
typedef network_backend_t backend_t
#ifndef __GTK_DOC_IGNORE__
	G_GNUC_DEPRECATED
#endif
;

NETWORK_API network_backend_t *backend_init(void) G_GNUC_DEPRECATED;
NETWORK_API void backend_free(network_backend_t *b) G_GNUC_DEPRECATED;
#endif

NETWORK_API network_backend_t *network_backend_new(void);
NETWORK_API void network_backend_free(network_backend_t *b);

/**
 * network_backends_t:
 *
 * collection of backends
 */
typedef struct {
	/*< private >*/
	GPtrArray *backends;
	GMutex    *backends_mutex;
	
	GTimeVal backend_last_check;
} network_backends_t;

NETWORK_API network_backends_t *network_backends_new(void);
NETWORK_API void network_backends_free(network_backends_t *bs);
NETWORK_API int network_backends_add(network_backends_t *bs, /* const */ gchar *address, backend_type_t type);
NETWORK_API int network_backends_check(network_backends_t *bs);
NETWORK_API network_backend_t * network_backends_get(network_backends_t *bs, guint ndx);
NETWORK_API guint network_backends_count(network_backends_t *bs);

#endif /* _BACKEND_H_ */

