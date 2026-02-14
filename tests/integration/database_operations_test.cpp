// ==============================================================================
// database_operations_test.cpp
//
// SPDX-FileCopyrightText: 2020-2023 Indridi Bjornsson
// SPDX-License-Identifier: MIT
// See file LICENSE.MIT for detail
//
// End-to-end integration tests with model + adapter + query builder
// ==============================================================================

#include <gtest/gtest.h>
#include <adapters/sqlite_adapter.h>
#include <model.h>
#include <config.h>
#include <sql_builder.h>
#include <activerecord.h>

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
                      ")", {});
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
               {attribute("Jane"), attribute("jane@example.com"), attribute(25)});

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
    conn->query("INSERT INTO users (name, age) VALUES (?, ?)",
               {attribute("User1"), attribute(20)});
    conn->query("INSERT INTO users (name, age) VALUES (?, ?)",
               {attribute("User2"), attribute(30)});
    conn->query("INSERT INTO users (name, age) VALUES (?, ?)",
               {attribute("User3"), attribute(40)});

    // Query all
    query<User> q;
    std::vector<User> users = q.all();

    EXPECT_EQ(3, users.size());
    EXPECT_FALSE(users[0].is_new_record());
}

TEST_F(DatabaseOperationsTest, QueryWithWhere) {
    // Insert test data
    auto conn = config::instance().get_connection();
    conn->query("INSERT INTO users (name, age) VALUES (?, ?)",
               {attribute("Young"), attribute(20)});
    conn->query("INSERT INTO users (name, age) VALUES (?, ?)",
               {attribute("Old"), attribute(60)});

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
    conn->query("INSERT INTO users (name, age) VALUES (?, ?)",
               {attribute("C"), attribute(30)});
    conn->query("INSERT INTO users (name, age) VALUES (?, ?)",
               {attribute("A"), attribute(20)});
    conn->query("INSERT INTO users (name, age) VALUES (?, ?)",
               {attribute("B"), attribute(25)});

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
                   {attribute("User" + std::to_string(i)), attribute(20 + i)});
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
                   {attribute("User" + std::to_string(i)), attribute(20 + i)});
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
                   {attribute("User" + std::to_string(i)), attribute(20 + i)});
    }

    // Count all
    query<User> q;
    long count = q.count();

    EXPECT_EQ(7, count);
}

TEST_F(DatabaseOperationsTest, QueryCountWithWhere) {
    // Insert test data
    auto conn = config::instance().get_connection();
    conn->query("INSERT INTO users (name, age) VALUES (?, ?)",
               {attribute("Young1"), attribute(20)});
    conn->query("INSERT INTO users (name, age) VALUES (?, ?)",
               {attribute("Young2"), attribute(22)});
    conn->query("INSERT INTO users (name, age) VALUES (?, ?)",
               {attribute("Old"), attribute(60)});

    // Count with WHERE
    query<User> q;
    q.where("age < ?", attribute(30));
    long count = q.count();

    EXPECT_EQ(2, count);
}

TEST_F(DatabaseOperationsTest, QueryExists) {
    // Insert test data
    auto conn = config::instance().get_connection();
    conn->query("INSERT INTO users (name, age) VALUES (?, ?)",
               {attribute("Test"), attribute(25)});

    // Check exists
    query<User> q1;
    q1.where("name = ?", attribute("Test"));
    EXPECT_TRUE(q1.exists());

    query<User> q2;
    q2.where("name = ?", attribute("NonExistent"));
    EXPECT_FALSE(q2.exists());
}

TEST_F(DatabaseOperationsTest, QueryFind) {
    // Insert test data
    auto conn = config::instance().get_connection();
    conn->query("INSERT INTO users (name, age) VALUES (?, ?)",
               {attribute("FindMe"), attribute(25)});

    // Get the ID (would be 1 for first insert)
    auto stmt = conn->query("SELECT id FROM users WHERE name = ?", {attribute("FindMe")});
    stmt->fetch();
    attribute_map attrs;
    stmt->get_attributes(attrs);
    long user_id = attrs["id"].as_integer();

    // Find by ID
    query<User> q;
    User user = q.find(user_id);

    EXPECT_EQ("FindMe", user.get("name").as_string());
    EXPECT_EQ(25, user.get("age").as_integer());
    EXPECT_FALSE(user.is_new_record());
}

TEST_F(DatabaseOperationsTest, QueryFirst) {
    // Insert test data
    auto conn = config::instance().get_connection();
    conn->query("INSERT INTO users (name, age) VALUES (?, ?)",
               {attribute("First"), attribute(10)});
    conn->query("INSERT INTO users (name, age) VALUES (?, ?)",
               {attribute("Second"), attribute(20)});

    // Get first
    query<User> q;
    q.order("id ASC");
    User first = q.first();

    EXPECT_EQ("First", first.get("name").as_string());
}

// ==============================================================================
// Complex Query Tests
// ==============================================================================

TEST_F(DatabaseOperationsTest, ComplexQuery) {
    // Insert test data
    auto conn = config::instance().get_connection();
    for (int i = 0; i < 20; i++) {
        conn->query("INSERT INTO users (name, age, active) VALUES (?, ?, ?)",
                   {attribute("User" + std::to_string(i)),
                    attribute(20 + i),
                    attribute(i % 2 == 0)});  // Even IDs are active
    }

    // Complex query: active users, age > 25, ordered by age desc, limit 5
    query<User> q;
    q.where("active = ? AND age > ?", {attribute(true), attribute(25)})
     .order("age DESC")
     .limit(5);

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
               {attribute("John"), attribute("john@example.com"), attribute(30)});

    // Load into model
    query<User> q;
    User user = q.first();

    std::string json = user.to_json_string();

    EXPECT_NE(std::string::npos, json.find("\"name\""));
    EXPECT_NE(std::string::npos, json.find("\"John\""));
    EXPECT_NE(std::string::npos, json.find("\"email\""));
}

TEST_F(DatabaseOperationsTest, ModelToXml) {
    // Insert test data
    auto conn = config::instance().get_connection();
    conn->query("INSERT INTO users (name, email) VALUES (?, ?)",
               {attribute("Jane"), attribute("jane@example.com")});

    // Load into model
    query<User> q;
    User user = q.first();

    std::string xml = user.to_xml_string();

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
    conn->query("INSERT INTO users (name, age) VALUES (?, ?)",
               {attribute("Test"), attribute(25)});

    // Load model
    query<User> q;
    User user = q.first();

    // Should not be dirty after load
    EXPECT_FALSE(user.is_dirty());

    // Modify
    user.set("age", attribute(26));
    EXPECT_TRUE(user.is_dirty());
    EXPECT_TRUE(user.attribute_is_dirty("age"));

    // Save
    user.save();
    EXPECT_FALSE(user.is_dirty());
}
