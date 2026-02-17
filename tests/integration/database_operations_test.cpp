// ==============================================================================
// database_operations_test.cpp
//
// SPDX-FileCopyrightText: 2020-2023 Indridi Bjornsson
// SPDX-License-Identifier: MIT
// See file LICENSE.MIT for detail
//
// End-to-end integration tests with model + adapter + query builder
// ==============================================================================

#include <activerecord.h>
#include <adapters/sqlite_adapter.h>
#include <config.h>
#include <datetime.h>
#include <gtest/gtest.h>
#include <model.h>
#include <sql_builder.h>

using namespace ar;

// ==============================================================================
// Test Model
// ==============================================================================

class User : public model {
  public:
	static const char* st_table_name() { return "users"; }
	static const char* st_primary_key() { return "id"; }

	User() : model(st_table_name(), st_primary_key()) {}
};

// ==============================================================================
// Test Fixture
// ==============================================================================

class DatabaseOperationsTest : public ::testing::Test {
  protected:
	void SetUp() override {
		// Create in-memory database
		auto adapter = std::make_shared<sqlite_adapter>(":memory:");

		// Configure global connection
		config::connection_map connections;
		connections["default"] = adapter;
		config::instance().set_connections(connections, "default");

		// Create table
		adapter->query("CREATE TABLE users ("
					   "id INTEGER PRIMARY KEY AUTOINCREMENT, "
					   "name TEXT NOT NULL, "
					   "email TEXT, "
					   "age INTEGER, "
					   "active BOOLEAN DEFAULT 1"
					   ")",
					   {});
	}

	void TearDown() override {
		// Clean up
	}
};

// ==============================================================================
// Model CRUD Tests
// ==============================================================================

TEST_F(DatabaseOperationsTest, CreateAndSaveUser) {
	User user;
	user.set("name", attribute("John Doe"));
	user.set("email", attribute("john@example.com"));
	user.set("age", attribute(30));

	EXPECT_TRUE(user.is_new_record());
	EXPECT_TRUE(user.is_dirty());

	bool saved = user.save();
	EXPECT_TRUE(saved);
}

TEST_F(DatabaseOperationsTest, UpdateUser) {
	// First, insert a user manually
	auto conn = config::instance().get_connection();
	conn->query("INSERT INTO users (name, email, age) VALUES (?, ?, ?)",
				{ attribute("Jane"), attribute("jane@example.com"), attribute(25) });

	// Create model with attributes (simulating a loaded model)
	User user;
	user.attributes_["id"] = attribute(1);
	user.attributes_["name"] = attribute("Jane");
	user.attributes_["email"] = attribute("jane@example.com");
	user.attributes_["age"] = attribute(25);
	user.new_record_ = false;

	// Update
	user.set("age", attribute(26));
	EXPECT_TRUE(user.is_dirty());
	EXPECT_TRUE(user.attribute_is_dirty("age"));

	bool updated = user.save();
	EXPECT_TRUE(updated);
	EXPECT_FALSE(user.is_dirty());
}

TEST_F(DatabaseOperationsTest, DestroyDeletesPersistedRecord) {
	auto conn = config::instance().get_connection();
	conn->query("INSERT INTO users (name, email, age) VALUES (?, ?, ?)",
				{ attribute("DeleteMe"), attribute("delete@example.com"), attribute(22) });

	auto user = query<User>().where("name = ?", attribute("DeleteMe")).first_or_throw();
	EXPECT_FALSE(user.is_new_record());

	EXPECT_TRUE(user.destroy());
	EXPECT_TRUE(user.is_new_record());

	auto deleted = query<User>().where("name = ?", attribute("DeleteMe")).first();
	EXPECT_FALSE(deleted.has_value());
}

TEST_F(DatabaseOperationsTest, SetMultipleAttributes) {
	User user;
	attribute_map attrs;
	attrs["name"] = attribute("Alice");
	attrs["email"] = attribute("alice@example.com");
	attrs["age"] = attribute(28);

	user.set_attributes(attrs);

	EXPECT_EQ("Alice", user.get("name").as_string());
	EXPECT_EQ("alice@example.com", user.get("email").as_string());
	EXPECT_EQ(28, user.get("age").as_integer());
}

// ==============================================================================
// Query Builder Tests
// ==============================================================================

TEST_F(DatabaseOperationsTest, QueryAll) {
	// Insert test data
	auto conn = config::instance().get_connection();
	conn->query("INSERT INTO users (name, age) VALUES (?, ?)", { attribute("User1"), attribute(20) });
	conn->query("INSERT INTO users (name, age) VALUES (?, ?)", { attribute("User2"), attribute(30) });
	conn->query("INSERT INTO users (name, age) VALUES (?, ?)", { attribute("User3"), attribute(40) });

	// Query all
	query<User> q;
	std::vector<User> users = q.all();

	EXPECT_EQ(3, users.size());
	EXPECT_FALSE(users[0].is_new_record());
}

TEST_F(DatabaseOperationsTest, QueryWithWhere) {
	// Insert test data
	auto conn = config::instance().get_connection();
	conn->query("INSERT INTO users (name, age) VALUES (?, ?)", { attribute("Young"), attribute(20) });
	conn->query("INSERT INTO users (name, age) VALUES (?, ?)", { attribute("Old"), attribute(60) });

	// Query with WHERE
	query<User> q;
	q.where("age > ?", attribute(30));
	std::vector<User> users = q.all();

	EXPECT_EQ(1, users.size());
	EXPECT_EQ("Old", users[0].get("name").as_string());
}

TEST_F(DatabaseOperationsTest, QueryWithOrder) {
	// Insert test data in random order
	auto conn = config::instance().get_connection();
	conn->query("INSERT INTO users (name, age) VALUES (?, ?)", { attribute("C"), attribute(30) });
	conn->query("INSERT INTO users (name, age) VALUES (?, ?)", { attribute("A"), attribute(20) });
	conn->query("INSERT INTO users (name, age) VALUES (?, ?)", { attribute("B"), attribute(25) });

	// Query with ORDER BY
	query<User> q;
	q.order("name ASC");
	std::vector<User> users = q.all();

	EXPECT_EQ(3, users.size());
	EXPECT_EQ("A", users[0].get("name").as_string());
	EXPECT_EQ("B", users[1].get("name").as_string());
	EXPECT_EQ("C", users[2].get("name").as_string());
}

TEST_F(DatabaseOperationsTest, QueryWithLimit) {
	// Insert test data
	auto conn = config::instance().get_connection();
	for (int i = 0; i < 10; i++) {
		conn->query("INSERT INTO users (name, age) VALUES (?, ?)",
					{ attribute("User" + std::to_string(i)), attribute(20 + i) });
	}

	// Query with LIMIT
	query<User> q;
	q.limit(3);
	std::vector<User> users = q.all();

	EXPECT_EQ(3, users.size());
}

TEST_F(DatabaseOperationsTest, QueryWithLimitAndOffset) {
	// Insert test data
	auto conn = config::instance().get_connection();
	for (int i = 0; i < 10; i++) {
		conn->query("INSERT INTO users (name, age) VALUES (?, ?)",
					{ attribute("User" + std::to_string(i)), attribute(20 + i) });
	}

	// Query with LIMIT and OFFSET
	query<User> q;
	q.limit(2).offset(3);
	std::vector<User> users = q.all();

	EXPECT_EQ(2, users.size());
	// Should get users at index 3 and 4
}

TEST_F(DatabaseOperationsTest, QueryCount) {
	// Insert test data
	auto conn = config::instance().get_connection();
	for (int i = 0; i < 7; i++) {
		conn->query("INSERT INTO users (name, age) VALUES (?, ?)",
					{ attribute("User" + std::to_string(i)), attribute(20 + i) });
	}

	// Count all
	query<User> q;
	long count = q.count();

	EXPECT_EQ(7, count);
}

TEST_F(DatabaseOperationsTest, QueryCountWithWhere) {
	// Insert test data
	auto conn = config::instance().get_connection();
	conn->query("INSERT INTO users (name, age) VALUES (?, ?)", { attribute("Young1"), attribute(20) });
	conn->query("INSERT INTO users (name, age) VALUES (?, ?)", { attribute("Young2"), attribute(22) });
	conn->query("INSERT INTO users (name, age) VALUES (?, ?)", { attribute("Old"), attribute(60) });

	// Count with WHERE
	query<User> q;
	q.where("age < ?", attribute(30));
	long count = q.count();

	EXPECT_EQ(2, count);
}

TEST_F(DatabaseOperationsTest, QueryCountIgnoresLimitAndOffset) {
	auto conn = config::instance().get_connection();
	for (int i = 0; i < 7; i++) {
		conn->query("INSERT INTO users (name, age) VALUES (?, ?)",
					{ attribute("User" + std::to_string(i)), attribute(20 + i) });
	}

	query<User> q;
	q.limit(2).offset(3);
	EXPECT_EQ(7, q.count());
}

TEST_F(DatabaseOperationsTest, QueryExists) {
	// Insert test data
	auto conn = config::instance().get_connection();
	conn->query("INSERT INTO users (name, age) VALUES (?, ?)", { attribute("Test"), attribute(25) });

	// Check exists
	query<User> q1;
	q1.where("name = ?", attribute("Test"));
	EXPECT_TRUE(q1.exists());

	query<User> q2;
	q2.where("name = ?", attribute("NonExistent"));
	EXPECT_FALSE(q2.exists());
}

TEST_F(DatabaseOperationsTest, QueryDeleteAllWithWhere) {
	auto conn{ config::instance().get_connection() };
	conn->query("INSERT INTO users (name, age) VALUES (?, ?)", { attribute("A"), attribute(20) });
	conn->query("INSERT INTO users (name, age) VALUES (?, ?)", { attribute("B"), attribute(35) });
	conn->query("INSERT INTO users (name, age) VALUES (?, ?)", { attribute("C"), attribute(40) });

	query<User> q;
	const long deleted{ q.where("age >= ?", attribute(30)).delete_all() };
	EXPECT_EQ(2, deleted);

	query<User> remaining;
	EXPECT_EQ(1, remaining.count());
	auto first{ remaining.first_or_throw() };
	EXPECT_EQ("A", first.get("name").as_string());
}

TEST_F(DatabaseOperationsTest, QueryFind) {
	// Insert test data
	auto conn = config::instance().get_connection();
	conn->query("INSERT INTO users (name, age) VALUES (?, ?)", { attribute("FindMe"), attribute(25) });

	// Get the ID (would be 1 for first insert)
	auto stmt = conn->query("SELECT id FROM users WHERE name = ?", { attribute("FindMe") });
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

TEST_F(DatabaseOperationsTest, QueryFindMultipleIds) {
	auto conn = config::instance().get_connection();
	conn->query("INSERT INTO users (name, age) VALUES (?, ?)", { attribute("FindA"), attribute(21) });
	conn->query("INSERT INTO users (name, age) VALUES (?, ?)", { attribute("FindB"), attribute(22) });
	conn->query("INSERT INTO users (name, age) VALUES (?, ?)", { attribute("FindC"), attribute(23) });

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

TEST_F(DatabaseOperationsTest, QueryFindMultipleIdsEmptyInputReturnsEmpty) {
	query<User> q;
	auto users = q.find(std::vector<long>{});
	EXPECT_TRUE(users.empty());
}

TEST_F(DatabaseOperationsTest, QueryFirst) {
	// Insert test data
	auto conn = config::instance().get_connection();
	conn->query("INSERT INTO users (name, age) VALUES (?, ?)", { attribute("First"), attribute(10) });
	conn->query("INSERT INTO users (name, age) VALUES (?, ?)", { attribute("Second"), attribute(20) });

	// Get first
	query<User> q;
	q.order("id ASC");
	auto first = q.first();
	ASSERT_TRUE(first.has_value());

	EXPECT_EQ("First", first->get("name").as_string());
}

TEST_F(DatabaseOperationsTest, QueryFirstReturnsNulloptWhenEmpty) {
	query<User> q;
	EXPECT_FALSE(q.first().has_value());
}

TEST_F(DatabaseOperationsTest, QueryLastReturnsNulloptWhenEmpty) {
	query<User> q;
	EXPECT_FALSE(q.last().has_value());
}

TEST_F(DatabaseOperationsTest, QueryFirstOrThrowThrowsWhenEmpty) {
	query<User> q;
	EXPECT_THROW(q.first_or_throw(), ex::item_not_found);
}

TEST_F(DatabaseOperationsTest, QueryLastOrThrowThrowsWhenEmpty) {
	query<User> q;
	EXPECT_THROW(q.last_or_throw(), ex::item_not_found);
}

TEST_F(DatabaseOperationsTest, QueryLastUsesPrimaryKeyDescWhenNoOrder) {
	auto conn = config::instance().get_connection();
	conn->query("INSERT INTO users (name, age) VALUES (?, ?)", { attribute("First"), attribute(10) });
	conn->query("INSERT INTO users (name, age) VALUES (?, ?)", { attribute("Last"), attribute(20) });

	query<User> q;
	auto last = q.last();
	ASSERT_TRUE(last.has_value());
	EXPECT_EQ("Last", last->get("name").as_string());
}

TEST_F(DatabaseOperationsTest, QueryLastRespectsReversedOrder) {
	auto conn = config::instance().get_connection();
	conn->query("INSERT INTO users (name, age) VALUES (?, ?)", { attribute("A"), attribute(10) });
	conn->query("INSERT INTO users (name, age) VALUES (?, ?)", { attribute("B"), attribute(20) });
	conn->query("INSERT INTO users (name, age) VALUES (?, ?)", { attribute("C"), attribute(30) });

	query<User> q;
	q.order("age ASC");
	auto last = q.last();
	ASSERT_TRUE(last.has_value());
	EXPECT_EQ("C", last->get("name").as_string());
}

TEST_F(DatabaseOperationsTest, QueryFirstOrThrowAndLastOrThrowReturnRecords) {
	auto conn = config::instance().get_connection();
	conn->query("INSERT INTO users (name, age) VALUES (?, ?)", { attribute("A"), attribute(10) });
	conn->query("INSERT INTO users (name, age) VALUES (?, ?)", { attribute("B"), attribute(20) });

	query<User> first_query;
	first_query.order("id ASC");
	User first = first_query.first_or_throw();
	EXPECT_EQ("A", first.get("name").as_string());

	query<User> last_query;
	last_query.order("id ASC");
	User last = last_query.last_or_throw();
	EXPECT_EQ("B", last.get("name").as_string());
}

TEST_F(DatabaseOperationsTest, QuerySupportsRangeBasedIteration) {
	auto conn = config::instance().get_connection();
	conn->query("INSERT INTO users (name, age) VALUES (?, ?)", { attribute("A"), attribute(10) });
	conn->query("INSERT INTO users (name, age) VALUES (?, ?)", { attribute("B"), attribute(20) });
	conn->query("INSERT INTO users (name, age) VALUES (?, ?)", { attribute("C"), attribute(30) });

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

TEST_F(DatabaseOperationsTest, ComplexQuery) {
	// Insert test data
	auto conn = config::instance().get_connection();
	for (int i = 0; i < 20; i++) {
		conn->query(
			"INSERT INTO users (name, age, active) VALUES (?, ?, ?)",
			{ attribute("User" + std::to_string(i)), attribute(20 + i), attribute(i % 2 == 0) }); // Even IDs are active
	}

	// Complex query: active users, age > 25, ordered by age desc, limit 5
	query<User> q;
	q.where("active = ? AND age > ?", { attribute(true), attribute(25) }).order("age DESC").limit(5);

	std::vector<User> users = q.all();

	EXPECT_EQ(5, users.size());
	// First user should have highest age among active users > 25
}

// ==============================================================================
// Model Serialization Tests
// ==============================================================================

TEST_F(DatabaseOperationsTest, ModelToJson) {
	// Insert test data
	auto conn = config::instance().get_connection();
	conn->query("INSERT INTO users (name, email, age) VALUES (?, ?, ?)",
				{ attribute("John"), attribute("john@example.com"), attribute(30) });

	// Load into model
	query<User> q;
	auto user = q.first();
	ASSERT_TRUE(user.has_value());

	std::string json = user->to_json_string();

	EXPECT_NE(std::string::npos, json.find("\"name\""));
	EXPECT_NE(std::string::npos, json.find("\"John\""));
	EXPECT_NE(std::string::npos, json.find("\"email\""));
}

TEST_F(DatabaseOperationsTest, ModelToXml) {
	// Insert test data
	auto conn = config::instance().get_connection();
	conn->query("INSERT INTO users (name, email) VALUES (?, ?)", { attribute("Jane"), attribute("jane@example.com") });

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

TEST_F(DatabaseOperationsTest, DirtyTrackingAfterLoad) {
	// Insert test data
	auto conn = config::instance().get_connection();
	conn->query("INSERT INTO users (name, age) VALUES (?, ?)", { attribute("Test"), attribute(25) });

	// Load model
	query<User> q;
	auto user = q.first();
	ASSERT_TRUE(user.has_value());

	// Should not be dirty after load
	EXPECT_FALSE(user->is_dirty());

	// Modify
	user->set("age", attribute(26));
	EXPECT_TRUE(user->is_dirty());
	EXPECT_TRUE(user->attribute_is_dirty("age"));

	// Save
	user->save();
	EXPECT_FALSE(user->is_dirty());
}

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

class TimestampVersionTest : public ::testing::Test {
  protected:
	void SetUp() override {
		auto adapter = std::make_shared<sqlite_adapter>(":memory:");

		config::connection_map connections;
		connections["default"] = adapter;
		config::instance().set_connections(connections, "default");

		// Table with timestamp and version columns
		adapter->query("CREATE TABLE timestamped_users ("
					   "id INTEGER PRIMARY KEY AUTOINCREMENT, "
					   "name TEXT NOT NULL, "
					   "email TEXT, "
					   "created_at DATETIME, "
					   "updated_at DATETIME, "
					   "version INTEGER"
					   ")",
					   {});

		// Table without timestamp/version columns
		adapter->query("CREATE TABLE plain_items ("
					   "id INTEGER PRIMARY KEY AUTOINCREMENT, "
					   "name TEXT NOT NULL"
					   ")",
					   {});
	}
};

// ==============================================================================
// Timestamp & Version Integration Tests
// ==============================================================================

TEST_F(TimestampVersionTest, InsertSetsCreatedAtAndUpdatedAt) {
	TimestampedUser user;
	user.init_record_tracking(true, true, true);
	user.set("name", attribute("Alice"));

	datetime before = datetime::now();
	user.save();

	// Reload from DB
	auto loaded = query<TimestampedUser>().where("name = ?", attribute("Alice")).first_or_throw();

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

TEST_F(TimestampVersionTest, InsertSetsVersionToOne) {
	TimestampedUser user;
	user.init_record_tracking(true, true, true);
	user.set("name", attribute("Bob"));
	user.save();

	auto loaded = query<TimestampedUser>().where("name = ?", attribute("Bob")).first_or_throw();
	EXPECT_EQ(1, loaded.get("version").as_integer());
}

TEST_F(TimestampVersionTest, UpdateIncrementsVersion) {
	TimestampedUser user;
	user.init_record_tracking(true, true, true);
	user.set("name", attribute("Charlie"));
	user.save();

	// Reload and update
	auto loaded = query<TimestampedUser>().where("name = ?", attribute("Charlie")).first_or_throw();
	EXPECT_EQ(1, loaded.get("version").as_integer());

	loaded.set("email", attribute("charlie@example.com"));
	loaded.save();

	auto reloaded = query<TimestampedUser>().where("name = ?", attribute("Charlie")).first_or_throw();
	EXPECT_EQ(2, reloaded.get("version").as_integer());
	EXPECT_EQ("charlie@example.com", reloaded.get("email").as_string());
}

TEST_F(TimestampVersionTest, UpdateRefreshesUpdatedAtButNotCreatedAt) {
	TimestampedUser user;
	user.init_record_tracking(true, true, true);
	user.set("name", attribute("Diana"));
	user.save();

	auto loaded = query<TimestampedUser>().where("name = ?", attribute("Diana")).first_or_throw();
	auto original_created = loaded.get("created_at").as_date().timestamp();

	// Update the record
	loaded.set("email", attribute("diana@example.com"));
	loaded.save();

	auto reloaded = query<TimestampedUser>().where("name = ?", attribute("Diana")).first_or_throw();

	// created_at should be unchanged
	EXPECT_EQ(original_created, reloaded.get("created_at").as_date().timestamp());
	// updated_at should still be valid
	EXPECT_FALSE(reloaded.get("updated_at").is_null());
}

TEST_F(TimestampVersionTest, VersionInWhereClauseEnsuresOptimisticLocking) {
	TimestampedUser user;
	user.init_record_tracking(true, true, true);
	user.set("name", attribute("Eve"));
	user.save();

	// Load two copies of the same record
	auto copy1 = query<TimestampedUser>().where("name = ?", attribute("Eve")).first_or_throw();
	auto copy2 = query<TimestampedUser>().where("name = ?", attribute("Eve")).first_or_throw();

	EXPECT_EQ(1, copy1.get("version").as_integer());
	EXPECT_EQ(1, copy2.get("version").as_integer());

	// First save succeeds (WHERE version = 1 matches)
	copy1.set("email", attribute("eve1@example.com"));
	copy1.save();

	// Second save fails with stale_record (WHERE version = 1 no longer matches, row has version 2)
	copy2.set("email", attribute("eve2@example.com"));
	EXPECT_THROW(copy2.save(), ex::stale_record);

	// DB should have copy1's changes, not copy2's
	auto final_record = query<TimestampedUser>().where("name = ?", attribute("Eve")).first_or_throw();
	EXPECT_EQ("eve1@example.com", final_record.get("email").as_string());
	EXPECT_EQ(2, final_record.get("version").as_integer());
}

TEST_F(TimestampVersionTest, StaleRecordRestoresInMemoryVersion) {
	TimestampedUser user;
	user.init_record_tracking(true, true, true);
	user.set("name", attribute("Grace"));
	user.save();

	auto copy1 = query<TimestampedUser>().where("name = ?", attribute("Grace")).first_or_throw();
	auto copy2 = query<TimestampedUser>().where("name = ?", attribute("Grace")).first_or_throw();

	copy1.set("email", attribute("grace1@example.com"));
	copy1.save();

	copy2.set("email", attribute("grace2@example.com"));
	EXPECT_THROW(copy2.save(), ex::stale_record);

	// Version should be restored after failed optimistic-lock update.
	EXPECT_EQ(1, copy2.get("version").as_integer());
	EXPECT_TRUE(copy2.attribute_is_dirty("version"));
}

TEST_F(TimestampVersionTest, DestroyWithStaleVersionThrows) {
	TimestampedUser user;
	user.init_record_tracking(true, true, true);
	user.set("name", attribute("Heidi"));
	user.save();

	auto copy1 = query<TimestampedUser>().where("name = ?", attribute("Heidi")).first_or_throw();
	auto copy2 = query<TimestampedUser>().where("name = ?", attribute("Heidi")).first_or_throw();

	copy1.set("email", attribute("heidi1@example.com"));
	copy1.save();

	EXPECT_THROW(copy2.destroy(), ex::stale_record);

	auto still_there = query<TimestampedUser>().where("name = ?", attribute("Heidi")).first();
	EXPECT_TRUE(still_there.has_value());
}

TEST_F(TimestampVersionTest, MultipleUpdatesIncrementVersionSequentially) {
	TimestampedUser user;
	user.init_record_tracking(true, true, true);
	user.set("name", attribute("Frank"));
	user.save();

	for (int i = 0; i < 5; ++i) {
		auto loaded = query<TimestampedUser>().where("name = ?", attribute("Frank")).first_or_throw();
		loaded.set("email", attribute("frank" + std::to_string(i) + "@example.com"));
		loaded.save();
	}

	auto final_record = query<TimestampedUser>().where("name = ?", attribute("Frank")).first_or_throw();
	EXPECT_EQ(6, final_record.get("version").as_integer()); // 1 (insert) + 5 (updates)
	EXPECT_EQ("frank4@example.com", final_record.get("email").as_string());
}

TEST_F(TimestampVersionTest, NoTimestampColumnsDoesNotCrash) {
	PlainItem item;
	item.set("name", attribute("Widget"));

	EXPECT_NO_THROW(item.save());

	auto loaded = query<PlainItem>().where("name = ?", attribute("Widget")).first_or_throw();
	EXPECT_EQ("Widget", loaded.get("name").as_string());

	// Update also works fine
	loaded.set("name", attribute("Updated Widget"));
	EXPECT_NO_THROW(loaded.save());

	auto reloaded = query<PlainItem>().where("name = ?", attribute("Updated Widget")).first_or_throw();
	EXPECT_EQ("Updated Widget", reloaded.get("name").as_string());
}

TEST_F(TimestampVersionTest, CreatedAtExistsButUpdatedAtDoesNot) {
	// Create a table with only created_at (no updated_at, no version)
	auto conn = config::instance().get_connection();
	conn->query("CREATE TABLE partial_items ("
				"id INTEGER PRIMARY KEY AUTOINCREMENT, "
				"name TEXT NOT NULL, "
				"created_at DATETIME"
				")",
				{});

	class PartialItem : public model {
	  public:
		static const char* st_table_name() { return "partial_items"; }
		static const char* st_primary_key() { return "id"; }
		PartialItem() : model(st_table_name(), st_primary_key()) {}
	};

	PartialItem item;
	item.init_record_tracking(true, false, false);
	item.set("name", attribute("Partial"));
	item.save();

	auto loaded = query<PartialItem>().where("name = ?", attribute("Partial")).first_or_throw();
	EXPECT_FALSE(loaded.get("created_at").is_null());

	// Update should work fine without updated_at or version columns
	loaded.set("name", attribute("Updated Partial"));
	EXPECT_NO_THROW(loaded.save());

	auto reloaded = query<PartialItem>().where("name = ?", attribute("Updated Partial")).first_or_throw();
	EXPECT_EQ("Updated Partial", reloaded.get("name").as_string());
}
