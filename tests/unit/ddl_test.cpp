// ============================================================================
// ddl_test.cpp
//
// SPDX-FileCopyrightText: 2020-2026 Indridi Bjornsson
// SPDX-License-Identifier: MIT
// See file LICENSE for details
//
// Unit tests for the DDL functions of connection and the SQLite adapter
//

#include <activerecord/adapters/sqlite_adapter.h>
#include <activerecord/config.h>
#include <activerecord/connection_factory.h>
#include <activerecord/schema.h>
#include <gtest/gtest.h>
#include <memory>
#include <string>

using namespace ar;

namespace {

// An adapter that implements only what connection requires: every DDL function must refuse
class plain_connection : public connection {
  public:
	std::unique_ptr<statement> query(const std::string&, const attribute_vector&) override { return nullptr; }
	void limit(std::string&, long, long) const override {}
	std::size_t insert_id() const override { return 0; }
	void begin_transaction() override {}
	void commit() override {}
	void rollback() override {}
};

// table_exists() goes through query(); a failing query must propagate
class failing_sqlite : public sqlite_adapter {
  public:
	failing_sqlite() : sqlite_adapter(":memory:") {}
	std::unique_ptr<statement> query(const std::string&, const attribute_vector&) override {
		throw ex::activerecord("lookup failed");
	}
};

table_schema simple_schema() {
	table_schema schema{ "things", "id", {}, {} };
	schema.columns.push_back(column_schema{ "id" });
	schema.columns.push_back(column_schema{ "name", column_type::string, 20 });
	return schema;
}

} // namespace

TEST(DdlTest, CustomAdapterWithoutOverridesRefusesDdl) {
	plain_connection conn;
	const table_schema schema{ simple_schema() };
	EXPECT_THROW((void)conn.column_type_sql(schema.columns[1]), ex::activerecord);
	EXPECT_THROW((void)conn.primary_key_sql(), ex::activerecord);
	EXPECT_THROW((void)conn.literal_sql(schema.columns[1]), ex::activerecord);
	EXPECT_THROW((void)conn.create_table_plan(schema), ex::activerecord);
	EXPECT_THROW((void)conn.table_exists("things"), ex::activerecord);
	EXPECT_TRUE(conn.enforces_foreign_keys());
}

TEST(DdlTest, ColumnsDifferingOnlyInCaseAreRejected) {
	sqlite_adapter adapter{ ":memory:" };
	table_schema schema{ simple_schema() };
	schema.columns.push_back(column_schema{ "Name", column_type::string, 20 });
	EXPECT_THROW((void)adapter.create_table_plan(schema), ex::activerecord);
	schema.columns.back().name = "nAmE2";
	EXPECT_NO_THROW((void)adapter.create_table_plan(schema));
}

TEST(DdlTest, DelimiterCharactersInNamesAreRejected) {
	sqlite_adapter adapter{ ":memory:" };
	for (const std::string bad : { "`odd`", "a\"b", "[x]", "a]b" }) {
		table_schema schema{ simple_schema() };
		schema.columns[1].name = bad;
		EXPECT_THROW((void)adapter.create_table_plan(schema), ex::activerecord) << bad;
		table_schema table{ simple_schema() };
		table.name = bad;
		EXPECT_THROW((void)adapter.create_table_plan(table), ex::activerecord) << bad;
	}
}

TEST(DdlTest, SqliteForeignKeysAreOffByDefaultAndFollowThePragma) {
	sqlite_adapter adapter{ ":memory:" };
	EXPECT_FALSE(adapter.enforces_foreign_keys());
	adapter.foreign_keys(true);
	EXPECT_TRUE(adapter.enforces_foreign_keys());
	adapter.query("PRAGMA foreign_keys = OFF");
	EXPECT_FALSE(adapter.enforces_foreign_keys()); // the live value, not a saved flag
	adapter.foreign_keys(true);
	EXPECT_TRUE(adapter.enforces_foreign_keys());
	adapter.foreign_keys(false);
	EXPECT_FALSE(adapter.enforces_foreign_keys());
}

TEST(DdlTest, SqliteForeignKeysCannotChangeInsideATransaction) {
	sqlite_adapter adapter{ ":memory:" };
	adapter.begin_transaction();
	EXPECT_THROW(adapter.foreign_keys(true), ex::config);
	adapter.rollback();
	EXPECT_FALSE(adapter.enforces_foreign_keys());
	adapter.query("CREATE TABLE usable (x INTEGER)"); // the connection still works
	EXPECT_TRUE(adapter.table_exists("usable"));
}

TEST(DdlTest, TableExistsPropagatesLookupErrors) {
	failing_sqlite adapter;
	EXPECT_THROW((void)adapter.table_exists("things"), ex::activerecord);
}

TEST(DdlTest, ExistingSqliteConstructorFormsStillCompile) {
	EXPECT_NO_THROW(sqlite_adapter(":memory:"));
	EXPECT_NO_THROW(sqlite_adapter(":memory:", {}));
	EXPECT_NO_THROW(sqlite_adapter(":memory:", {}, {}));
	EXPECT_NO_THROW(sqlite_adapter(":memory:", "user", "password"));
}

TEST(DdlTest, ForeignKeysUrlParameter) {
	EXPECT_FALSE(connection::parse_connection_url("sqlite://:memory:").foreign_keys);
	for (const char* value : { "on", "true", "1", "ON" }) {
		EXPECT_TRUE(
			connection::parse_connection_url(std::string{ "sqlite://:memory:?foreign_keys=" } + value).foreign_keys)
			<< value;
	}
	EXPECT_FALSE(connection::parse_connection_url("sqlite://:memory:?foreign_keys=off").foreign_keys);

	EXPECT_FALSE(make_connection("sqlite://:memory:")->enforces_foreign_keys());
	EXPECT_TRUE(make_connection("sqlite://:memory:?foreign_keys=on")->enforces_foreign_keys());
}

TEST(DdlTest, MysqlDecimalStorage) {
	// Examples from the MySQL 8 manual, "Precision Math": DECIMAL(18,9) takes 8 bytes, DECIMAL(20,6) 10 bytes
	EXPECT_EQ(8U, detail::mysql_decimal_bytes(18, 9));
	EXPECT_EQ(10U, detail::mysql_decimal_bytes(20, 6));
	EXPECT_EQ(3U, detail::mysql_decimal_bytes(5, 0));
	EXPECT_EQ(1U, detail::mysql_decimal_bytes(1, 0));
}
