// ============================================================================
// transaction_test.cpp
//
// SPDX-FileCopyrightText: 2020-2026 Indridi Bjornsson
// SPDX-License-Identifier: MIT
// See file LICENSE.MIT for detail
//
// Unit tests for transaction RAII wrapper
// ============================================================================

#include <activerecord/config.h>
#include <activerecord/model.h>
#include <activerecord/transaction.h>
#include <gtest/gtest.h>

#ifdef AR_SQLITE
#include <activerecord/adapters/sqlite_adapter.h>

using namespace ar;

// Test fixture with in-memory database
class TransactionTest : public ::testing::Test {
  protected:
	void SetUp() override {
		// Create in-memory SQLite database
		auto adapter = std::make_shared<sqlite_adapter>(":memory:");

		config::connection_map connections;
		connections["default"] = adapter;
		config::instance().set_connections(connections, "default");

		// Create test table
		std::string create_sql = R"(
			CREATE TABLE test_items (
				id INTEGER PRIMARY KEY AUTOINCREMENT,
				name TEXT NOT NULL,
				value INTEGER
			)
		)";
		adapter->query(create_sql);
	}
};

// Test model
class TestItem : public model {
  public:
	TestItem() : model(st_table_name(), st_primary_key()) {}

	static const char* st_table_name() { return "test_items"; }
	static const char* st_primary_key() { return "id"; }

	int id() const { return static_cast<int>(get("id").as_integer()); }
	std::string name() const { return get("name").as_string(); }
	int value() const { return static_cast<int>(get("value").as_integer()); }

	void set_name(const std::string& val) { set("name", val); }
	void set_value(int val) { set("value", static_cast<int64_t>(val)); }
};

// ============================================================================
// Basic Transaction Tests
// ============================================================================

TEST_F(TransactionTest, CommitSavesChanges) {
	{
		transaction tx;

		TestItem item;
		item.set_name("Item 1");
		item.set_value(100);
		EXPECT_TRUE(item.save());

		tx.commit();
	}

	// Verify data was committed
	query<TestItem> q;
	auto items = q.all();
	ASSERT_EQ(1, items.size());
	EXPECT_EQ("Item 1", items[0].name());
	EXPECT_EQ(100, items[0].value());
}

TEST_F(TransactionTest, RollbackDiscardsChanges) {
	{
		transaction tx;

		TestItem item;
		item.set_name("Item 1");
		item.set_value(100);
		EXPECT_TRUE(item.save());

		// Explicit rollback
		tx.rollback();
	}

	// Verify data was rolled back
	query<TestItem> q;
	EXPECT_EQ(0, q.count());
}

TEST_F(TransactionTest, AutoRollbackOnException) {
	try {
		transaction tx;

		TestItem item;
		item.set_name("Item 1");
		item.set_value(100);
		EXPECT_TRUE(item.save());

		// Throw before commit; tx must roll back in its destructor
		throw std::runtime_error("Simulated error");
	} catch (const std::runtime_error&) { // NOLINT(bugprone-empty-catch) -- expected exception
	}

	// Verify data was rolled back
	query<TestItem> q;
	EXPECT_EQ(0, q.count());
}

TEST_F(TransactionTest, AutoRollbackOnScopeExit) {
	{
		transaction tx;

		TestItem item;
		item.set_name("Item 1");
		item.set_value(100);
		EXPECT_TRUE(item.save());

		// Exit scope without commit - should auto-rollback
	}

	// Verify data was rolled back
	query<TestItem> q;
	EXPECT_EQ(0, q.count());
}

// ============================================================================
// Multiple Operations in Transaction
// ============================================================================

TEST_F(TransactionTest, MultipleInsertsInTransaction) {
	{
		transaction tx;

		TestItem item1;
		item1.set_name("Item 1");
		item1.set_value(100);
		EXPECT_TRUE(item1.save());

		TestItem item2;
		item2.set_name("Item 2");
		item2.set_value(200);
		EXPECT_TRUE(item2.save());

		TestItem item3;
		item3.set_name("Item 3");
		item3.set_value(300);
		EXPECT_TRUE(item3.save());

		tx.commit();
	}

	// Verify all data was committed
	query<TestItem> q;
	auto items = q.all();
	ASSERT_EQ(3, items.size());
}

TEST_F(TransactionTest, InsertAndUpdateInTransaction) {
	int item_id;

	{
		transaction tx;

		// Insert
		TestItem item;
		item.set_name("Original");
		item.set_value(100);
		EXPECT_TRUE(item.save());
		item_id = item.id();

		// Update
		item.set_name("Updated");
		item.set_value(200);
		EXPECT_TRUE(item.save());

		tx.commit();
	}

	// Verify data was committed with updates
	query<TestItem> q;
	TestItem retrieved = q.find(item_id);
	EXPECT_EQ("Updated", retrieved.name());
	EXPECT_EQ(200, retrieved.value());
}

// ============================================================================
// Error Handling
// ============================================================================

TEST_F(TransactionTest, PartialRollbackOnError) {
	{
		transaction tx;

		TestItem item1;
		item1.set_name("Item 1");
		item1.set_value(100);
		EXPECT_TRUE(item1.save());

		TestItem item2;
		item2.set_name("Item 2");
		item2.set_value(200);
		EXPECT_TRUE(item2.save());

		// Don't commit - both should be rolled back
	}

	// Verify both items were rolled back
	query<TestItem> q;
	EXPECT_EQ(0, q.count());
}

TEST_F(TransactionTest, CommitIsIdempotent) {
	{
		transaction tx;

		TestItem item;
		item.set_name("Item 1");
		item.set_value(100);
		EXPECT_TRUE(item.save());

		tx.commit();
		tx.commit(); // Second commit should be safe (no-op)
	}

	// Verify data was committed
	query<TestItem> q;
	EXPECT_EQ(1, q.count());
}

TEST_F(TransactionTest, RollbackIsIdempotent) {
	{
		transaction tx;

		TestItem item;
		item.set_name("Item 1");
		item.set_value(100);
		EXPECT_TRUE(item.save());

		tx.rollback();
		tx.rollback(); // Second rollback should be safe (no-op)
	}

	// Verify data was rolled back
	query<TestItem> q;
	EXPECT_EQ(0, q.count());
}

#endif // AR_SQLITE
