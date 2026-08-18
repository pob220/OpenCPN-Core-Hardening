/**************************************************************************
 *   Copyright (C) 2026 by OpenCPN contributors                           *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *************************************************************************/

#ifndef MODEL_NAVOBJ_DB_TRANSACTION_H
#define MODEL_NAVOBJ_DB_TRANSACTION_H

#include <string>

struct sqlite3;

namespace navobj_db {

/**
 * Delete a route and clean up only the routepoints which it orphaned.
 *
 * A routepoint is retained when another route references it, when it is an
 * isolated mark, or when its persisted shared flag says it should survive
 * route deletion. All database changes are committed atomically.
 */
bool DeleteRouteAndOrphanedPoints(sqlite3* db, const std::string& route_guid);

}  // namespace navobj_db

#endif  // MODEL_NAVOBJ_DB_TRANSACTION_H
