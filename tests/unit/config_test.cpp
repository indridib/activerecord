// ==============================================================================
// config_test.cpp
//
// SPDX-FileCopyrightText: 2020-2023 Indridi Bjornsson
// SPDX-License-Identifier: MIT
// See file LICENSE.MIT for detail
//
// Unit tests for the config class
// ==============================================================================

#include <activerecord/activerecord.h>
#include <activerecord/config.h>
#include <activerecord/connection.h>
#include <gtest/gtest.h>

using namespace ar;

// Mock connection for testing
class test_connection : public connection {
  public:
	std::unique_ptr<statement> query(const std::string&, const attribute_vector&) override { return nullptr; }

	void limit(std::string&, long, long) const override {}

	std::size_t insert_id() const override { return 0; }

	void begin_transaction() override {}
	void commit() override {}
	void rollback() override {}
};

// ==============================================================================
// Singleton Tests
// ==============================================================================

TEST(ConfigTest, SingletonInstance) {
	config& instance1 = config::instance();
	config& instance2 = config::instance();

	EXPECT_EQ(&instance1, &instance2);
}

// ==============================================================================
// Connection Management Tests
// ==============================================================================

TEST(ConfigTest, SetAndGetConnection) {
	config::connection_map connections;
	connections["test_db"] = std::make_shared<test_connection>();

	config::instance().set_connections(connections);

	auto conn = config::instance().get_connection("test_db");
	EXPECT_NE(nullptr, conn);
}

TEST(ConfigTest, SetConnectionsWithDefault) {
	config::connection_map connections;
	connections["primary"] = std::make_shared<test_connection>();
	connections["secondary"] = std::make_shared<test_connection>();

	config::instance().set_connections(connections, "primary");

	auto conn = config::instance().get_connection(); // Should return default
	EXPECT_NE(nullptr, conn);
}

TEST(ConfigTest, GetDefaultConnection) {
	config::connection_map connections;
	connections["default"] = std::make_shared<test_connection>();

	config::instance().set_connections(connections, "default");

	auto conn = config::instance().get_connection();
	EXPECT_NE(nullptr, conn);
}

TEST(ConfigTest, GetConnectionByName) {
	config::connection_map connections;
	connections["db1"] = std::make_shared<test_connection>();
	connections["db2"] = std::make_shared<test_connection>();

	config::instance().set_connections(connections);

	auto conn1 = config::instance().get_connection("db1");
	auto conn2 = config::instance().get_connection("db2");

	EXPECT_NE(nullptr, conn1);
	EXPECT_NE(nullptr, conn2);
	EXPECT_NE(conn1, conn2); // Should be different connections
}

TEST(ConfigTest, GetNonExistentConnectionThrows) {
	config::connection_map connections;
	connections["existing"] = std::make_shared<test_connection>();

	config::instance().set_connections(connections);

	EXPECT_THROW(config::instance().get_connection("nonexistent"), ex::config);
}

TEST(ConfigTest, GetConnectionWithEmptyNameAndNoDefault) {
	config::connection_map connections;
	connections["some_db"] = std::make_shared<test_connection>();

	config::instance().set_connections(connections, ""); // Explicitly clear default

	EXPECT_THROW(config::instance().get_connection(""), ex::config);
}
