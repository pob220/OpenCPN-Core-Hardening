/**************************************************************************
 *   Copyright (C) 2026 by OpenCPN contributors                           *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *************************************************************************/

#include <sqlite3.h>

#include <string>

#include <gtest/gtest.h>

#include "model/navobj_db_transaction.h"

namespace {

class NavobjRouteDeleteTest : public ::testing::Test {
protected:
  void SetUp() override {
    ASSERT_EQ(SQLITE_OK, sqlite3_open(":memory:", &db));
    // This deliberately resembles an early navobj.db schema: there are no
    // foreign keys or cascades on routepoints_link.
    Execute(R"(
      CREATE TABLE routes (guid TEXT PRIMARY KEY NOT NULL);
      CREATE TABLE routepoints (
        guid TEXT PRIMARY KEY NOT NULL,
        shared INTEGER,
        isolated INTEGER
      );
      CREATE TABLE routepoints_link (
        route_guid TEXT,
        point_guid TEXT,
        point_order INTEGER,
        PRIMARY KEY (route_guid, point_order)
      );
    )");
  }

  void TearDown() override { sqlite3_close(db); }

  void Execute(const char* sql) {
    char* error = nullptr;
    const int result = sqlite3_exec(db, sql, nullptr, nullptr, &error);
    const std::string message = error ? error : "";
    sqlite3_free(error);
    ASSERT_EQ(SQLITE_OK, result) << message;
  }

  int Count(const char* sql, const std::string& guid) {
    sqlite3_stmt* statement = nullptr;
    EXPECT_EQ(SQLITE_OK, sqlite3_prepare_v2(db, sql, -1, &statement, nullptr));
    EXPECT_EQ(SQLITE_OK, sqlite3_bind_text(statement, 1, guid.c_str(), -1,
                                           SQLITE_TRANSIENT));
    EXPECT_EQ(SQLITE_ROW, sqlite3_step(statement));
    const int count = sqlite3_column_int(statement, 0);
    sqlite3_finalize(statement);
    return count;
  }

  int RouteCount(const std::string& guid) {
    return Count("SELECT COUNT(*) FROM routes WHERE guid = ?", guid);
  }

  int PointCount(const std::string& guid) {
    return Count("SELECT COUNT(*) FROM routepoints WHERE guid = ?", guid);
  }

  int LinkCount(const std::string& guid) {
    return Count("SELECT COUNT(*) FROM routepoints_link WHERE route_guid = ?",
                 guid);
  }

  sqlite3* db = nullptr;
};

TEST_F(NavobjRouteDeleteTest, PreservesReferencedAndKeepPolicyPoints) {
  Execute(R"(
    INSERT INTO routes VALUES ('route-a'), ('route-b');
    INSERT INTO routepoints VALUES
      ('route-a-only', 0, 0),
      ('shared-between-routes', 0, 0),
      ('keep-shared-policy', 1, 0),
      ('keep-isolated-policy', 0, 1),
      ('legacy-null-policy', NULL, NULL),
      ('unrelated-orphan', 0, 0);
    INSERT INTO routepoints_link VALUES
      ('route-a', 'route-a-only', 1),
      ('route-a', 'shared-between-routes', 2),
      ('route-a', 'keep-shared-policy', 3),
      ('route-a', 'keep-isolated-policy', 4),
      ('route-a', 'legacy-null-policy', 5),
      ('route-b', 'shared-between-routes', 1);
  )");

  ASSERT_TRUE(navobj_db::DeleteRouteAndOrphanedPoints(db, "route-a"));
  EXPECT_EQ(0, RouteCount("route-a"));
  EXPECT_EQ(1, RouteCount("route-b"));
  EXPECT_EQ(0, LinkCount("route-a"));
  EXPECT_EQ(1, LinkCount("route-b"));
  EXPECT_EQ(0, PointCount("route-a-only"));
  EXPECT_EQ(1, PointCount("shared-between-routes"));
  EXPECT_EQ(1, PointCount("keep-shared-policy"));
  EXPECT_EQ(1, PointCount("keep-isolated-policy"));
  EXPECT_EQ(0, PointCount("legacy-null-policy"));
  EXPECT_EQ(1, PointCount("unrelated-orphan"));

  ASSERT_TRUE(navobj_db::DeleteRouteAndOrphanedPoints(db, "route-b"));
  EXPECT_EQ(0, PointCount("shared-between-routes"));
}

TEST_F(NavobjRouteDeleteTest, RollsBackEveryChangeOnFailure) {
  Execute(R"(
    INSERT INTO routes VALUES ('route-a');
    INSERT INTO routepoints VALUES ('route-a-only', 0, 0);
    INSERT INTO routepoints_link VALUES ('route-a', 'route-a-only', 1);
    CREATE TRIGGER reject_route_delete
      BEFORE DELETE ON routes
      BEGIN
        SELECT RAISE(ABORT, 'injected route delete failure');
      END;
  )");

  EXPECT_FALSE(navobj_db::DeleteRouteAndOrphanedPoints(db, "route-a"));
  EXPECT_EQ(1, RouteCount("route-a"));
  EXPECT_EQ(1, LinkCount("route-a"));
  EXPECT_EQ(1, PointCount("route-a-only"));
}

}  // namespace
