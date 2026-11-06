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
#include <cstdint>
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

// Counts every query and the CREATE statements among them
class counting_sqlite : public sqlite_adapter {
  public:
	counting_sqlite() : sqlite_adapter(":memory:") {}
	std::unique_ptr<statement> query(const std::string& sql, const attribute_vector& values = {}) override {
		++calls;
		if (sql.starts_with("CREATE")) {
			++creates;
		}
		return sqlite_adapter::query(sql, values);
	}
	void reset() { calls = creates = 0; }

	int calls{ 0 };
	int creates{ 0 };
};

// Claims every table is missing, to simulate a table created by someone else between check and create
class blind_sqlite : public sqlite_adapter {
  public:
	blind_sqlite() : sqlite_adapter(":memory:") {}
	bool table_exists(std::string_view /*name*/) override { return false; }
};

struct UnitParent : typed_model<UnitParent> {
	static constexpr auto table_name{ "unit_parents" };
	static constexpr auto primary_key{ "id" };
	std::int64_t id{};
	std::string name{};
	static constexpr auto columns() {
		return ar::columns(col<&UnitParent::id>("id"), col<&UnitParent::name>("name").index());
	}
};

struct UnitChild : typed_model<UnitChild> {
	static constexpr auto table_name{ "unit_children" };
	static constexpr auto primary_key{ "id" };
	std::int64_t id{};
	std::int64_t parent_id{};
	static constexpr auto columns() {
		return ar::columns(
			col<&UnitChild::id>("id"),
			col<&UnitChild::parent_id>("parent_id").references<UnitParent>().on_delete(fk_action::cascade));
	}
};

bool sqlite_index_exists(connection& conn, std::string_view name) {
	return conn.query("SELECT 1 FROM sqlite_master WHERE type = 'index' AND name = ?", { attribute{ name } })->fetch();
}

void use_default(const std::shared_ptr<connection>& conn) {
	config::instance().set_connections({ { "default", conn } }, "default");
}

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

TEST(DdlTest, UppercaseNamesAreRejected) {
	sqlite_adapter adapter{ ":memory:" };
	table_schema schema{ simple_schema() };
	schema.columns.push_back(column_schema{ "Name", column_type::string, 20 });
	EXPECT_THROW((void)adapter.create_table_plan(schema), ex::activerecord);
	schema.columns.back().name = "name2";
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

// ==============================================================================
// create_table / drop_table
// ==============================================================================

class CreateTableTest : public ::testing::Test {
  protected:
	void SetUp() override { use_default(conn_); }
	void TearDown() override { config::instance().set_connections({}, ""); }

	std::shared_ptr<counting_sqlite> conn_{ std::make_shared<counting_sqlite>() };
};

TEST_F(CreateTableTest, CreatesAndDropsInOrder) {
	conn_->foreign_keys(true);
	create_tables<UnitParent, UnitChild>();
	EXPECT_TRUE(conn_->table_exists("unit_parents"));
	EXPECT_TRUE(conn_->table_exists("unit_children"));
	EXPECT_TRUE(sqlite_index_exists(*conn_, index_name("unit_parents", "name")));

	drop_tables<UnitParent, UnitChild>(); // children first, so the parent can go
	EXPECT_FALSE(conn_->table_exists("unit_children"));
	EXPECT_FALSE(conn_->table_exists("unit_parents"));
}

TEST_F(CreateTableTest, IfNotExistsAndIfExists) {
	EXPECT_TRUE(create_table<UnitParent>());
	EXPECT_FALSE(create_table<UnitParent>({ .if_not_exists = true }));
	EXPECT_THROW(create_table<UnitParent>(), ex::activerecord); // the server's "already exists"

	drop_table<UnitParent>();
	EXPECT_NO_THROW(drop_table<UnitParent>({ .if_exists = true }));
	EXPECT_THROW(drop_table<UnitParent>(), ex::activerecord);
}

TEST_F(CreateTableTest, InvalidSchemaSendsNoQuery) {
	table_schema bad{ simple_schema() };
	bad.columns[1].name = "a.b";
	conn_->reset();
	EXPECT_THROW(create_table(bad), ex::activerecord);
	EXPECT_EQ(0, conn_->calls);

	// Validation comes first, even with if_not_exists and an existing table of that name
	create_table(simple_schema());
	table_schema invalid_existing{ simple_schema() };
	invalid_existing.columns[1].length = 0;
	conn_->reset();
	EXPECT_THROW(create_table(invalid_existing, { .if_not_exists = true }), ex::activerecord);
	EXPECT_EQ(0, conn_->calls);

	conn_->reset();
	EXPECT_THROW(drop_table("a.b"), ex::activerecord);
	EXPECT_EQ(0, conn_->calls);
}

TEST_F(CreateTableTest, ForeignKeysNeedEnforcement) {
	create_table<UnitParent>();
	conn_->reset();
	EXPECT_THROW(create_table<UnitChild>(), ex::config); // SQLite enforcement is off by default
	EXPECT_EQ(0, conn_->creates);
	EXPECT_FALSE(conn_->table_exists("unit_children"));

	// An existing FK table is skipped by if_not_exists without looking at enforcement
	conn_->foreign_keys(true);
	create_table<UnitChild>();
	conn_->foreign_keys(false);
	conn_->reset();
	EXPECT_FALSE(create_table<UnitChild>({ .if_not_exists = true }));
	EXPECT_EQ(0, conn_->creates);
	EXPECT_EQ(1, conn_->calls); // only the existence check
}

TEST_F(CreateTableTest, IfNotExistsDoesNotRepair) {
	create_table<UnitParent>();
	conn_->query("DROP INDEX " + conn_->escape(index_name("unit_parents", "name")));
	EXPECT_FALSE(create_table<UnitParent>({ .if_not_exists = true }));
	EXPECT_FALSE(sqlite_index_exists(*conn_, index_name("unit_parents", "name")));
}

TEST_F(CreateTableTest, FailureAfterCreateTableKeepsTheTable) {
	// SQLite index names are per database: take the name the new table's index will need
	conn_->query("CREATE TABLE squatter (x INTEGER)");
	conn_->query("CREATE INDEX " + conn_->escape(index_name("unit_parents", "name")) + " ON squatter (x)");

	EXPECT_THROW(create_table<UnitParent>(), ex::activerecord);
	EXPECT_TRUE(conn_->table_exists("unit_parents")); // no automatic cleanup
	drop_table<UnitParent>();
	EXPECT_FALSE(conn_->table_exists("unit_parents"));
}

TEST(CreateTableRaceTest, TableCreatedBetweenCheckAndCreate) {
	auto conn{ std::make_shared<blind_sqlite>() };
	use_default(conn);
	create_table<UnitParent>();
	try {
		create_table<UnitParent>({ .if_not_exists = true });
		ADD_FAILURE() << "expected the server's error";
	} catch (const ex::activerecord& e) {
		EXPECT_NE(std::string::npos, std::string{ e.what() }.find("already exists")) << e.what();
	}
	config::instance().set_connections({}, "");
}

TEST(CreateTableConnectionTest, NamedUnknownAndMissingConnections) {
	auto first{ std::make_shared<sqlite_adapter>(":memory:") };
	auto second{ std::make_shared<sqlite_adapter>(":memory:") };
	config::instance().set_connections({ { "first", first }, { "second", second } }, "first");

	create_table<UnitParent>({}, "second");
	EXPECT_TRUE(second->table_exists("unit_parents"));
	EXPECT_FALSE(first->table_exists("unit_parents"));
	EXPECT_THROW(create_table<UnitParent>({}, "nope"), ex::config);

	config::instance().set_connections({ { "first", first } }, "");
	EXPECT_THROW(create_table<UnitParent>(), ex::config); // no default connection
	EXPECT_THROW(drop_table<UnitParent>(), ex::config);
	config::instance().set_connections({}, "");
}

TEST_F(CreateTableTest, CreateTableSqlOnlyRenders) {
	conn_->reset();
	const std::vector<std::string> plan{ create_table_sql<UnitParent>() };
	ASSERT_EQ(2U, plan.size());
	EXPECT_TRUE(plan[0].starts_with("CREATE TABLE `unit_parents`"));
	EXPECT_EQ(0, conn_->calls);
}
