// ============================================================================
// datetime_integration_test.cpp
//
// SPDX-FileCopyrightText: 2020-2026 Indridi Bjornsson
// SPDX-License-Identifier: MIT
// See file LICENSE.MIT for detail
//
// Integration tests for datetime with database adapters
// ============================================================================

#include <attribute.h>
#include <datetime.h>
#include <gtest/gtest.h>
#include <model.h>

#ifdef AR_SQLITE
#include <adapters/sqlite_adapter.h>
#endif

using namespace ar;

#ifdef AR_SQLITE

// ============================================================================
// Test Model with DateTime Fields
// ============================================================================

class Event : public model {
  public:
	Event() : model(st_table_name(), st_primary_key()) {}

	// Model metadata
	static const char* st_table_name() { return "events"; }
	static const char* st_primary_key() { return "id"; }

	// Field accessors
	int id() const { return static_cast<int>(get("id").as_integer()); }
	std::string name() const { return get("name").as_string(); }
	datetime created_at() const { return get("created_at").as_date(); }
	datetime starts_at() const { return get("starts_at").as_date(); }

	// Field setters
	void set_name(const std::string& value) { set("name", value); }
	void set_created_at(const datetime& value) { set("created_at", value); }
	void set_starts_at(const datetime& value) { set("starts_at", value); }
};

// ============================================================================
// Test Fixture
// ============================================================================

class DateTimeIntegrationTest : public ::testing::Test {
  protected:
	void SetUp() override {
		// Use in-memory SQLite database
		auto adapter = std::make_shared<sqlite_adapter>(":memory:");

		config::connection_map connections;
		connections["default"] = adapter;
		config::instance().set_connections(connections, "default");

		// Create events table
		std::string create_sql = R"(
			CREATE TABLE events (
				id INTEGER PRIMARY KEY AUTOINCREMENT,
				name TEXT NOT NULL,
				created_at INTEGER,
				starts_at INTEGER
			)
		)";
		adapter->query(create_sql);
	}

	void TearDown() override {
		// Clean up
	}
};

// ============================================================================
// Basic DateTime Storage and Retrieval Tests
// ============================================================================

TEST_F(DateTimeIntegrationTest, InsertRecordWithDateTime) {
	Event event;
	event.set_name("Conference 2024");
	event.set_created_at(datetime::now());
	event.set_starts_at(datetime{ 2024, 9, 15, 9, 0, 0 });

	EXPECT_TRUE(event.save());
	EXPECT_GT(event.id(), 0);
}

TEST_F(DateTimeIntegrationTest, RetrieveDateTimeFromDatabase) {
	// Insert a record
	Event event;
	event.set_name("Meeting");
	datetime start_time{ 2024, 6, 15, 14, 30, 0 };
	event.set_starts_at(start_time);
	event.save();

	// Retrieve it
	query<Event> q;
	Event retrieved = q.find(event.id());

	EXPECT_EQ("Meeting", retrieved.name());

	datetime retrieved_time = retrieved.starts_at();
	EXPECT_EQ(type_date, retrieved.get("starts_at").type());
	EXPECT_EQ(2024, retrieved_time.year());
	EXPECT_EQ(6, retrieved_time.month());
	EXPECT_EQ(15, retrieved_time.day());
	EXPECT_EQ(14, retrieved_time.hour());
	EXPECT_EQ(30, retrieved_time.minute());
}

TEST_F(DateTimeIntegrationTest, UpdateDateTimeField) {
	// Create event
	Event event;
	event.set_name("Workshop");
	event.set_starts_at(datetime{ 2024, 6, 15, 10, 0, 0 });
	event.save();

	// Update the start time
	int id = event.id();
	datetime new_time{ 2024, 6, 16, 14, 0, 0 }; // Reschedule to next day
	event.set_starts_at(new_time);
	EXPECT_TRUE(event.save());

	// Verify update
	query<Event> q;
	Event updated = q.find(id);
	datetime retrieved_time = updated.starts_at();

	EXPECT_EQ(2024, retrieved_time.year());
	EXPECT_EQ(6, retrieved_time.month());
	EXPECT_EQ(16, retrieved_time.day());  // Changed from 15 to 16
	EXPECT_EQ(14, retrieved_time.hour()); // Changed from 10 to 14
}

// ============================================================================
// Query Tests with DateTime
// ============================================================================

TEST_F(DateTimeIntegrationTest, QueryByDateRange) {
	// Insert events at different times
	Event event1;
	event1.set_name("Event 1");
	event1.set_starts_at(datetime{ 2024, 6, 10, 10, 0, 0 });
	event1.save();

	Event event2;
	event2.set_name("Event 2");
	event2.set_starts_at(datetime{ 2024, 6, 15, 10, 0, 0 });
	event2.save();

	Event event3;
	event3.set_name("Event 3");
	event3.set_starts_at(datetime{ 2024, 6, 20, 10, 0, 0 });
	event3.save();

	// Query events after June 12
	query<Event> q;
	q.where("starts_at >= ?", attribute_vector{ attribute{ datetime{ 2024, 6, 12, 0, 0, 0 } } });
	auto events = q.all();

	EXPECT_EQ(2, events.size()); // Should get Event 2 and Event 3
}

// ============================================================================
// NULL DateTime Tests
// ============================================================================

TEST_F(DateTimeIntegrationTest, NullDateTimeField) {
	Event event;
	event.set_name("TBD Event");
	// Don't set starts_at - should remain NULL
	event.save();

	query<Event> q;
	Event retrieved = q.find(event.id());

	// Accessing NULL datetime field
	attribute attr = retrieved.get("starts_at");
	EXPECT_TRUE(attr.is_null());
}

// ============================================================================
// DateTime Format Tests
// ============================================================================

TEST_F(DateTimeIntegrationTest, DateOnlyStorage) {
	Event event;
	event.set_name("Birthday Party");
	datetime date_only{ 2024, 12, 25 }; // Christmas, no time
	event.set_starts_at(date_only);
	event.save();

	query<Event> q;
	Event retrieved = q.find(event.id());
	datetime retrieved_date = retrieved.starts_at();

	EXPECT_EQ(2024, retrieved_date.year());
	EXPECT_EQ(12, retrieved_date.month());
	EXPECT_EQ(25, retrieved_date.day());
	EXPECT_EQ(0, retrieved_date.hour());
	EXPECT_EQ(0, retrieved_date.minute());
}

TEST_F(DateTimeIntegrationTest, RoundTripFormatting) {
	// Test that datetime survives database round-trip
	datetime original{ 2024, 6, 15, 14, 30, 45 };

	Event event;
	event.set_name("Test Event");
	event.set_starts_at(original);
	event.save();

	query<Event> q;
	Event retrieved = q.find(event.id());
	datetime roundtrip = retrieved.starts_at();

	EXPECT_EQ(original.year(), roundtrip.year());
	EXPECT_EQ(original.month(), roundtrip.month());
	EXPECT_EQ(original.day(), roundtrip.day());
	EXPECT_EQ(original.hour(), roundtrip.hour());
	EXPECT_EQ(original.minute(), roundtrip.minute());
	EXPECT_EQ(original.second(), roundtrip.second());
}

// ============================================================================
// Practical Usage Examples
// ============================================================================

TEST_F(DateTimeIntegrationTest, CreatedAtTimestamp) {
	// Common pattern: Set created_at when creating record
	Event event;
	event.set_name("New Event");
	event.set_created_at(datetime::now());
	event.save();

	// Verify created_at was stored
	query<Event> q;
	Event retrieved = q.find(event.id());
	datetime created = retrieved.created_at();

	EXPECT_GT(created.timestamp(), 0);
	EXPECT_EQ(datetime::now().year(), created.year()); // Same year as now
}

TEST_F(DateTimeIntegrationTest, ScheduleFutureEvent) {
	// Schedule event for future date
	Event event;
	event.set_name("Future Conference");

	// Set date to September 1, 2025
	datetime future{ 2025, 9, 1, 9, 0, 0 };
	event.set_starts_at(future);
	event.save();

	query<Event> q;
	Event retrieved = q.find(event.id());

	EXPECT_EQ("Future Conference", retrieved.name());
	EXPECT_EQ(2025, retrieved.starts_at().year());
}

#endif // AR_SQLITE
