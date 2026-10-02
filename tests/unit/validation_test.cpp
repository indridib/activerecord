// ==============================================================================
// validation_test.cpp
//
// SPDX-FileCopyrightText: 2020-2026 Indridi Bjornsson
// SPDX-License-Identifier: MIT
// See file LICENSE.MIT for detail
// ==============================================================================

#include <algorithm>
#include <config.h>
#include <gtest/gtest.h>
#include <regex>
#include <typed_model.h>
#include <validations.h>

using namespace ar;

class validation_test_statement : public statement {
  public:
	explicit validation_test_statement(std::size_t affected_rows = 1) : affected_rows_(affected_rows) {}

	void execute(const std::string&, const attribute_vector&) override {}
	bool fetch() override { return false; }
	std::size_t affected_rows() const noexcept override { return affected_rows_; }
	void get_attributes(attribute_map&) override {}

  private:
	std::size_t affected_rows_{ 0 };
};

class validation_test_connection : public connection {
  public:
	std::size_t insert_id() const override { return 99; }

	std::unique_ptr<statement> query(const std::string&, const attribute_vector&) override {
		++query_count_;
		return std::make_unique<validation_test_statement>(1);
	}

	void begin_transaction() override {}
	void commit() override {}
	void rollback() override {}

	void limit(std::string& sql, long offset, long limit) const override {
		if (limit >= 0) {
			sql += std::format(" LIMIT {}", limit);
			if (offset >= 0) {
				sql += std::format(" OFFSET {}", offset);
			}
		}
	}

	std::size_t query_count() const noexcept { return query_count_; }
	void reset_query_count() noexcept { query_count_ = 0; }

  private:
	std::size_t query_count_{ 0 };
};

struct validated_user : typed_model<validated_user> {
	static constexpr auto table_name{ "users" };
	static constexpr auto primary_key{ "id" };

	long id{};
	std::string name{};
	std::string email{};
	int age{};

	static constexpr auto columns() {
		return ar::columns(ar::col<&validated_user::id>("id"), ar::col<&validated_user::name>("name"),
						   ar::col<&validated_user::email>("email"), ar::col<&validated_user::age>("age"));
	}

	static auto validations() {
		return ar::validate(
			ar::presence("name"), ar::presence("email"), ar::length("name", { .min = 2, .max = 20 }),
			ar::format("email", std::regex{ R"(^.+@.+$)" }),
			ar::numericality("age", { .greater_than = 0, .less_than_or_equal_to = 150, .only_integer = true }));
	}
};

struct no_validation_user : typed_model<no_validation_user> {
	static constexpr auto table_name{ "users" };
	static constexpr auto primary_key{ "id" };

	long id{};
	std::string name{};

	static constexpr auto columns() {
		return ar::columns(ar::col<&no_validation_user::id>("id"), ar::col<&no_validation_user::name>("name"));
	}
};

class ValidationTest : public ::testing::Test {
  protected:
	void SetUp() override {
		connection_ = std::make_shared<validation_test_connection>();
		config::connection_map connections;
		connections["default"] = connection_;
		config::instance().set_connections(connections, "default");
	}

	std::shared_ptr<validation_test_connection> connection_{};
};

TEST_F(ValidationTest, IsValidPopulatesErrorsForInvalidRecord) {
	validated_user user;
	user.name = "";
	user.email = "invalid-email";
	user.age = -1;

	EXPECT_FALSE(user.is_valid());
	EXPECT_TRUE(user.has_errors());
	EXPECT_EQ(4u, user.errors().count());

	ASSERT_EQ(2u, user.errors().on("name").size());
	EXPECT_EQ("can't be blank", user.errors().on("name")[0]);
	EXPECT_EQ("is too short (minimum is 2)", user.errors().on("name")[1]);

	ASSERT_EQ(1u, user.errors().on("email").size());
	EXPECT_EQ("is invalid", user.errors().on("email")[0]);

	ASSERT_EQ(1u, user.errors().on("age").size());
	EXPECT_EQ("must be greater than 0", user.errors().on("age")[0]);

	const auto full = user.errors().full_messages();
	EXPECT_EQ(4u, full.size());
	EXPECT_NE(std::find(full.begin(), full.end(), "name can't be blank"), full.end());
}

TEST_F(ValidationTest, IsValidClearsPreviousErrorsAfterFixingData) {
	validated_user user;
	user.name = "";
	user.email = "invalid";
	user.age = -1;
	EXPECT_FALSE(user.is_valid());
	EXPECT_TRUE(user.has_errors());

	user.name = "Bob";
	user.email = "bob@example.com";
	user.age = 42;

	EXPECT_TRUE(user.is_valid());
	EXPECT_FALSE(user.has_errors());
	EXPECT_TRUE(user.errors().full_messages().empty());
}

TEST_F(ValidationTest, SaveReturnsFalseAndSkipsInsertWhenInvalid) {
	validated_user user;
	user.name = "";
	user.email = "broken";
	user.age = 0;

	EXPECT_FALSE(user.save());
	EXPECT_TRUE(user.has_errors());
	EXPECT_EQ(0u, connection_->query_count());
}

TEST_F(ValidationTest, SaveFalseBypassesValidation) {
	validated_user user;
	user.name = "";
	user.email = "broken";
	user.age = -10;

	EXPECT_TRUE(user.save(false));
	EXPECT_EQ(1u, connection_->query_count());
}

TEST_F(ValidationTest, SaveSucceedsWhenValidAndClearsErrors) {
	validated_user user;
	user.name = "Alice";
	user.email = "alice@example.com";
	user.age = 30;

	EXPECT_TRUE(user.save());
	EXPECT_FALSE(user.has_errors());
	EXPECT_EQ(1u, connection_->query_count());
	EXPECT_EQ(99, user.id);
}

TEST_F(ValidationTest, ModelWithoutValidationsBehavesAsBefore) {
	no_validation_user user;
	user.name = "No Rules";

	EXPECT_TRUE(user.is_valid());
	EXPECT_FALSE(user.has_errors());
	EXPECT_TRUE(user.save());
	EXPECT_EQ(1u, connection_->query_count());
}
