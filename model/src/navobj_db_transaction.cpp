/**************************************************************************
 *   Copyright (C) 2026 by OpenCPN contributors                           *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *************************************************************************/

#include "model/navobj_db_transaction.h"

#include <vector>

#include <sqlite3.h>

namespace navobj_db {
namespace {

bool Exec(sqlite3* db, const char* sql) {
  return sqlite3_exec(db, sql, nullptr, nullptr, nullptr) == SQLITE_OK;
}

bool BindGuid(sqlite3_stmt* statement, int index, const std::string& guid) {
  return sqlite3_bind_text(statement, index, guid.c_str(), -1,
                           SQLITE_TRANSIENT) == SQLITE_OK;
}

}  // namespace

bool DeleteRouteAndOrphanedPoints(sqlite3* db, const std::string& route_guid) {
  if (!db || route_guid.empty()) return false;
  if (!Exec(db, "BEGIN IMMEDIATE TRANSACTION")) return false;

  auto rollback = [db]() {
    sqlite3_exec(db, "ROLLBACK", nullptr, nullptr, nullptr);
  };
  std::vector<std::string> point_guids;
  sqlite3_stmt* statement = nullptr;

  constexpr const char* select_points =
      "SELECT DISTINCT point_guid FROM routepoints_link WHERE route_guid = ?";
  if (sqlite3_prepare_v2(db, select_points, -1, &statement, nullptr) !=
          SQLITE_OK ||
      !BindGuid(statement, 1, route_guid)) {
    sqlite3_finalize(statement);
    rollback();
    return false;
  }
  int result = SQLITE_OK;
  while ((result = sqlite3_step(statement)) == SQLITE_ROW) {
    const auto* text = sqlite3_column_text(statement, 0);
    if (text) point_guids.emplace_back(reinterpret_cast<const char*>(text));
  }
  sqlite3_finalize(statement);
  if (result != SQLITE_DONE) {
    rollback();
    return false;
  }

  constexpr const char* delete_route = "DELETE FROM routes WHERE guid = ?";
  if (sqlite3_prepare_v2(db, delete_route, -1, &statement, nullptr) !=
          SQLITE_OK ||
      !BindGuid(statement, 1, route_guid) ||
      sqlite3_step(statement) != SQLITE_DONE) {
    sqlite3_finalize(statement);
    rollback();
    return false;
  }
  sqlite3_finalize(statement);

  // Do not rely on foreign_keys being enabled: legacy databases and test
  // fixtures may not have an ON DELETE CASCADE constraint in effect.
  constexpr const char* delete_links =
      "DELETE FROM routepoints_link WHERE route_guid = ?";
  if (sqlite3_prepare_v2(db, delete_links, -1, &statement, nullptr) !=
          SQLITE_OK ||
      !BindGuid(statement, 1, route_guid) ||
      sqlite3_step(statement) != SQLITE_DONE) {
    sqlite3_finalize(statement);
    rollback();
    return false;
  }
  sqlite3_finalize(statement);

  constexpr const char* delete_orphan =
      "DELETE FROM routepoints "
      "WHERE guid = ? "
      "AND COALESCE(isolated, 0) = 0 "
      "AND COALESCE(shared, 0) = 0 "
      "AND NOT EXISTS ("
      "  SELECT 1 FROM routepoints_link WHERE point_guid = ?"
      ")";
  for (const auto& point_guid : point_guids) {
    if (sqlite3_prepare_v2(db, delete_orphan, -1, &statement, nullptr) !=
            SQLITE_OK ||
        !BindGuid(statement, 1, point_guid) ||
        !BindGuid(statement, 2, point_guid) ||
        sqlite3_step(statement) != SQLITE_DONE) {
      sqlite3_finalize(statement);
      rollback();
      return false;
    }
    sqlite3_finalize(statement);
  }

  if (!Exec(db, "COMMIT")) {
    rollback();
    return false;
  }
  return true;
}

}  // namespace navobj_db
