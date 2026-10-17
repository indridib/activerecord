// ============================================================================
// sqlite_adapter_test.cpp
//
// SPDX-FileCopyrightText: 2020-2026 Indridi Bjornsson
// SPDX-License-Identifier: MIT
// See file LICENSE for details
//
// Integration tests for SQLite adapter
//

#include <activerecord/activerecord.h>
#include <activerecord/adapters/sqlite_adapter.h>
#include <gtest/gtest.h>

using namespace ar;

// ==============================================================================
// Test Fixture
// ==============================================================================

class SQLiteAdapterTest : public ::testing::Test {
  protected:
	void SetUp() override {
		// Create in-memory database for testing
		adapter = std::make_shared<sqlite_adapter>(":memory:");

		// Create test table
		adapter->query("CREATE TABLE users ("
					   "id INTEGER PRIMARY KEY AUTOINCREMENT, "
					   "name TEXT NOT NULL, "
					   "age INTEGER, "
					   "email TEXT, "
					   "active BOOLEAN, "
					   "score REAL"
					   ")",
					   {});
	}

	void TearDown() override { adapter.reset(); }

	std::shared_ptr<sqlite_adapter> adapter;
};

// ==============================================================================
// Connection Tests
// ==============================================================================

TEST_F(SQLiteAdapterTest, ConnectionSucceeds) {
	EXPECT_NE(nullptr, adapter);
}

TEST(SQLiteAdapterConnectionTest, InvalidPathThrows) {
	// Try to open a database in a non-existent directory
	EXPECT_THROW(sqlite_adapter("/nonexistent/path/database.db"), ex::activerecord);
}

// ==============================================================================
// INSERT Tests
// ==============================================================================

TEST_F(SQLiteAdapterTest, InsertSingleRow) {
	attribute_vector values = { "John Doe", 30, "john@example.com", true, 95.5 };

	auto stmt = adapter->query("INSERT INTO users (name, age, email, active, score) VALUES (?, ?, ?, ?, ?)", values);

	EXPECT_NE(nullptr, stmt);
	EXPECT_EQ(1u, stmt->affected_rows());
}

TEST_F(SQLiteAdapterTest, InsertMultipleRows) {
	for (int i = 0; i < 5; i++) {
		attribute_vector values = { "User " + std::to_string(i), 20 + i, "user" + std::to_string(i) + "@example.com" };

		adapter->query("INSERT INTO users (name, age, email) VALUES (?, ?, ?)", values);
	}

	// Verify count
	auto stmt = adapter->query("SELECT COUNT(*) as count FROM users", {});
	EXPECT_TRUE(stmt->fetch());

	attribute_map attrs;
	stmt->get_attributes(attrs);
	EXPECT_EQ(5, attrs["count"].as_integer());
}

// ==============================================================================
// SELECT Tests
// ==============================================================================

TEST_F(SQLiteAdapterTest, SelectAllRows) {
	// Insert test data
	adapter->query("INSERT INTO users (name, age) VALUES (?, ?)", { "Alice", 25 });
	adapter->query("INSERT INTO users (name, age) VALUES (?, ?)", { "Bob", 30 });

	// Select all
	auto stmt = adapter->query("SELECT * FROM users ORDER BY id", {});

	// First row
	EXPECT_TRUE(stmt->fetch());
	attribute_map attrs1;
	stmt->get_attributes(attrs1);
	EXPECT_EQ("Alice", attrs1["name"].as_string());
	EXPECT_EQ(25, attrs1["age"].as_integer());

	// Second row
	EXPECT_TRUE(stmt->fetch());
	attribute_map attrs2;
	stmt->get_attributes(attrs2);
	EXPECT_EQ("Bob", attrs2["name"].as_string());
	EXPECT_EQ(30, attrs2["age"].as_integer());

	// No more rows
	EXPECT_FALSE(stmt->fetch());
}

TEST_F(SQLiteAdapterTest, SelectWithWhere) {
	// Insert test data
	adapter->query("INSERT INTO users (name, age) VALUES (?, ?)", { "Alice", 25 });
	adapter->query("INSERT INTO users (name, age) VALUES (?, ?)", { "Bob", 30 });
	adapter->query("INSERT INTO users (name, age) VALUES (?, ?)", { "Charlie", 35 });

	// Select with WHERE clause
	auto stmt = adapter->query("SELECT * FROM users WHERE age > ?", { 26 });

	int count = 0;
	while (stmt->fetch()) {
		count++;
	}

	EXPECT_EQ(2, count); // Bob and Charlie
}

// ==============================================================================
// Parameter Binding Tests
// ==============================================================================

TEST_F(SQLiteAdapterTest, BindIntegerParameter) {
	adapter->query("INSERT INTO users (name, age) VALUES (?, ?)", { "Test", 42 });

	auto stmt = adapter->query("SELECT age FROM users WHERE name = ?", { "Test" });
	EXPECT_TRUE(stmt->fetch());

	attribute_map attrs;
	stmt->get_attributes(attrs);
	EXPECT_EQ(42, attrs["age"].as_integer());
}

TEST_F(SQLiteAdapterTest, BindStringParameter) {
	adapter->query("INSERT INTO users (name) VALUES (?)", { "Test User" });

	auto stmt = adapter->query("SELECT name FROM users WHERE name = ?", { "Test User" });
	EXPECT_TRUE(stmt->fetch());

	attribute_map attrs;
	stmt->get_attributes(attrs);
	EXPECT_EQ("Test User", attrs["name"].as_string());
}

TEST_F(SQLiteAdapterTest, BindNullParameter) {
	adapter->query("INSERT INTO users (name, age) VALUES (?, ?)", { "Test", attribute() });

	auto stmt = adapter->query("SELECT age FROM users WHERE name = ?", { "Test" });
	EXPECT_TRUE(stmt->fetch());

	attribute_map attrs;
	stmt->get_attributes(attrs);
	EXPECT_TRUE(attrs["age"].is_null());
}

TEST_F(SQLiteAdapterTest, BindBoolParameter) {
	adapter->query("INSERT INTO users (name, active) VALUES (?, ?)", { "Test", true });

	auto stmt = adapter->query("SELECT active FROM users WHERE name = ?", { "Test" });
	EXPECT_TRUE(stmt->fetch());

	attribute_map attrs;
	stmt->get_attributes(attrs);
	EXPECT_EQ(1, attrs["active"].as_integer()); // SQLite stores bool as int
}

TEST_F(SQLiteAdapterTest, BindDoubleParameter) {
	adapter->query("INSERT INTO users (name, score) VALUES (?, ?)", { "Test", 98.7 });

	auto stmt = adapter->query("SELECT score FROM users WHERE name = ?", { "Test" });
	EXPECT_TRUE(stmt->fetch());

	attribute_map attrs;
	stmt->get_attributes(attrs);
	EXPECT_DOUBLE_EQ(98.7, attrs["score"].as_real());
}

TEST_F(SQLiteAdapterTest, BindMultipleParameters) {
	adapter->query("INSERT INTO users (name, age, email) VALUES (?, ?, ?)", { "Test", 25, "test@example.com" });

	auto stmt = adapter->query("SELECT * FROM users WHERE name = ? AND age = ?", { "Test", 25 });
	EXPECT_TRUE(stmt->fetch());

	attribute_map attrs;
	stmt->get_attributes(attrs);
	EXPECT_EQ("test@example.com", attrs["email"].as_string());
}

// ==============================================================================
// UPDATE Tests
// ==============================================================================

TEST_F(SQLiteAdapterTest, UpdateRow) {
	// Insert
	adapter->query("INSERT INTO users (name, age) VALUES (?, ?)", { "John", 30 });

	// Update
	auto update_stmt = adapter->query("UPDATE users SET age = ? WHERE name = ?", { 31, "John" });
	EXPECT_EQ(1u, update_stmt->affected_rows());

	// Verify
	auto stmt = adapter->query("SELECT age FROM users WHERE name = ?", { "John" });
	EXPECT_TRUE(stmt->fetch());

	attribute_map attrs;
	stmt->get_attributes(attrs);
	EXPECT_EQ(31, attrs["age"].as_integer());
}

TEST_F(SQLiteAdapterTest, UpdateNonMatchingRowHasZeroAffectedRows) {
	adapter->query("INSERT INTO users (name, age) VALUES (?, ?)", { "John", 30 });

	auto update_stmt = adapter->query("UPDATE users SET age = ? WHERE name = ?", { 31, "NoMatch" });
	EXPECT_EQ(0u, update_stmt->affected_rows());
}

// ==============================================================================
// DELETE Tests
// ==============================================================================

TEST_F(SQLiteAdapterTest, DeleteRow) {
	// Insert
	adapter->query("INSERT INTO users (name) VALUES (?)", { "ToDelete" });
	adapter->query("INSERT INTO users (name) VALUES (?)", { "ToKeep" });

	// Delete
	auto delete_stmt = adapter->query("DELETE FROM users WHERE name = ?", { "ToDelete" });
	EXPECT_EQ(1u, delete_stmt->affected_rows());

	// Verify count
	auto stmt = adapter->query("SELECT COUNT(*) as count FROM users", {});
	EXPECT_TRUE(stmt->fetch());

	attribute_map attrs;
	stmt->get_attributes(attrs);
	EXPECT_EQ(1, attrs["count"].as_integer());
}

TEST_F(SQLiteAdapterTest, SelectHasZeroAffectedRows) {
	auto insert_stmt = adapter->query("INSERT INTO users (name, age) VALUES (?, ?)", { "Alice", 25 });
	EXPECT_EQ(1u, insert_stmt->affected_rows());

	auto select_stmt = adapter->query("SELECT * FROM users WHERE name = ?", { "Alice" });
	EXPECT_EQ(0u, select_stmt->affected_rows());
	EXPECT_TRUE(select_stmt->fetch());
}

// ==============================================================================
// LIMIT/OFFSET Tests
// ==============================================================================

TEST_F(SQLiteAdapterTest, LimitClause) {
	std::string sql = "SELECT * FROM users";
	adapter->limit(sql, -1, 5);

	EXPECT_NE(std::string::npos, sql.find("LIMIT 5"));
	EXPECT_EQ(std::string::npos, sql.find("OFFSET"));
}

TEST_F(SQLiteAdapterTest, LimitWithOffset) {
	std::string sql = "SELECT * FROM users";
	adapter->limit(sql, 10, 5);

	EXPECT_NE(std::string::npos, sql.find("LIMIT 5"));
	EXPECT_NE(std::string::npos, sql.find("OFFSET 10"));
}

TEST_F(SQLiteAdapterTest, LimitInQuery) {
	// Insert 10 rows
	for (int i = 0; i < 10; i++) {
		adapter->query("INSERT INTO users (name, age) VALUES (?, ?)", { "User" + std::to_string(i), 20 + i });
	}

	// Query with LIMIT
	auto stmt = adapter->query("SELECT * FROM users LIMIT 3", {});

	int count = 0;
	while (stmt->fetch()) {
		count++;
	}

	EXPECT_EQ(3, count);
}

// ==============================================================================
// Field Type Conversion Tests
// ==============================================================================

TEST_F(SQLiteAdapterTest, IntegerFieldConversion) {
	adapter->query("INSERT INTO users (name, age) VALUES (?, ?)", { "Test", 42 });

	auto stmt = adapter->query("SELECT age FROM users", {});
	EXPECT_TRUE(stmt->fetch());

	attribute_map attrs;
	stmt->get_attributes(attrs);
	EXPECT_EQ(type_integer, attrs["age"].type());
	EXPECT_EQ(42, attrs["age"].as_integer());
}

TEST_F(SQLiteAdapterTest, TextFieldConversion) {
	adapter->query("INSERT INTO users (name) VALUES (?)", { "Test Name" });

	auto stmt = adapter->query("SELECT name FROM users", {});
	EXPECT_TRUE(stmt->fetch());

	attribute_map attrs;
	stmt->get_attributes(attrs);
	EXPECT_EQ(type_string, attrs["name"].type());
	EXPECT_EQ("Test Name", attrs["name"].as_string());
}

TEST_F(SQLiteAdapterTest, RealFieldConversion) {
	adapter->query("INSERT INTO users (name, score) VALUES (?, ?)", { "Test", 95.5 });

	auto stmt = adapter->query("SELECT score FROM users", {});
	EXPECT_TRUE(stmt->fetch());

	attribute_map attrs;
	stmt->get_attributes(attrs);
	EXPECT_EQ(type_decimal, attrs["score"].type());
	EXPECT_DOUBLE_EQ(95.5, attrs["score"].as_real());
}

TEST_F(SQLiteAdapterTest, NullFieldConversion) {
	adapter->query("INSERT INTO users (name, age) VALUES (?, ?)", { "Test", attribute() });

	auto stmt = adapter->query("SELECT age FROM users", {});
	EXPECT_TRUE(stmt->fetch());

	attribute_map attrs;
	stmt->get_attributes(attrs);
	EXPECT_EQ(type_null, attrs["age"].type());
	EXPECT_TRUE(attrs["age"].is_null());
}

// ==============================================================================
// accepts_limit_and_order_for_update_and_delete Tests
// ==============================================================================

TEST_F(SQLiteAdapterTest, AcceptsLimitForUpdateAndDelete) {
	EXPECT_FALSE(adapter->accepts_limit_and_order_for_update_and_delete());
}
