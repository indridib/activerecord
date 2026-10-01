// ============================================================================
// adapter_tests.cpp
//
// Parameterized integration tests that run against multiple database adapters
// This ensures all adapters behave consistently
// ============================================================================

#include "test_adapters.h"

#include <activerecord.h>
#include <config.h>
#include <gtest/gtest.h>
#include <model.h>
#include <sql_builder.h>

using namespace ar;
using namespace ar::test;

// ==============================================================================
// Test Model
// ==============================================================================

namespace {

class User : public model {
  public:
	static const char* st_table_name() { return "users"; }
	static const char* st_primary_key() { return "id"; }

	User() : model(st_table_name(), st_primary_key()) {}
};

} // namespace

// ==============================================================================
// Parameterized Test Fixture
// ==============================================================================

class DatabaseAdapterTest : public AdapterTest {
  protected:
	table_list table_definitions(AdapterType type) const override { return { { "users", users_columns(type) } }; }
};

// ==============================================================================
// Shared Integration Tests
// ==============================================================================

TEST_P(DatabaseAdapterTest, InsertAndRetrieve) {
	User user;
	user.set("name", "John Doe");
	user.set("email", "john@example.com");
	user.set("age", 30);

	EXPECT_TRUE(user.save());

	// Retrieve and verify
	query<User> q;
	auto users = q.where("name = ?", "John Doe").all();
	ASSERT_EQ(1, users.size());
	EXPECT_EQ("John Doe", users[0]["name"].as_string());
	EXPECT_EQ("john@example.com", users[0]["email"].as_string());
	EXPECT_EQ(30, users[0]["age"].as_integer());
}

TEST_P(DatabaseAdapterTest, UpdateRecord) {
	User user;
	user.set("name", "Jane");
	user.set("age", 25);
	user.save();

	user.set("age", 26);
	EXPECT_TRUE(user.save());

	query<User> q;
	auto users = q.where("name = ?", "Jane").all();
	ASSERT_EQ(1, users.size());
	EXPECT_EQ(26, users[0]["age"].as_integer());
}

TEST_P(DatabaseAdapterTest, QueryWithWhere) {
	// Insert multiple users
	for (int i = 0; i < 5; ++i) {
		User u;
		u.set("name", "User" + std::to_string(i));
		u.set("age", 20 + i * 5);
		u.save();
	}

	// Query users older than 25
	query<User> q;
	auto users = q.where("age > ?", 25).all();
	EXPECT_GE(users.size(), 2);
}

TEST_P(DatabaseAdapterTest, QueryWithOrderBy) {
	User u1, u2, u3;
	u1.set("name", "Charlie");
	u1.set("age", 30);
	u1.save();

	u2.set("name", "Alice");
	u2.set("age", 25);
	u2.save();

	u3.set("name", "Bob");
	u3.set("age", 35);
	u3.save();

	query<User> q;
	auto users = q.order("name ASC").all();
	ASSERT_GE(users.size(), 3);
	EXPECT_EQ("Alice", users[0]["name"].as_string());
}

TEST_P(DatabaseAdapterTest, QueryWithLimit) {
	for (int i = 0; i < 10; ++i) {
		User u;
		u.set("name", "User" + std::to_string(i));
		u.save();
	}

	query<User> q;
	auto users = q.limit(5).all();
	EXPECT_EQ(5, users.size());
}

TEST_P(DatabaseAdapterTest, QueryCount) {
	for (int i = 0; i < 7; ++i) {
		User u;
		u.set("name", "User" + std::to_string(i));
		u.save();
	}

	query<User> q;
	EXPECT_EQ(7, q.count());
}

TEST_P(DatabaseAdapterTest, ParameterBinding) {
	User u;
	u.set("name", "Test's Name"); // Name with apostrophe
	u.set("email", "test@example.com");
	EXPECT_TRUE(u.save());

	query<User> q;
	auto users = q.where("name = ?", "Test's Name").all();
	ASSERT_EQ(1, users.size());
	EXPECT_EQ("Test's Name", users[0]["name"].as_string());
}

TEST_P(DatabaseAdapterTest, NullValues) {
	User u;
	u.set("name", "NoEmail");
	u.set("email", attribute()); // NULL
	u.save();

	query<User> q;
	auto users = q.where("name = ?", "NoEmail").all();
	ASSERT_EQ(1, users.size());
	EXPECT_TRUE(users[0]["email"].is_null());
}

TEST_P(DatabaseAdapterTest, BooleanValues) {
	User u;
	u.set("name", "Active User");
	u.set("active", true);
	u.save();

	query<User> q;
	auto users = q.where("name = ?", "Active User").all();
	ASSERT_EQ(1, users.size());
	// Note: Boolean handling may vary by database
}

#ifdef AR_MSSQL
// ==============================================================================
// MS-SQL: the id read back after an INSERT must be the inserted row's, even when an
// INSERT trigger inserts into another identity table (@@IDENTITY would return that id)
// ==============================================================================

class MssqlInsertIdTest : public ::testing::Test {
  protected:
	void SetUp() override {
		adapter_ = create_connection(AdapterType::MSSQL);
		config::instance().set_connections({ { "default", adapter_ } }, "default");
		TearDown();
		adapter_->query("CREATE TABLE users (" + users_columns(AdapterType::MSSQL) + ")", {});
		// Seeded at 100 so a trigger identity can't coincide with the users id
		adapter_->query("CREATE TABLE users_audit (id INT IDENTITY(100,1) PRIMARY KEY, user_id INT)", {});
		adapter_->query("CREATE TRIGGER users_audit_trg ON users AFTER INSERT AS "
						"INSERT INTO users_audit (user_id) SELECT id FROM inserted",
						{});
	}

	void TearDown() override {
		if (adapter_) {
			adapter_->query("DROP TABLE IF EXISTS users", {}); // drops the trigger too
			adapter_->query("DROP TABLE IF EXISTS users_audit", {});
		}
	}

	std::shared_ptr<connection> adapter_;
};

TEST_F(MssqlInsertIdTest, IgnoresTriggerIdentity) {
	User user;
	user.set("name", "Trigger");
	ASSERT_TRUE(user.save());

	query<User> q;
	auto users = q.where("name = ?", "Trigger").all();
	ASSERT_EQ(1, users.size());
	EXPECT_EQ(users[0]["id"].as_integer(), user["id"].as_integer());
	EXPECT_LT(user["id"].as_integer(), 100); // not the users_audit identity
}
#endif

// ==============================================================================
// Test Instantiation
// ==============================================================================

INSTANTIATE_TEST_SUITE_P(AllAdapters, DatabaseAdapterTest, ::testing::ValuesIn(enabled_adapters()), adapter_param_name);
