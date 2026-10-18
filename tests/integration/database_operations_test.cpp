// ============================================================================
// database_operations_test.cpp
//
// SPDX-FileCopyrightText: 2020-2026 Indridi Bjornsson
// SPDX-License-Identifier: MIT
// See file LICENSE for details
//
// End-to-end integration tests with model + adapter + query builder
//

#include "test_adapters.h"

#include <activerecord/activerecord.h>
#include <activerecord/config.h>
#include <activerecord/datetime.h>
#include <activerecord/model.h>
#include <activerecord/sql_builder.h>
#include <activerecord/typed_model.h>
#include <gtest/gtest.h>

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

	using model::attributes_;
	using model::new_record_;
};

} // namespace

// ==============================================================================
// Test Fixture
// ==============================================================================

class DatabaseOperationsTest : public AdapterTest {
  protected:
	table_list table_definitions(AdapterType type) const override {
		return { { "users", users_columns(type) },
				 { "measurements", std::format("{}, value {}", pk_column(type), double_type(type)) } };
	}
};

namespace {

class Measurement : public model {
  public:
	static const char* st_table_name() { return "measurements"; }
	static const char* st_primary_key() { return "id"; }

	Measurement() : model(st_table_name(), st_primary_key()) {}
};

} // namespace

// ==============================================================================
// Model CRUD Tests
// ==============================================================================

TEST_P(DatabaseOperationsTest, CreateAndSaveUser) {
	User user;
	user.set("name", "John Doe");
	user.set("email", "john@example.com");
	user.set("age", 30);

	EXPECT_TRUE(user.is_new_record());
	EXPECT_TRUE(user.is_dirty());

	bool saved = user.save();
	EXPECT_TRUE(saved);
}

TEST_P(DatabaseOperationsTest, UpdateUser) {
	// First, insert a user manually
	auto conn = config::instance().get_connection();
	conn->query("INSERT INTO users (name, email, age) VALUES (?, ?, ?)", { "Jane", "jane@example.com", 25 });

	// Create model with attributes (simulating a loaded model)
	User user;
	user.attributes_["id"] = 1;
	user.attributes_["name"] = "Jane";
	user.attributes_["email"] = "jane@example.com";
	user.attributes_["age"] = 25;
	user.new_record_ = false;

	// Update
	user.set("age", 26);
	EXPECT_TRUE(user.is_dirty());
	EXPECT_TRUE(user.attribute_is_dirty("age"));

	bool updated = user.save();
	EXPECT_TRUE(updated);
	EXPECT_FALSE(user.is_dirty());
}

TEST_P(DatabaseOperationsTest, DestroyDeletesPersistedRecord) {
	auto conn = config::instance().get_connection();
	conn->query("INSERT INTO users (name, email, age) VALUES (?, ?, ?)", { "DeleteMe", "delete@example.com", 22 });

	auto user = query<User>().where("name = ?", "DeleteMe").first_or_throw();
	EXPECT_FALSE(user.is_new_record());

	EXPECT_TRUE(user.destroy());
	EXPECT_TRUE(user.is_new_record());

	auto deleted = query<User>().where("name = ?", "DeleteMe").first();
	EXPECT_FALSE(deleted.has_value());
}

TEST_P(DatabaseOperationsTest, DoubleValuesRoundTripExactly) {
	const std::vector<double> values{ 1.234567891, 0.1, -98765.4321012345, 1e-10, 123456789.123456789, 1e20 };
	for (const double value : values) {
		Measurement m;
		m.set("value", value);
		ASSERT_TRUE(m.save());
	}

	const auto rows = query<Measurement>().order("id").all();
	ASSERT_EQ(values.size(), rows.size());
	for (std::size_t i = 0; i < values.size(); ++i) {
		EXPECT_EQ(values[i], rows[i].get("value").as_real()) << "value " << i;
	}
}

TEST_P(DatabaseOperationsTest, SetMultipleAttributes) {
	User user;
	attribute_map attrs;
	attrs["name"] = "Alice";
	attrs["email"] = "alice@example.com";
	attrs["age"] = 28;

	user.set_attributes(attrs);

	EXPECT_EQ("Alice", user.get("name").as_string());
	EXPECT_EQ("alice@example.com", user.get("email").as_string());
	EXPECT_EQ(28, user.get("age").as_integer());
}

// ==============================================================================
// Query Builder Tests
// ==============================================================================

TEST_P(DatabaseOperationsTest, QueryAll) {
	// Insert test data
	auto conn = config::instance().get_connection();
	conn->query("INSERT INTO users (name, age) VALUES (?, ?)", { "User1", 20 });
	conn->query("INSERT INTO users (name, age) VALUES (?, ?)", { "User2", 30 });
	conn->query("INSERT INTO users (name, age) VALUES (?, ?)", { "User3", 40 });

	// Query all
	query<User> q;
	std::vector<User> users = q.all();

	EXPECT_EQ(3, users.size());
	EXPECT_FALSE(users[0].is_new_record());
}

TEST_P(DatabaseOperationsTest, QueryWithWhere) {
	// Insert test data
	auto conn = config::instance().get_connection();
	conn->query("INSERT INTO users (name, age) VALUES (?, ?)", { "Young", 20 });
	conn->query("INSERT INTO users (name, age) VALUES (?, ?)", { "Old", 60 });

	// Query with WHERE
	query<User> q;
	q.where("age > ?", 30);
	std::vector<User> users = q.all();

	EXPECT_EQ(1, users.size());
	EXPECT_EQ("Old", users[0].get("name").as_string());
}

TEST_P(DatabaseOperationsTest, QueryWithOrder) {
	// Insert test data in random order
	auto conn = config::instance().get_connection();
	conn->query("INSERT INTO users (name, age) VALUES (?, ?)", { "C", 30 });
	conn->query("INSERT INTO users (name, age) VALUES (?, ?)", { "A", 20 });
	conn->query("INSERT INTO users (name, age) VALUES (?, ?)", { "B", 25 });

	// Query with ORDER BY
	query<User> q;
	q.order("name ASC");
	std::vector<User> users = q.all();

	EXPECT_EQ(3, users.size());
	EXPECT_EQ("A", users[0].get("name").as_string());
	EXPECT_EQ("B", users[1].get("name").as_string());
	EXPECT_EQ("C", users[2].get("name").as_string());
}

TEST_P(DatabaseOperationsTest, QueryWhereInFiltersRows) {
	auto conn = config::instance().get_connection();
	conn->query("INSERT INTO users (name, age) VALUES (?, ?)", { "Age20", 20 });
	conn->query("INSERT INTO users (name, age) VALUES (?, ?)", { "Age30", 30 });
	conn->query("INSERT INTO users (name, age) VALUES (?, ?)", { "Age40", 40 });

	query<User> q;
	auto users = q.where_in("age", { 20, 40 }).order("age ASC").all();

	ASSERT_EQ(2u, users.size());
	EXPECT_EQ("Age20", users[0].get("name").as_string());
	EXPECT_EQ("Age40", users[1].get("name").as_string());
}

TEST_P(DatabaseOperationsTest, QueryWhereInEmptyReturnsNoRows) {
	auto conn = config::instance().get_connection();
	conn->query("INSERT INTO users (name, age) VALUES (?, ?)", { "OnlyUser", 33 });

	query<User> q;
	const attribute_vector none{};
	auto users = q.where_in("id", none).all();

	EXPECT_TRUE(users.empty());
}

TEST_P(DatabaseOperationsTest, QueryWhereNotFiltersRows) {
	auto conn = config::instance().get_connection();
	conn->query("INSERT INTO users (name, age) VALUES (?, ?)", { "Young", 18 });
	conn->query("INSERT INTO users (name, age) VALUES (?, ?)", { "Adult", 35 });

	query<User> q;
	auto users = q.where_not("age < ?", { 30 }).all();

	ASSERT_EQ(1u, users.size());
	EXPECT_EQ("Adult", users[0].get("name").as_string());
}

TEST_P(DatabaseOperationsTest, QueryChainedWhereHelpersAreCombinedWithAnd) {
	auto conn = config::instance().get_connection();
	conn->query("INSERT INTO users (name, age, active) VALUES (?, ?, ?)", { "A", 20, true });
	conn->query("INSERT INTO users (name, age, active) VALUES (?, ?, ?)", { "B", 40, true });
	conn->query("INSERT INTO users (name, age, active) VALUES (?, ?, ?)", { "C", 40, false });

	query<User> q;
	auto users = q.where("active = ?", true).where_in("age", { 40 }).all();

	ASSERT_EQ(1u, users.size());
	EXPECT_EQ("B", users[0].get("name").as_string());
}

TEST_P(DatabaseOperationsTest, QueryDistinctReturnsUniqueValues) {
	auto conn = config::instance().get_connection();
	conn->query("INSERT INTO users (name, age) VALUES (?, ?)", { "A1", 21 });
	conn->query("INSERT INTO users (name, age) VALUES (?, ?)", { "A2", 21 });
	conn->query("INSERT INTO users (name, age) VALUES (?, ?)", { "B1", 35 });

	query<User> q;
	auto rows = q.select("age").distinct().order("age ASC").all();

	ASSERT_EQ(2u, rows.size());
	EXPECT_EQ(21, rows[0].get("age").as_integer());
	EXPECT_EQ(35, rows[1].get("age").as_integer());
}

TEST_P(DatabaseOperationsTest, QueryWithLimit) {
	// Insert test data
	auto conn = config::instance().get_connection();
	for (int i = 0; i < 10; i++) {
		conn->query("INSERT INTO users (name, age) VALUES (?, ?)", { "User" + std::to_string(i), 20 + i });
	}

	// Query with LIMIT
	query<User> q;
	q.limit(3);
	std::vector<User> users = q.all();

	EXPECT_EQ(3, users.size());
}

TEST_P(DatabaseOperationsTest, QueryWithLimitAndOffset) {
	// Insert test data
	auto conn = config::instance().get_connection();
	for (int i = 0; i < 10; i++) {
		conn->query("INSERT INTO users (name, age) VALUES (?, ?)", { "User" + std::to_string(i), 20 + i });
	}

	// Query with LIMIT and OFFSET
	query<User> q;
	q.limit(2).offset(3);
	std::vector<User> users = q.all();

	EXPECT_EQ(2, users.size());
	// Should get users at index 3 and 4
}

TEST_P(DatabaseOperationsTest, QueryCount) {
	// Insert test data
	auto conn = config::instance().get_connection();
	for (int i = 0; i < 7; i++) {
		conn->query("INSERT INTO users (name, age) VALUES (?, ?)", { "User" + std::to_string(i), 20 + i });
	}

	// Count all
	query<User> q;
	long count = q.count();

	EXPECT_EQ(7, count);
}

TEST_P(DatabaseOperationsTest, QueryCountWithWhere) {
	// Insert test data
	auto conn = config::instance().get_connection();
	conn->query("INSERT INTO users (name, age) VALUES (?, ?)", { "Young1", 20 });
	conn->query("INSERT INTO users (name, age) VALUES (?, ?)", { "Young2", 22 });
	conn->query("INSERT INTO users (name, age) VALUES (?, ?)", { "Old", 60 });

	// Count with WHERE
	query<User> q;
	q.where("age < ?", 30);
	long count = q.count();

	EXPECT_EQ(2, count);
}

TEST_P(DatabaseOperationsTest, QueryCountIgnoresLimitAndOffset) {
	auto conn = config::instance().get_connection();
	for (int i = 0; i < 7; i++) {
		conn->query("INSERT INTO users (name, age) VALUES (?, ?)", { "User" + std::to_string(i), 20 + i });
	}

	query<User> q;
	q.limit(2).offset(3);
	EXPECT_EQ(7, q.count());
}

TEST_P(DatabaseOperationsTest, QueryCountIgnoresOrder) {
	auto conn = config::instance().get_connection();
	for (int i = 0; i < 5; i++) {
		conn->query("INSERT INTO users (name, age) VALUES (?, ?)", { "User" + std::to_string(i), 20 + i });
	}

	query<User> q;
	q.order("name DESC");
	EXPECT_EQ(5, q.count());
}

TEST_P(DatabaseOperationsTest, QueryExists) {
	// Insert test data
	auto conn = config::instance().get_connection();
	conn->query("INSERT INTO users (name, age) VALUES (?, ?)", { "Test", 25 });

	// Check exists
	query<User> q1;
	q1.where("name = ?", "Test");
	EXPECT_TRUE(q1.exists());

	query<User> q2;
	q2.where("name = ?", "NonExistent");
	EXPECT_FALSE(q2.exists());
}

TEST_P(DatabaseOperationsTest, QueryDeleteAllWithWhere) {
	auto conn{ config::instance().get_connection() };
	conn->query("INSERT INTO users (name, age) VALUES (?, ?)", { "A", 20 });
	conn->query("INSERT INTO users (name, age) VALUES (?, ?)", { "B", 35 });
	conn->query("INSERT INTO users (name, age) VALUES (?, ?)", { "C", 40 });

	query<User> q;
	const long deleted{ q.where("age >= ?", 30).delete_all() };
	EXPECT_EQ(2, deleted);

	query<User> remaining;
	EXPECT_EQ(1, remaining.count());
	auto first{ remaining.first_or_throw() };
	EXPECT_EQ("A", first.get("name").as_string());
}

TEST_P(DatabaseOperationsTest, QueryFind) {
	// Insert test data
	auto conn = config::instance().get_connection();
	conn->query("INSERT INTO users (name, age) VALUES (?, ?)", { "FindMe", 25 });

	// Get the ID (would be 1 for first insert)
	auto stmt = conn->query("SELECT id FROM users WHERE name = ?", { "FindMe" });
	stmt->fetch();
	attribute_map attrs;
	stmt->get_attributes(attrs);
	const auto user_id = static_cast<long>(attrs["id"].as_integer());

	// Find by ID
	query<User> q;
	User user = q.find(user_id);

	EXPECT_EQ("FindMe", user.get("name").as_string());
	EXPECT_EQ(25, user.get("age").as_integer());
	EXPECT_FALSE(user.is_new_record());
}

TEST_P(DatabaseOperationsTest, QueryFindMultipleIds) {
	auto conn = config::instance().get_connection();
	conn->query("INSERT INTO users (name, age) VALUES (?, ?)", { "FindA", 21 });
	conn->query("INSERT INTO users (name, age) VALUES (?, ?)", { "FindB", 22 });
	conn->query("INSERT INTO users (name, age) VALUES (?, ?)", { "FindC", 23 });

	query<User> q;
	auto users = q.find(std::vector<long>{ 1, 3 });

	ASSERT_EQ(2, users.size());
	bool has_a = false;
	bool has_c = false;
	for (const auto& user : users) {
		const std::string name = user.get("name").as_string();
		if (name == "FindA")
			has_a = true;
		if (name == "FindC")
			has_c = true;
	}
	EXPECT_TRUE(has_a);
	EXPECT_TRUE(has_c);
}

TEST_P(DatabaseOperationsTest, QueryFindMultipleIdsEmptyInputReturnsEmpty) {
	query<User> q;
	auto users = q.find(std::vector<long>{});
	EXPECT_TRUE(users.empty());
}

TEST_P(DatabaseOperationsTest, QueryFirst) {
	// Insert test data
	auto conn = config::instance().get_connection();
	conn->query("INSERT INTO users (name, age) VALUES (?, ?)", { "First", 10 });
	conn->query("INSERT INTO users (name, age) VALUES (?, ?)", { "Second", 20 });

	// Get first
	query<User> q;
	q.order("id ASC");
	auto first = q.first();
	ASSERT_TRUE(first.has_value());

	EXPECT_EQ("First", first->get("name").as_string());
}

TEST_P(DatabaseOperationsTest, QueryFirstReturnsNulloptWhenEmpty) {
	query<User> q;
	EXPECT_FALSE(q.first().has_value());
}

TEST_P(DatabaseOperationsTest, QueryLastReturnsNulloptWhenEmpty) {
	query<User> q;
	EXPECT_FALSE(q.last().has_value());
}

TEST_P(DatabaseOperationsTest, QueryFirstOrThrowThrowsWhenEmpty) {
	query<User> q;
	EXPECT_THROW(q.first_or_throw(), ex::item_not_found);
}

TEST_P(DatabaseOperationsTest, QueryLastOrThrowThrowsWhenEmpty) {
	query<User> q;
	EXPECT_THROW(q.last_or_throw(), ex::item_not_found);
}

TEST_P(DatabaseOperationsTest, QueryLastUsesPrimaryKeyDescWhenNoOrder) {
	auto conn = config::instance().get_connection();
	conn->query("INSERT INTO users (name, age) VALUES (?, ?)", { "First", 10 });
	conn->query("INSERT INTO users (name, age) VALUES (?, ?)", { "Last", 20 });

	query<User> q;
	auto last = q.last();
	ASSERT_TRUE(last.has_value());
	EXPECT_EQ("Last", last->get("name").as_string());
}

TEST_P(DatabaseOperationsTest, QueryLastRespectsReversedOrder) {
	auto conn = config::instance().get_connection();
	conn->query("INSERT INTO users (name, age) VALUES (?, ?)", { "A", 10 });
	conn->query("INSERT INTO users (name, age) VALUES (?, ?)", { "B", 20 });
	conn->query("INSERT INTO users (name, age) VALUES (?, ?)", { "C", 30 });

	query<User> q;
	q.order("age ASC");
	auto last = q.last();
	ASSERT_TRUE(last.has_value());
	EXPECT_EQ("C", last->get("name").as_string());
}

TEST_P(DatabaseOperationsTest, QueryFirstOrThrowAndLastOrThrowReturnRecords) {
	auto conn = config::instance().get_connection();
	conn->query("INSERT INTO users (name, age) VALUES (?, ?)", { "A", 10 });
	conn->query("INSERT INTO users (name, age) VALUES (?, ?)", { "B", 20 });

	query<User> first_query;
	first_query.order("id ASC");
	User first = first_query.first_or_throw();
	EXPECT_EQ("A", first.get("name").as_string());

	query<User> last_query;
	last_query.order("id ASC");
	User last = last_query.last_or_throw();
	EXPECT_EQ("B", last.get("name").as_string());
}

TEST_P(DatabaseOperationsTest, QuerySupportsRangeBasedIteration) {
	auto conn = config::instance().get_connection();
	conn->query("INSERT INTO users (name, age) VALUES (?, ?)", { "A", 10 });
	conn->query("INSERT INTO users (name, age) VALUES (?, ?)", { "B", 20 });
	conn->query("INSERT INTO users (name, age) VALUES (?, ?)", { "C", 30 });

	query<User> q;
	q.order("id ASC");

	int count = 0;
	std::string first_name;
	for (const auto& user : q) {
		if (count == 0) {
			first_name = user.get("name").as_string();
		}
		++count;
	}

	EXPECT_EQ(3, count);
	EXPECT_EQ("A", first_name);
}

// ==============================================================================
// Complex Query Tests
// ==============================================================================

TEST_P(DatabaseOperationsTest, ComplexQuery) {
	// Insert test data
	auto conn = config::instance().get_connection();
	for (int i = 0; i < 20; i++) {
		conn->query("INSERT INTO users (name, age, active) VALUES (?, ?, ?)",
					{ "User" + std::to_string(i), 20 + i, i % 2 == 0 }); // Even IDs are active
	}

	// Complex query: active users, age > 25, ordered by age desc, limit 5
	query<User> q;
	q.where("active = ? AND age > ?", { true, 25 }).order("age DESC").limit(5);

	std::vector<User> users = q.all();

	EXPECT_EQ(5, users.size());
	// First user should have highest age among active users > 25
}

// ==============================================================================
// Model Serialization Tests
// ==============================================================================

TEST_P(DatabaseOperationsTest, ModelToJson) {
	// Insert test data
	auto conn = config::instance().get_connection();
	conn->query("INSERT INTO users (name, email, age) VALUES (?, ?, ?)", { "John", "john@example.com", 30 });

	// Load into model
	query<User> q;
	auto user = q.first();
	ASSERT_TRUE(user.has_value());

	std::string json = user->to_json_string();

	EXPECT_NE(std::string::npos, json.find("\"name\""));
	EXPECT_NE(std::string::npos, json.find("\"John\""));
	EXPECT_NE(std::string::npos, json.find("\"email\""));
}

TEST_P(DatabaseOperationsTest, ModelToXml) {
	// Insert test data
	auto conn = config::instance().get_connection();
	conn->query("INSERT INTO users (name, email) VALUES (?, ?)", { "Jane", "jane@example.com" });

	// Load into model
	query<User> q;
	auto user = q.first();
	ASSERT_TRUE(user.has_value());

	std::string xml = user->to_xml_string();

	EXPECT_NE(std::string::npos, xml.find("<users>"));
	EXPECT_NE(std::string::npos, xml.find("</users>"));
	EXPECT_NE(std::string::npos, xml.find("<name>Jane</name>"));
}

// ==============================================================================
// Dirty Tracking Integration Tests
// ==============================================================================

TEST_P(DatabaseOperationsTest, DirtyTrackingAfterLoad) {
	// Insert test data
	auto conn = config::instance().get_connection();
	conn->query("INSERT INTO users (name, age) VALUES (?, ?)", { "Test", 25 });

	// Load model
	query<User> q;
	auto user = q.first();
	ASSERT_TRUE(user.has_value());

	// Should not be dirty after load
	EXPECT_FALSE(user->is_dirty());

	// Modify
	user->set("age", 26);
	EXPECT_TRUE(user->is_dirty());
	EXPECT_TRUE(user->attribute_is_dirty("age"));

	// Save
	user->save();
	EXPECT_FALSE(user->is_dirty());
}

#ifdef AR_SQLITE
// ==============================================================================
// Generated-SQL tests (SQLite only): they assert query-builder output, which is
// adapter-specific text (e.g. MS-SQL uses OFFSET ... FETCH instead of LIMIT)
// ==============================================================================

class QueryBuilderSqlTest : public ::testing::Test {
  protected:
	void SetUp() override {
		config::instance().set_connections({ { "default", create_connection(AdapterType::SQLite) } }, "default");
	}
	void TearDown() override { config::instance().set_connections({}); }
};

TEST_F(QueryBuilderSqlTest, QueryToSqlIncludesCurrentScope) {
	query<User> q;
	q.select("id, name").where("age > ?", 18).order("name ASC").limit(5).offset(2);

	const std::string sql = q.to_sql();
	EXPECT_NE(std::string::npos, sql.find("SELECT id, name FROM users"));
	EXPECT_NE(std::string::npos, sql.find("WHERE age > ?"));
	EXPECT_NE(std::string::npos, sql.find("ORDER BY name ASC"));
	EXPECT_NE(std::string::npos, sql.find("LIMIT 5"));
	EXPECT_NE(std::string::npos, sql.find("OFFSET 2"));
}

TEST_F(QueryBuilderSqlTest, QueryBindValuesExposeWhereParameters) {
	query<User> q;
	q.where("age > ? AND active = ?", { 18, true });

	const auto& values = q.bind_values();
	ASSERT_EQ(2u, values.size());
	EXPECT_EQ(18, values[0].as_integer());
	EXPECT_TRUE(values[1].as_bool());
}

TEST_F(QueryBuilderSqlTest, QueryOrderReplacesPreviousOrder) {
	query<User> q;
	q.order("name ASC").order("id DESC");

	const std::string sql = q.to_sql();
	EXPECT_NE(std::string::npos, sql.find("ORDER BY id DESC"));
	EXPECT_EQ(std::string::npos, sql.find("ORDER BY name ASC"));
}
#endif

// ==============================================================================
// Timestamp & Version Model
// ==============================================================================

class TimestampedUser : public model {
  public:
	static const char* st_table_name() { return "timestamped_users"; }
	static const char* st_primary_key() { return "id"; }

	TimestampedUser() : model(st_table_name(), st_primary_key()) {}
};

// A model without timestamp/version columns
class PlainItem : public model {
  public:
	static const char* st_table_name() { return "plain_items"; }
	static const char* st_primary_key() { return "id"; }

	PlainItem() : model(st_table_name(), st_primary_key()) {}
};

// ==============================================================================
// Timestamp & Version Test Fixture
// ==============================================================================

class TimestampVersionTest : public AdapterTest {
  protected:
	table_list table_definitions(AdapterType type) const override {
		const std::string text{ text_type(type) };
		const std::string ts{ timestamp_type(type) };
		return {
			// Table with timestamp and version columns (timestamps stored as Unix time)
			{ "timestamped_users",
			  std::format("{}, name {} NOT NULL, email {}, created_at {}, updated_at {}, version INTEGER",
						  pk_column(type), text, text, ts, ts) },
			// Table without timestamp/version columns
			{ "plain_items", std::format("{}, name {} NOT NULL", pk_column(type), text) },
			// Table with only created_at (no updated_at, no version)
			{ "partial_items", std::format("{}, name {} NOT NULL, created_at {}", pk_column(type), text, ts) },
		};
	}
};

// ==============================================================================
// Timestamp & Version Integration Tests
// ==============================================================================

TEST_P(TimestampVersionTest, InsertSetsCreatedAtAndUpdatedAt) {
	TimestampedUser user;
	user.init_record_tracking(true);
	user.set("name", "Alice");

	datetime before = datetime::now();
	user.save();

	// Reload from DB
	auto loaded = query<TimestampedUser>().where("name = ?", "Alice").first_or_throw();

	EXPECT_FALSE(loaded.get("created_at").is_null());
	EXPECT_FALSE(loaded.get("updated_at").is_null());

	// Both should be approximately "now"
	auto created = loaded.get("created_at").as_date().timestamp();
	auto updated = loaded.get("updated_at").as_date().timestamp();
	auto before_ts = before.timestamp();

	EXPECT_GE(created, before_ts - 2);
	EXPECT_LE(created, before_ts + 2);
	EXPECT_GE(updated, before_ts - 2);
	EXPECT_LE(updated, before_ts + 2);
}

TEST_P(TimestampVersionTest, InsertSetsVersionToOne) {
	TimestampedUser user;
	user.init_record_tracking(true);
	user.set("name", "Bob");
	user.save();

	auto loaded = query<TimestampedUser>().where("name = ?", "Bob").first_or_throw();
	EXPECT_EQ(1, loaded.get("version").as_integer());
}

TEST_P(TimestampVersionTest, UpdateIncrementsVersion) {
	TimestampedUser user;
	user.init_record_tracking(true);
	user.set("name", "Charlie");
	user.save();

	// Reload and update
	auto loaded = query<TimestampedUser>().where("name = ?", "Charlie").first_or_throw();
	EXPECT_EQ(1, loaded.get("version").as_integer());

	loaded.set("email", "charlie@example.com");
	loaded.save();

	auto reloaded = query<TimestampedUser>().where("name = ?", "Charlie").first_or_throw();
	EXPECT_EQ(2, reloaded.get("version").as_integer());
	EXPECT_EQ("charlie@example.com", reloaded.get("email").as_string());
}

TEST_P(TimestampVersionTest, UpdateRefreshesUpdatedAtButNotCreatedAt) {
	TimestampedUser user;
	user.init_record_tracking(true);
	user.set("name", "Diana");
	user.save();

	auto loaded = query<TimestampedUser>().where("name = ?", "Diana").first_or_throw();
	auto original_created = loaded.get("created_at").as_date().timestamp();

	// Update the record
	loaded.set("email", "diana@example.com");
	loaded.save();

	auto reloaded = query<TimestampedUser>().where("name = ?", "Diana").first_or_throw();

	// created_at should be unchanged
	EXPECT_EQ(original_created, reloaded.get("created_at").as_date().timestamp());
	// updated_at should still be valid
	EXPECT_FALSE(reloaded.get("updated_at").is_null());
}

TEST_P(TimestampVersionTest, VersionInWhereClauseEnsuresOptimisticLocking) {
	TimestampedUser user;
	user.init_record_tracking(true);
	user.set("name", "Eve");
	user.save();

	// Load two copies of the same record
	auto copy1 = query<TimestampedUser>().where("name = ?", "Eve").first_or_throw();
	auto copy2 = query<TimestampedUser>().where("name = ?", "Eve").first_or_throw();

	EXPECT_EQ(1, copy1.get("version").as_integer());
	EXPECT_EQ(1, copy2.get("version").as_integer());

	// First save succeeds (WHERE version = 1 matches)
	copy1.set("email", "eve1@example.com");
	copy1.save();

	// Second save fails with stale_record (WHERE version = 1 no longer matches, row has version 2)
	copy2.set("email", "eve2@example.com");
	EXPECT_THROW(copy2.save(), ex::stale_record);

	// DB should have copy1's changes, not copy2's
	auto final_record = query<TimestampedUser>().where("name = ?", "Eve").first_or_throw();
	EXPECT_EQ("eve1@example.com", final_record.get("email").as_string());
	EXPECT_EQ(2, final_record.get("version").as_integer());
}

TEST_P(TimestampVersionTest, StaleRecordRestoresInMemoryVersion) {
	TimestampedUser user;
	user.init_record_tracking(true);
	user.set("name", "Grace");
	user.save();

	auto copy1 = query<TimestampedUser>().where("name = ?", "Grace").first_or_throw();
	auto copy2 = query<TimestampedUser>().where("name = ?", "Grace").first_or_throw();

	copy1.set("email", "grace1@example.com");
	copy1.save();

	copy2.set("email", "grace2@example.com");
	EXPECT_THROW(copy2.save(), ex::stale_record);

	// Version should be restored after failed optimistic-lock update.
	EXPECT_EQ(1, copy2.get("version").as_integer());
	EXPECT_TRUE(copy2.attribute_is_dirty("version"));
}

TEST_P(TimestampVersionTest, DestroyWithStaleVersionThrows) {
	TimestampedUser user;
	user.init_record_tracking(true);
	user.set("name", "Heidi");
	user.save();

	auto copy1 = query<TimestampedUser>().where("name = ?", "Heidi").first_or_throw();
	auto copy2 = query<TimestampedUser>().where("name = ?", "Heidi").first_or_throw();

	copy1.set("email", "heidi1@example.com");
	copy1.save();

	EXPECT_THROW(copy2.destroy(), ex::stale_record);

	auto still_there = query<TimestampedUser>().where("name = ?", "Heidi").first();
	EXPECT_TRUE(still_there.has_value());
}

TEST_P(TimestampVersionTest, MultipleUpdatesIncrementVersionSequentially) {
	TimestampedUser user;
	user.init_record_tracking(true);
	user.set("name", "Frank");
	user.save();

	for (int i = 0; i < 5; ++i) {
		auto loaded = query<TimestampedUser>().where("name = ?", "Frank").first_or_throw();
		loaded.set("email", "frank" + std::to_string(i) + "@example.com");
		loaded.save();
	}

	auto final_record = query<TimestampedUser>().where("name = ?", "Frank").first_or_throw();
	EXPECT_EQ(6, final_record.get("version").as_integer()); // 1 (insert) + 5 (updates)
	EXPECT_EQ("frank4@example.com", final_record.get("email").as_string());
}

TEST_P(TimestampVersionTest, NoTimestampColumnsDoesNotCrash) {
	PlainItem item;
	item.set("name", "Widget");

	EXPECT_NO_THROW(item.save());

	auto loaded = query<PlainItem>().where("name = ?", "Widget").first_or_throw();
	EXPECT_EQ("Widget", loaded.get("name").as_string());

	// Update also works fine
	loaded.set("name", "Updated Widget");
	EXPECT_NO_THROW(loaded.save());

	auto reloaded = query<PlainItem>().where("name = ?", "Updated Widget").first_or_throw();
	EXPECT_EQ("Updated Widget", reloaded.get("name").as_string());
}

TEST_P(TimestampVersionTest, MetadataTrackingOffWorksWithPartialTrackingColumns) {
	// partial_items (created_at only, no updated_at or version) is created by the fixture

	class PartialItem : public model {
	  public:
		static const char* st_table_name() { return "partial_items"; }
		static const char* st_primary_key() { return "id"; }
		PartialItem() : model(st_table_name(), st_primary_key()) {}
	};

	PartialItem item;
	item.init_record_tracking(false);
	item.set("name", "Partial");
	item.save();

	auto loaded = query<PartialItem>().where("name = ?", "Partial").first_or_throw();
	EXPECT_TRUE(loaded.get("created_at").is_null());

	// Update should work fine without updated_at or version columns
	loaded.set("name", "Updated Partial");
	EXPECT_NO_THROW(loaded.save());

	auto reloaded = query<PartialItem>().where("name = ?", "Updated Partial").first_or_throw();
	EXPECT_EQ("Updated Partial", reloaded.get("name").as_string());
}

TEST_P(TimestampVersionTest, NullVersionPlaceholderDoesNotLock) {
	auto conn = config::instance().get_connection();
	conn->query("INSERT INTO timestamped_users (name) VALUES (?)", { "NoVersion" });

	auto user = query<TimestampedUser>().where("name = ?", "NoVersion").first_or_throw();
	ASSERT_TRUE(user.get("version").is_null());
	user.set("email", "nv@example.com");
	EXPECT_TRUE(user.save());
	EXPECT_TRUE(user.get("version").is_null());

	auto reloaded = query<TimestampedUser>().where("name = ?", "NoVersion").first_or_throw();
	EXPECT_EQ("nv@example.com", reloaded.get("email").as_string());
	EXPECT_TRUE(user.destroy());
}

// ==============================================================================
// Record identity and UPDATE/DELETE bounds
// ==============================================================================

namespace {

class Account : public model {
  public:
	static const char* st_table_name() { return "accounts"; }
	static const char* st_primary_key() { return "uid"; }

	Account() : model(st_table_name(), st_primary_key()) {}
};

struct TypedAccount : typed_model<TypedAccount> {
	static constexpr auto table_name{ "accounts" };
	static constexpr auto primary_key{ "uid" };

	long uid{};
	std::string name{};

	static constexpr auto columns() {
		return ar::columns(ar::col<&TypedAccount::uid>("uid"), ar::col<&TypedAccount::name>("name"));
	}
};

// Tables whose primary key has the name of an automatically managed metadata column
class PkCreatedAt : public model {
  public:
	static const char* st_table_name() { return "pk_created_at"; }
	static const char* st_primary_key() { return "created_at"; }

	PkCreatedAt() : model(st_table_name(), st_primary_key()) {}
};

class PkUpdatedAt : public model {
  public:
	static const char* st_table_name() { return "pk_updated_at"; }
	static const char* st_primary_key() { return "updated_at"; }

	PkUpdatedAt() : model(st_table_name(), st_primary_key()) {}
};

class PkVersion : public model {
  public:
	static const char* st_table_name() { return "pk_version"; }
	static const char* st_primary_key() { return "version"; }

	PkVersion() : model(st_table_name(), st_primary_key()) {}
};

} // namespace

class IdentityAndBoundsTest : public AdapterTest {
  protected:
	table_list table_definitions(AdapterType type) const override {
		const std::string text{ text_type(type) };
		const std::string ts{ timestamp_type(type) };
		return {
			{ "users", users_columns(type) },
			{ "accounts", std::format("uid INT PRIMARY KEY, name {} NOT NULL", text) },
			{ "pk_created_at",
			  std::format("created_at {} PRIMARY KEY, name {} NOT NULL, updated_at {}, version INTEGER", ts, text,
						  ts) },
			{ "pk_updated_at",
			  std::format("updated_at {} PRIMARY KEY, name {} NOT NULL, created_at {}, version INTEGER", ts, text,
						  ts) },
			{ "pk_version", std::format("version INTEGER PRIMARY KEY, name {} NOT NULL, created_at {}, updated_at {}",
										text, ts, ts) },
		};
	}

	static void insert_users() {
		auto conn = config::instance().get_connection();
		conn->query("INSERT INTO users (name, age) VALUES (?, ?)", { "U1", 20 });
		conn->query("INSERT INTO users (name, age) VALUES (?, ?)", { "U2", 30 });
		conn->query("INSERT INTO users (name, age) VALUES (?, ?)", { "U3", 40 });
		conn->query("INSERT INTO users (name, age) VALUES (?, ?)", { "U4", 50 });
	}

	static std::vector<std::string> names(const std::vector<User>& users) {
		std::vector<std::string> result;
		result.reserve(users.size());
		for (const auto& user : users) {
			result.push_back(user.get("name").as_string());
		}
		return result;
	}

	static std::string account_name(int uid) {
		return query<Account>().where("uid = ?", uid).first_or_throw().get("name").as_string();
	}
};

TEST_P(IdentityAndBoundsTest, CustomPrimaryKeyAliasCannotOverwriteAnotherRow) {
	auto conn = config::instance().get_connection();
	conn->query("INSERT INTO accounts (uid, name) VALUES (?, ?)", { 1, "A" });
	conn->query("INSERT INTO accounts (uid, name) VALUES (?, ?)", { 2, "B" });

	auto account = query<Account>().where("uid = ?", 1).first_or_throw();
	EXPECT_THROW(account.set("id", 2), ex::readonly);
	account.set("name", "overwritten");
	EXPECT_TRUE(account.save());

	EXPECT_EQ(1, account.get("uid").as_integer());
	EXPECT_EQ("overwritten", account_name(1));
	EXPECT_EQ("B", account_name(2));
}

TEST_P(IdentityAndBoundsTest, TypedRecordWithoutLoadedPrimaryKeyCannotBeSaved) {
	auto conn = config::instance().get_connection();
	conn->query("INSERT INTO accounts (uid, name) VALUES (?, ?)", { 1, "A" });
	conn->query("INSERT INTO accounts (uid, name) VALUES (?, ?)", { 2, "B" });

	auto account = query<TypedAccount>().select("name").where("uid = ?", 1).first_or_throw();
	account.uid = 2;
	account.name = "overwritten";
	EXPECT_THROW(account.save(), ex::readonly);

	EXPECT_EQ("A", account_name(1));
	EXPECT_EQ("B", account_name(2));
}

TEST_P(IdentityAndBoundsTest, CreatedAtPrimaryKeyIsNotManaged) {
	auto conn = config::instance().get_connection();
	conn->query("INSERT INTO pk_created_at (created_at, name, version) VALUES (?, ?, ?)", { 100, "A", 1 });
	conn->query("INSERT INTO pk_created_at (created_at, name, version) VALUES (?, ?, ?)", { 200, "B", 1 });

	auto row = query<PkCreatedAt>().where("created_at = ?", 100).first_or_throw();
	row.set("name", "Changed");
	EXPECT_TRUE(row.save());
	EXPECT_EQ(100, row.get("created_at").as_date().timestamp());
	EXPECT_EQ(2, row.get("version").as_integer()); // ordinary metadata is still managed

	auto other = query<PkCreatedAt>().where("created_at = ?", 200).first_or_throw();
	EXPECT_EQ("B", other.get("name").as_string());
	EXPECT_EQ(1, other.get("version").as_integer());

	EXPECT_TRUE(row.destroy());
	EXPECT_EQ(1, query<PkCreatedAt>().count());
}

TEST_P(IdentityAndBoundsTest, UpdatedAtPrimaryKeyIsNotManaged) {
	// The second row is keyed by the current time: an update that set updated_at = now would hit it
	const std::time_t now{ datetime::now().timestamp() };
	auto conn = config::instance().get_connection();
	conn->query("INSERT INTO pk_updated_at (updated_at, name, version) VALUES (?, ?, ?)", { 100, "A", 1 });
	conn->query("INSERT INTO pk_updated_at (updated_at, name, version) VALUES (?, ?, ?)",
				{ static_cast<int64_t>(now), "B", 1 });

	auto row = query<PkUpdatedAt>().where("updated_at = ?", 100).first_or_throw();
	row.set("name", "Changed");
	EXPECT_TRUE(row.save());
	EXPECT_EQ(100, row.get("updated_at").as_date().timestamp());

	auto changed = query<PkUpdatedAt>().where("updated_at = ?", 100).first_or_throw();
	EXPECT_EQ("Changed", changed.get("name").as_string());
	auto other = query<PkUpdatedAt>().where("name = ?", "B").first_or_throw();
	EXPECT_EQ(now, other.get("updated_at").as_date().timestamp());
	EXPECT_EQ(1, other.get("version").as_integer());

	EXPECT_TRUE(row.destroy());
	EXPECT_EQ(1, query<PkUpdatedAt>().count());
	EXPECT_TRUE(query<PkUpdatedAt>().where("name = ?", "B").exists());
}

TEST_P(IdentityAndBoundsTest, VersionPrimaryKeyIsNotManaged) {
	auto conn = config::instance().get_connection();
	conn->query("INSERT INTO pk_version (version, name) VALUES (?, ?)", { 1, "A" });
	conn->query("INSERT INTO pk_version (version, name) VALUES (?, ?)", { 2, "B" });

	auto row = query<PkVersion>().where("version = ?", 1).first_or_throw();
	row.set("name", "Changed");
	EXPECT_TRUE(row.save());
	EXPECT_EQ(1, row.get("version").as_integer());

	EXPECT_EQ("Changed", query<PkVersion>().where("version = ?", 1).first_or_throw().get("name").as_string());
	EXPECT_EQ("B", query<PkVersion>().where("version = ?", 2).first_or_throw().get("name").as_string());

	EXPECT_TRUE(row.destroy());
	EXPECT_EQ(1, query<PkVersion>().count());
	EXPECT_TRUE(query<PkVersion>().where("version = ?", 2).exists());
}

TEST_P(IdentityAndBoundsTest, OrderedDeleteAllDeletesOnlyMatchingRows) {
	insert_users();

	EXPECT_EQ(3, query<User>().where("age >= ?", 30).order("age DESC").delete_all());
	EXPECT_EQ(std::vector<std::string>{ "U1" }, names(query<User>().all()));
}

TEST_P(IdentityAndBoundsTest, DeleteAllLimitAppliesOnMysqlOnly) {
	insert_users();

	if (adapter_type() == AdapterType::MySQL) {
		EXPECT_EQ(0, query<User>().limit(0).delete_all());
		EXPECT_EQ(1, query<User>().order("age DESC").limit(1).delete_all());
		EXPECT_EQ((std::vector<std::string>{ "U1", "U2", "U3" }), names(query<User>().order("id").all()));
	} else {
		EXPECT_THROW(query<User>().limit(1).delete_all(), ex::activerecord);
		EXPECT_THROW(query<User>().limit(0).delete_all(), ex::activerecord);
		EXPECT_EQ(4, query<User>().count());
	}
}

TEST_P(IdentityAndBoundsTest, DeleteAllOffsetThrowsOnEveryAdapter) {
	insert_users();

	EXPECT_THROW(query<User>().offset(1).delete_all(), ex::activerecord);
	EXPECT_THROW(query<User>().offset(0).delete_all(), ex::activerecord);
	EXPECT_THROW(query<User>().order("age").limit(1).offset(1).delete_all(), ex::activerecord);
	EXPECT_EQ(4, query<User>().count());
}

TEST_P(IdentityAndBoundsTest, GroupedCountAndExistsThrow) {
	insert_users();

	EXPECT_THROW(query<User>().group("active").count(), ex::activerecord);
	EXPECT_THROW(query<User>().group("active").exists(), ex::activerecord);
}

TEST_P(IdentityAndBoundsTest, OffsetWithoutLimitSkipsRows) {
	insert_users();

	EXPECT_EQ((std::vector<std::string>{ "U3", "U4" }), names(query<User>().order("id").offset(2).all()));
	EXPECT_EQ((std::vector<std::string>{ "U1", "U2", "U3", "U4" }), names(query<User>().order("id").offset(0).all()));
	EXPECT_TRUE(query<User>().order("id").offset(10).all().empty());
}

// ==============================================================================
// Test Instantiation
// ==============================================================================

INSTANTIATE_TEST_SUITE_P(AllAdapters, DatabaseOperationsTest, ::testing::ValuesIn(enabled_adapters()),
						 adapter_param_name);
INSTANTIATE_TEST_SUITE_P(AllAdapters, TimestampVersionTest, ::testing::ValuesIn(enabled_adapters()),
						 adapter_param_name);
INSTANTIATE_TEST_SUITE_P(AllAdapters, IdentityAndBoundsTest, ::testing::ValuesIn(enabled_adapters()),
						 adapter_param_name);
