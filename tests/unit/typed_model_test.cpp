// ============================================================================
// typed_model_test.cpp
//
// SPDX-FileCopyrightText: 2020-2026 Indridi Bjornsson
// SPDX-License-Identifier: MIT
// See file LICENSE for details
//
// Unit tests for typed_model<T>
//

#include <activerecord/config.h>
#include <activerecord/sql_builder.h>
#include <activerecord/typed_model.h>
#include <gtest/gtest.h>

using namespace ar;

class typed_test_statement : public statement {
  public:
	explicit typed_test_statement(std::vector<attribute_map> rows, std::size_t affected_rows = 1)
		: rows_(std::move(rows)), affected_rows_(affected_rows) {}

	void execute(const std::string&, const attribute_vector&) override {}

	bool fetch() override {
		if (index_ >= rows_.size()) {
			return false;
		}
		++index_;
		return true;
	}

	std::size_t affected_rows() const noexcept override { return affected_rows_; }

	void get_attributes(attribute_map& attrs) override {
		if (index_ == 0 || index_ > rows_.size()) {
			attrs.clear();
			return;
		}
		attrs = rows_[index_ - 1];
	}

  private:
	std::vector<attribute_map> rows_{};
	std::size_t index_{ 0 };
	std::size_t affected_rows_{ 0 };
};

class typed_test_connection : public connection {
  public:
	std::size_t insert_id() const override { return 1; }

	std::unique_ptr<statement> query(const std::string&, const attribute_vector&) override {
		if (!queued_.empty()) {
			auto stmt = std::move(queued_.front());
			queued_.erase(queued_.begin());
			return stmt;
		}
		return std::make_unique<typed_test_statement>(std::vector<attribute_map>{}, 1);
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

	void queue(std::unique_ptr<statement> stmt) { queued_.push_back(std::move(stmt)); }

  private:
	std::vector<std::unique_ptr<statement>> queued_{};
};

struct typed_user : typed_model<typed_user> {
	static constexpr auto table_name{ "users" };
	static constexpr auto primary_key{ "id" };

	long id{};
	std::string name{};
	std::string email{};

	static constexpr auto columns() {
		return ar::columns(ar::col<&typed_user::id>("id"), ar::col<&typed_user::name>("name"),
						   ar::col<&typed_user::email>("email"));
	}
};

struct typed_user_tracking_off : typed_model<typed_user_tracking_off, false> {
	static constexpr auto table_name{ "users" };
	static constexpr auto primary_key{ "id" };

	long id{};
	std::string name{};
	ar::datetime updated_at{};
	int64_t version{};

	static constexpr auto columns() {
		return ar::columns(ar::col<&typed_user_tracking_off::id>("id"), ar::col<&typed_user_tracking_off::name>("name"),
						   ar::col<&typed_user_tracking_off::updated_at>("updated_at"),
						   ar::col<&typed_user_tracking_off::version>("version"));
	}
};

// NOLINTNEXTLINE(performance-enum-size) -- int32_t base is deliberate: exercises enum <-> INTEGER mapping
enum class task_state : int32_t { pending = 0, running = 1, done = 2 };

struct typed_task : typed_model<typed_task> {
	static constexpr auto table_name{ "tasks" };
	static constexpr auto primary_key{ "id" };

	long id{};
	task_state state{ task_state::pending };

	static constexpr auto columns() {
		return ar::columns(ar::col<&typed_task::id>("id"), ar::col<&typed_task::state>("state"));
	}
};

class TypedModelTest : public ::testing::Test {
  protected:
	void SetUp() override {
		connection_ = std::make_shared<typed_test_connection>();
		config::connection_map connections;
		connections["default"] = connection_;
		config::instance().set_connections(connections, "default");
	}

	std::shared_ptr<typed_test_connection> connection_{};
};

TEST_F(TypedModelTest, QueryFirstSyncsMembersFromAttributes) {
	attribute_map row;
	row["id"] = 7;
	row["name"] = "Alice";
	row["email"] = "alice@example.com";
	connection_->queue(std::make_unique<typed_test_statement>(std::vector<attribute_map>{ row }));

	auto user = query<typed_user>().first();
	ASSERT_TRUE(user.has_value());
	EXPECT_EQ(7, user->id);
	EXPECT_EQ("Alice", user->name);
	EXPECT_EQ("alice@example.com", user->email);
}

TEST_F(TypedModelTest, QueryIteratorSyncsMembersFromAttributes) {
	attribute_map row1;
	row1["id"] = 1;
	row1["name"] = "A";
	row1["email"] = "a@example.com";
	attribute_map row2;
	row2["id"] = 2;
	row2["name"] = "B";
	row2["email"] = "b@example.com";
	connection_->queue(std::make_unique<typed_test_statement>(std::vector<attribute_map>{ row1, row2 }));

	auto users = query<typed_user>().all();
	ASSERT_EQ(2u, users.size());
	EXPECT_EQ(1, users[0].id);
	EXPECT_EQ("A", users[0].name);
	EXPECT_EQ(2, users[1].id);
	EXPECT_EQ("B", users[1].name);
}

TEST_F(TypedModelTest, SyncToAttributesOnlyMarksChangedMembersDirty) {
	typed_user user;
	user.attributes_["id"] = 10;
	user.attributes_["name"] = "Jane";
	user.attributes_["email"] = "jane@example.com";
	user.new_record_ = false;
	user.sync_from_attributes();
	user.reset_dirty();

	user.sync_to_attributes();
	EXPECT_FALSE(user.is_dirty());

	user.email = "new@example.com";
	user.sync_to_attributes();
	EXPECT_TRUE(user.attribute_is_dirty("email"));
	EXPECT_FALSE(user.attribute_is_dirty("name"));
	EXPECT_FALSE(user.attribute_is_dirty("id"));
}

TEST_F(TypedModelTest, PersistedPrimaryKeyMutationThrowsOnSave) {
	typed_user user;
	user.attributes_["id"] = 42;
	user.attributes_["name"] = "Jane";
	user.attributes_["email"] = "jane@example.com";
	user.new_record_ = false;
	user.sync_from_attributes();
	user.reset_dirty();

	user.id = 43;
	EXPECT_THROW(user.save(false), ex::readonly);
}

TEST_F(TypedModelTest, SaveSyncsGeneratedPrimaryKeyBackToMembers) {
	typed_user user;
	user.name = "New User";
	user.email = "new@example.com";

	EXPECT_TRUE(user.save(false));
	EXPECT_EQ(1, user.id);

	// No member changes; should not throw due to stale member/snapshot state.
	EXPECT_NO_THROW(user.save(false));
}

TEST_F(TypedModelTest, TypedModelCanDisableRecordMetadataTracking) {
	typed_user_tracking_off user;
	user.attributes_["id"] = 11;
	user.attributes_["name"] = "Frozen";
	user.attributes_["updated_at"] = ar::datetime{ 2024, 6, 15, 10, 0, 0 };
	user.attributes_["version"] = static_cast<int64_t>(7);
	user.new_record_ = false;
	user.sync_from_attributes();
	user.reset_dirty();

	user.name = "Changed";

	// Simulate zero affected rows. With version tracking enabled this would throw stale_record.
	connection_->queue(std::make_unique<typed_test_statement>(std::vector<attribute_map>{}, 0));
	EXPECT_NO_THROW(user.save(false));
	EXPECT_EQ(7, user.version);
	const auto expected_updated_at = ar::datetime{ 2024, 6, 15, 10, 0, 0 }.timestamp();
	EXPECT_EQ(expected_updated_at, user.updated_at.timestamp());
}

TEST_F(TypedModelTest, QueryFirstSyncsEnumMembersFromAttributes) {
	attribute_map row;
	row["id"] = 3;
	row["state"] = static_cast<int64_t>(2);
	connection_->queue(std::make_unique<typed_test_statement>(std::vector<attribute_map>{ row }));

	auto task = query<typed_task>().first();
	ASSERT_TRUE(task.has_value());
	EXPECT_EQ(3, task->id);
	EXPECT_EQ(task_state::done, task->state);
}

TEST_F(TypedModelTest, SyncToAttributesConvertsEnumMembersToIntegerAttributes) {
	typed_task task;
	task.id = 9;
	task.state = task_state::running;

	task.sync_to_attributes();

	ASSERT_TRUE(task.isset("state"));
	EXPECT_EQ(1, task.attributes_["state"].as_integer());
}
