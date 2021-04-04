/* $%BEGINLICENSE%$
 Copyright (c) 2008, Oracle and/or its affiliates. All rights reserved.

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


#ifndef _CHASSIS_STATS_H_
#define _CHASSIS_STATS_H_

#include <glib.h>
#include "chassis-exports.h"

/**
 * chassis_stats_t:
 *
 * global statistics
 */
typedef struct chassis_stats chassis_stats_t;

CHASSIS_API chassis_stats_t * chassis_stats_new(void);
CHASSIS_API void chassis_stats_free(chassis_stats_t *stats);

CHASSIS_API GHashTable* chassis_stats_get(chassis_stats_t *stats);
#endif
