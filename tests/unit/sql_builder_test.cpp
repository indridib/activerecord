// ==============================================================================
// sql_builder_test.cpp
//
// SPDX-FileCopyrightText: 2020-2023 Indridi Bjornsson
// SPDX-License-Identifier: MIT
// See file LICENSE.MIT for detail
//
// Unit tests for the sql_builder class
// ==============================================================================

#include <activerecord.h>
#include <gtest/gtest.h>
#include <sql_builder.h>

using namespace ar;

// Mock connection for testing (satisfies the interface without real DB)
class mock_connection : public connection {
  public:
	std::unique_ptr<statement> query(const std::string&, const attribute_vector&) override { return nullptr; }

	void limit(std::string& sql, long offset, long limit) const override {
		if (limit >= 0) {
			sql += " LIMIT " + std::to_string(limit);
			if (offset >= 0) {
				sql += " OFFSET " + std::to_string(offset);
			}
		}
	}

	std::size_t insert_id() const override { return 0; }

	void begin_transaction() override {}
	void commit() override {}
	void rollback() override {}
};

class delete_sql_builder_for_test : public sql_builder {
  public:
	delete_sql_builder_for_test(config::connection_ptr con, std::string_view table) : sql_builder(con, table) {}

	std::string build_delete_public() const { return build_delete(); }
};

class mock_delete_limit_connection : public mock_connection {
  public:
	bool accepts_limit_and_order_for_update_and_delete() const override { return true; }
};

// ==============================================================================
// SELECT Statement Tests
// ==============================================================================

TEST(SqlBuilderTest, BasicSelect) {
	auto conn = std::make_shared<mock_connection>();
	sql_builder builder(conn, "users");

	builder.add_select("*");
	std::string sql = builder.to_string();

	EXPECT_NE(std::string::npos, sql.find("SELECT *"));
	EXPECT_NE(std::string::npos, sql.find("FROM users"));
}

TEST(SqlBuilderTest, SelectWithColumns) {
	auto conn = std::make_shared<mock_connection>();
	sql_builder builder(conn, "users");

	builder.add_select("id, name, email");
	std::string sql = builder.to_string();

	EXPECT_NE(std::string::npos, sql.find("SELECT id, name, email"));
}

TEST(SqlBuilderTest, SelectWithWhere) {
	auto conn = std::make_shared<mock_connection>();
	sql_builder builder(conn, "users");

	builder.add_select("*");
	builder.add_where("id = ?", { 5 });
	std::string sql = builder.to_string();

	EXPECT_NE(std::string::npos, sql.find("WHERE"));
	EXPECT_NE(std::string::npos, sql.find("id = ?"));

	const attribute_vector& bind_values = builder.get_bind_values();
	ASSERT_EQ(1, bind_values.size());
	EXPECT_EQ(5, bind_values[0].as_integer());
}

TEST(SqlBuilderTest, SelectWithMultipleWhereValues) {
	auto conn = std::make_shared<mock_connection>();
	sql_builder builder(conn, "users");

	builder.add_select("*");
	attribute_vector values = { "John", 25 };
	builder.add_where("name = ? AND age > ?", values);
	std::string sql = builder.to_string();

	EXPECT_NE(std::string::npos, sql.find("WHERE"));
	EXPECT_NE(std::string::npos, sql.find("name = ? AND age > ?"));

	const attribute_vector& bind_values = builder.get_bind_values();
	ASSERT_EQ(2, bind_values.size());
	EXPECT_EQ("John", bind_values[0].as_string());
	EXPECT_EQ(25, bind_values[1].as_integer());
}

TEST(SqlBuilderTest, SelectWithOrderBy) {
	auto conn = std::make_shared<mock_connection>();
	sql_builder builder(conn, "users");

	builder.add_select("*");
	builder.add_order("name ASC");
	std::string sql = builder.to_string();

	EXPECT_NE(std::string::npos, sql.find("ORDER BY name ASC"));
}

TEST(SqlBuilderTest, SelectWithGroupBy) {
	auto conn = std::make_shared<mock_connection>();
	sql_builder builder(conn, "orders");

	builder.add_select("customer_id, COUNT(*)");
	builder.add_group("customer_id");
	std::string sql = builder.to_string();

	EXPECT_NE(std::string::npos, sql.find("GROUP BY customer_id"));
}

TEST(SqlBuilderTest, SelectWithHaving) {
	auto conn = std::make_shared<mock_connection>();
	sql_builder builder(conn, "orders");

	builder.add_select("customer_id, COUNT(*)");
	builder.add_group("customer_id");
	builder.add_having("COUNT(*) > 5");
	std::string sql = builder.to_string();

	EXPECT_NE(std::string::npos, sql.find("HAVING COUNT(*) > 5"));
}

TEST(SqlBuilderTest, SelectWithJoins) {
	auto conn = std::make_shared<mock_connection>();
	sql_builder builder(conn, "users");

	builder.add_select("users.*, profiles.bio");
	builder.add_joins("INNER JOIN profiles ON users.id = profiles.user_id");
	std::string sql = builder.to_string();

	EXPECT_NE(std::string::npos, sql.find("INNER JOIN profiles"));
}

TEST(SqlBuilderTest, SelectWithLimit) {
	auto conn = std::make_shared<mock_connection>();
	sql_builder builder(conn, "users");

	builder.add_select("*");
	builder.set_limit(10);
	std::string sql = builder.to_string();

	EXPECT_NE(std::string::npos, sql.find("LIMIT 10"));
}

TEST(SqlBuilderTest, SelectWithLimitAndOffset) {
	auto conn = std::make_shared<mock_connection>();
	sql_builder builder(conn, "users");

	builder.add_select("*");
	builder.set_limit(10);
	builder.set_offset(20);
	std::string sql = builder.to_string();

	EXPECT_NE(std::string::npos, sql.find("LIMIT 10"));
	EXPECT_NE(std::string::npos, sql.find("OFFSET 20"));
}

TEST(SqlBuilderTest, CompleteSelectQuery) {
	auto conn = std::make_shared<mock_connection>();
	sql_builder builder(conn, "users");

	builder.add_select("id, name");
	builder.add_where("age > ?", { 18 });
	builder.add_order("name ASC");
	builder.set_limit(10);
	builder.set_offset(5);

	std::string sql = builder.to_string();

	EXPECT_NE(std::string::npos, sql.find("SELECT id, name"));
	EXPECT_NE(std::string::npos, sql.find("FROM users"));
	EXPECT_NE(std::string::npos, sql.find("WHERE age > ?"));
	EXPECT_NE(std::string::npos, sql.find("ORDER BY name ASC"));
	EXPECT_NE(std::string::npos, sql.find("LIMIT 10"));
	EXPECT_NE(std::string::npos, sql.find("OFFSET 5"));
}

// ==============================================================================
// INSERT Statement Tests
// ==============================================================================

TEST(SqlBuilderTest, BasicInsert) {
	auto conn = std::make_shared<mock_connection>();
	sql_builder builder(conn, "users");

	string_vector names = { "name", "email", "age" };
	attribute_vector values = { "John", "john@example.com", 30 };

	builder.insert(names, values);
	std::string sql = builder.to_string();

	EXPECT_NE(std::string::npos, sql.find("INSERT INTO users"));
	EXPECT_NE(std::string::npos, sql.find("name"));
	EXPECT_NE(std::string::npos, sql.find("email"));
	EXPECT_NE(std::string::npos, sql.find("age"));
	EXPECT_NE(std::string::npos, sql.find("VALUES"));
	EXPECT_NE(std::string::npos, sql.find("?"));

	const attribute_vector& bind_values = builder.get_bind_values();
	ASSERT_EQ(3, bind_values.size());
	EXPECT_EQ("John", bind_values[0].as_string());
	EXPECT_EQ("john@example.com", bind_values[1].as_string());
	EXPECT_EQ(30, bind_values[2].as_integer());
}

TEST(SqlBuilderTest, InsertSingleColumn) {
	auto conn = std::make_shared<mock_connection>();
	sql_builder builder(conn, "users");

	string_vector names = { "name" };
	attribute_vector values = { "John" };

	builder.insert(names, values);
	std::string sql = builder.to_string();

	EXPECT_NE(std::string::npos, sql.find("INSERT INTO users"));
	EXPECT_NE(std::string::npos, sql.find("(`name`)"));
	EXPECT_NE(std::string::npos, sql.find("VALUES(?)"));
}

// ==============================================================================
// UPDATE Statement Tests
// ==============================================================================

TEST(SqlBuilderTest, BasicUpdate) {
	auto conn = std::make_shared<mock_connection>();
	sql_builder builder(conn, "users");

	string_vector names = { "name", "age" };
	attribute_vector values = { "Jane", 25 };

	builder.update(names, values);
	std::string sql = builder.to_string();

	EXPECT_NE(std::string::npos, sql.find("UPDATE users"));
	EXPECT_NE(std::string::npos, sql.find("SET"));
	EXPECT_NE(std::string::npos, sql.find("?"));
}

TEST(SqlBuilderTest, UpdateWithWhere) {
	auto conn = std::make_shared<mock_connection>();
	sql_builder builder(conn, "users");

	string_vector names = { "name" };
	attribute_vector values = { "Jane" };

	builder.update(names, values);
	builder.add_where("id = ?", { 5 });
	std::string sql = builder.to_string();

	EXPECT_NE(std::string::npos, sql.find("UPDATE users"));
	EXPECT_NE(std::string::npos, sql.find("WHERE id = ?"));

	const attribute_vector& bind_values = builder.get_bind_values();
	ASSERT_EQ(2, bind_values.size());
	EXPECT_EQ("Jane", bind_values[0].as_string()); // UPDATE value first
	EXPECT_EQ(5, bind_values[1].as_integer());	   // WHERE value second
}

// ==============================================================================
// DELETE Statement Tests
// ==============================================================================

TEST(SqlBuilderTest, DeleteWithWhereUsesBindValues) {
	auto conn = std::make_shared<mock_connection>();
	delete_sql_builder_for_test builder(conn, "users");

	builder.add_where("id = ?", { 7 });
	std::string sql = builder.build_delete_public();

	EXPECT_EQ("DELETE FROM users WHERE id = ?", sql);
	const attribute_vector& bind_values = builder.get_bind_values();
	ASSERT_EQ(1, bind_values.size());
	EXPECT_EQ(7, bind_values[0].as_integer());
}

TEST(SqlBuilderTest, DeleteWithOrderLimitAndOffset) {
	auto conn = std::make_shared<mock_delete_limit_connection>();
	delete_sql_builder_for_test builder(conn, "users");

	builder.add_where("active = ?", { true });
	builder.add_order("id DESC");
	builder.set_limit(2);
	builder.set_offset(1);

	std::string sql = builder.build_delete_public();
	EXPECT_EQ("DELETE FROM users WHERE active = ? ORDER BY id DESC LIMIT 2 OFFSET 1", sql);

	const attribute_vector& bind_values = builder.get_bind_values();
	ASSERT_EQ(1, bind_values.size());
	EXPECT_TRUE(bind_values[0].as_bool());
}
