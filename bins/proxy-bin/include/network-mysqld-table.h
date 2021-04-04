/* $%BEGINLICENSE%$
 Copyright (C) 2007-2008 MySQL AB, 2008 Sun Microsystems, Inc

 This program is free software; you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation; version 2 of the License.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with this program; if not, write to the Free Software
 Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

 $%ENDLICENSE%$ */
 

#ifndef _NETWORK_MYSQLD_TABLE_H_
#define _NETWORK_MYSQLD_TABLE_H_

#ifdef _WIN32
/* mysql.h needs SOCKET defined */
#include <winsock2.h>
#endif

#include <glib.h>
#include <mysql.h> /* enum field_types */

#include "network-exports.h"

/**
 * network_mysqld_column:
 *
 * column definition
 */
typedef MYSQL_FIELD network_mysqld_column;

NETWORK_API network_mysqld_column *network_mysqld_column_new();
NETWORK_API void network_mysqld_column_free(network_mysqld_column *col);
NETWORK_API const char *network_mysqld_column_get_typestring(network_mysqld_column *column);

/**
 * network_mysqld_columns:
 *
 * array of #network_mysqld_column
 */
typedef GPtrArray network_mysqld_columns;

NETWORK_API network_mysqld_columns *network_mysqld_columns_new();
NETWORK_API void network_mysqld_columns_free(network_mysqld_columns *cols);

/**
 * network_mysqld_table:
 * @table_id: table id for RBR
 * @db_name: schema name
 * @table_name: table name
 * @columns: columns
 *
 * a table in the context of RBR
 */
typedef struct {
	guint64 table_id;

	GString *db_name;
	GString *table_name;

	network_mysqld_columns *columns;
} network_mysqld_table;

NETWORK_API network_mysqld_table *network_mysqld_table_new();
NETWORK_API void network_mysqld_table_free(network_mysqld_table *tbl);

#endif
