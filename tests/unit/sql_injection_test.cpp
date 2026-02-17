// ============================================================================
// sql_injection_test.cpp
//
// SPDX-FileCopyrightText: 2020-2026 Indridi Bjornsson
// SPDX-License-Identifier: MIT
// See file LICENSE.MIT for detail
//
// Tests for SQL injection prevention mechanisms
// ============================================================================

#include <attribute.h>
#include <connection.h>
#include <gtest/gtest.h>

#ifdef AR_SQLITE
#include <adapters/sqlite_adapter.h>
#include <config.h>
#include <model.h>
#endif

using namespace ar;

// ============================================================================
// quote_name() Tests - Identifier Escaping
// ============================================================================

TEST(SqlInjectionTest, QuoteNameBasic) {
	std::string result = connection::quote_name("users");
	EXPECT_EQ("`users`", result);
}

TEST(SqlInjectionTest, QuoteNameWithEmbeddedQuote) {
	std::string result = connection::quote_name("user`name");
	EXPECT_EQ("`user``name`", result);
}

TEST(SqlInjectionTest, QuoteNameAlreadyQuoted) {
	std::string result = connection::quote_name("`users`");
	EXPECT_EQ("`users`", result);
}

TEST(SqlInjectionTest, QuoteNameMultipleEmbeddedQuotes) {
	std::string result = connection::quote_name("tab`le`name");
	EXPECT_EQ("`tab``le``name`", result);
}

TEST(SqlInjectionTest, QuoteNameEmptyString) {
	std::string result = connection::quote_name("");
	EXPECT_EQ("``", result);
}

TEST(SqlInjectionTest, QuoteNameWithSpaces) {
	std::string result = connection::quote_name("user data");
	EXPECT_EQ("`user data`", result);
}

TEST(SqlInjectionTest, QuoteNameSqlInjectionAttempt) {
	// Malicious table name attempt
	std::string malicious = "users`; DROP TABLE users; --";
	std::string result = connection::quote_name(malicious);
	EXPECT_EQ("`users``; DROP TABLE users; --`", result);

	// The escaped version is safe - the backtick is doubled
	// Database sees literal: users`; DROP TABLE users; -- (as identifier, not SQL)
}

TEST(SqlInjectionTest, QuoteNameNestedQuotes) {
	std::string result = connection::quote_name("a`b`c`d");
	EXPECT_EQ("`a``b``c``d`", result);
}

// ============================================================================
// Attribute Binding Tests - Data Value Safety
// ============================================================================

TEST(SqlInjectionTest, AttributeStringWithQuotes) {
	// Attributes should store values safely without manual escaping
	attribute attr{ "Robert'; DROP TABLE users; --" };

	EXPECT_EQ(type_string, attr.type());
	EXPECT_EQ("Robert'; DROP TABLE users; --", attr.as_string());

	// When used with prepared statements, this is safe
	// The value is bound as a parameter, not concatenated into SQL
}

TEST(SqlInjectionTest, AttributeStringWithBackticks) {
	attribute attr{ "user`name" };
	EXPECT_EQ("user`name", attr.as_string());

	// Safe when used with parameter binding
}

TEST(SqlInjectionTest, AttributeNumericTypes) {
	// Numeric types can't be used for SQL injection
	attribute int_attr{ 42 };
	attribute double_attr{ 3.14 };
	attribute bool_attr{ true };

	EXPECT_EQ(42, int_attr.as_integer());
	EXPECT_DOUBLE_EQ(3.14, double_attr.as_real());
	EXPECT_TRUE(bool_attr.as_bool());
}

#ifdef AR_SQLITE

// ============================================================================
// Integration Tests - Real Database Operations
// ============================================================================

class SqlInjectionIntegrationTest : public ::testing::Test {
  protected:
	void SetUp() override {
		// Use in-memory SQLite database
		auto adapter = std::make_shared<sqlite_adapter>(":memory:");

		config::connection_map connections;
		connections["default"] = adapter;
		config::instance().set_connections(connections, "default");

		// Create users table
		std::string create_sql = R"(
			CREATE TABLE users (
				id INTEGER PRIMARY KEY AUTOINCREMENT,
				name TEXT NOT NULL,
				email TEXT
			)
		)";
		adapter->query(create_sql);
	}
};

// Test model
class User : public model {
  public:
	User() : model(st_table_name(), st_primary_key()) {}

	// Model metadata
	static const char* st_table_name() { return "users"; }
	static const char* st_primary_key() { return "id"; }

	// Field accessors
	int id() const { return static_cast<int>(get("id").as_integer()); }
	std::string name() const { return get("name").as_string(); }
	std::string email() const { return get("email").as_string(); }

	// Field setters
	void set_name(const std::string& value) { set("name", value); }
	void set_email(const std::string& value) { set("email", value); }
};

TEST_F(SqlInjectionIntegrationTest, InsertMaliciousStringValue) {
	// Attempt SQL injection via data value
	User user;
	user.set_name("Robert'); DROP TABLE users; --");
	user.set_email("test@example.com");

	EXPECT_TRUE(user.save());

	// Table should still exist - the malicious string was treated as data
	query<User> q;
	auto users = q.all();
	EXPECT_EQ(1, users.size());
	EXPECT_EQ("Robert'); DROP TABLE users; --", users[0].name());
}

TEST_F(SqlInjectionIntegrationTest, QueryMaliciousStringValue) {
	// Insert a user
	User user;
	user.set_name("Alice");
	user.set_email("alice@example.com");
	user.save();

	// Attempt SQL injection via WHERE clause value
	query<User> q;
	q.where("name = ?", attribute_vector{ attribute{ "Alice'; DROP TABLE users; --" } });
	auto users = q.all();

	// Should find no users (malicious string doesn't match)
	// Table should still exist
	EXPECT_EQ(0, users.size());

	// Verify table still exists by querying for the real user
	query<User> q2;
	q2.where("name = ?", attribute_vector{ attribute{ "Alice" } });
	auto real_users = q2.all();
	EXPECT_EQ(1, real_users.size());
}

TEST_F(SqlInjectionIntegrationTest, UpdateMaliciousStringValue) {
	// Insert a user
	User user;
	user.set_name("Bob");
	user.set_email("bob@example.com");
	user.save();

	int user_id = user.id();

	// Update with malicious string
	user.set_name("Bob'; DELETE FROM users WHERE '1'='1");
	EXPECT_TRUE(user.save());

	// Verify update worked and no SQL injection occurred
	query<User> q;
	User updated = q.find(user_id);
	EXPECT_EQ("Bob'; DELETE FROM users WHERE '1'='1", updated.name());

	// Only one user should exist (not deleted by injection attempt)
	query<User> q2;
	EXPECT_EQ(1, q2.count());
}

TEST_F(SqlInjectionIntegrationTest, SpecialCharactersInData) {
	User user;
	user.set_name("O'Brien"); // Single quote
	user.set_email("user@example.com");
	EXPECT_TRUE(user.save());

	query<User> q;
	q.where("name = ?", attribute_vector{ attribute{ "O'Brien" } });
	auto users = q.all();

	EXPECT_EQ(1, users.size());
	EXPECT_EQ("O'Brien", users[0].name());
}

TEST_F(SqlInjectionIntegrationTest, BackticksInData) {
	User user;
	user.set_name("user`name"); // Backtick
	user.set_email("test@example.com");
	EXPECT_TRUE(user.save());

	query<User> q;
	User retrieved = q.find(user.id());
	EXPECT_EQ("user`name", retrieved.name());
}

TEST_F(SqlInjectionIntegrationTest, NullByteInData) {
	// Null bytes in strings - tests safe handling
	std::string name_with_null = std::string("Alice") + '\0' + "Malicious";

	User user;
	user.set_name(name_with_null);
	user.set_email("test@example.com");
	EXPECT_TRUE(user.save());

	// SQLite stores strings as null-terminated, so the string gets truncated at null byte
	// This is expected behavior - the important thing is it doesn't cause crashes or SQL injection
	query<User> q;
	User retrieved = q.find(user.id());
	// String is truncated at null byte (SQLite behavior)
	EXPECT_EQ("Alice", retrieved.name());
	EXPECT_EQ(5, retrieved.name().size()); // Only "Alice" is stored
}

TEST_F(SqlInjectionIntegrationTest, UnicodeInData) {
	User user;
	user.set_name("用户名"); // Chinese characters
	user.set_email("unicode@example.com");
	EXPECT_TRUE(user.save());

	query<User> q;
	User retrieved = q.find(user.id());
	EXPECT_EQ("用户名", retrieved.name());
}

TEST_F(SqlInjectionIntegrationTest, VeryLongStringValue) {
	// Test with very long string (potential buffer overflow attempts)
	std::string long_string(10000, 'A');
	long_string += "'; DROP TABLE users; --";

	User user;
	user.set_name(long_string);
	user.set_email("test@example.com");
	EXPECT_TRUE(user.save());

	query<User> q;
	User retrieved = q.find(user.id());
	EXPECT_EQ(long_string, retrieved.name());
}

#endif // AR_SQLITE

// ============================================================================
// Best Practices Documentation
// ============================================================================

TEST(SqlInjectionTest, DocumentSafePractices) {
	// This test documents safe vs unsafe practices

	// ✅ SAFE: Table/column names with quote_name()
	std::string safe_table = connection::quote_name("users");
	EXPECT_EQ("`users`", safe_table);

	// Even malicious table names are escaped
	std::string malicious_table = connection::quote_name("users`; DROP TABLE users; --");
	EXPECT_EQ("`users``; DROP TABLE users; --`", malicious_table);

	// ✅ SAFE: Using prepared statements with placeholders
	// query<User>().where("name = ?", {attribute{user_input}})
	// Values are bound separately, never concatenated into SQL

	// ❌ UNSAFE: String concatenation (DON'T DO THIS)
	// std::string unsafe_sql = "SELECT * FROM users WHERE name = '" + user_input + "'";
	// This would allow SQL injection if user_input contains quotes
}
