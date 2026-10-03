// ============================================================================
// datetime_integration_test.cpp
//
// SPDX-FileCopyrightText: 2020-2026 Indridi Bjornsson
// SPDX-License-Identifier: MIT
// See file LICENSE.MIT for detail
//
// Integration tests for datetime with database adapters
// ============================================================================

#include "test_adapters.h"

#include <activerecord/attribute.h>
#include <activerecord/datetime.h>
#include <activerecord/model.h>
#include <gtest/gtest.h>

using namespace ar;
using namespace ar::test;

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

// A model over a native date/time column (not a Unix-timestamp integer)
class NativeTime : public model {
  public:
	NativeTime() : model(st_table_name(), st_primary_key()) {}
	static const char* st_table_name() { return "native_times"; }
	static const char* st_primary_key() { return "id"; }
};

class DateTimeIntegrationTest : public AdapterTest {
  protected:
	table_list table_definitions(AdapterType type) const override {
		const std::string ts{ timestamp_type(type) };
		return {
			// Datetimes stored as Unix timestamps (project convention)
			{ "events", std::format("{}, name {} NOT NULL, created_at {}, starts_at {}", pk_column(type),
									text_type(type), ts, ts) },
			// A native date/time column; named without an _at/_date/_time suffix so no
			// adapter can recognize it by name rather than by its declared type
			{ "native_times", std::format("{}, happened {}", pk_column(type), native_datetime_type(type)) },
		};
	}
};

// ============================================================================
// Basic DateTime Storage and Retrieval Tests
// ============================================================================

TEST_P(DateTimeIntegrationTest, InsertRecordWithDateTime) {
	Event event;
	event.set_name("Conference 2024");
	event.set_created_at(datetime::now());
	event.set_starts_at(datetime{ 2024, 9, 15, 9, 0, 0 });

	EXPECT_TRUE(event.save());
	EXPECT_GT(event.id(), 0);
}

TEST_P(DateTimeIntegrationTest, RetrieveDateTimeFromDatabase) {
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

	// Contract on every adapter: the Unix-timestamp column reads back as the same datetime
	// through as_date() (docs/attributes.md). The attribute's type() is adapter-specific: only
	// SQLite infers type_date for INTEGER columns named *_at / *_date / *_time; the others
	// return the stored integer.
	if (adapter_type() == AdapterType::SQLite) {
		EXPECT_EQ(type_date, retrieved.get("starts_at").type());
	}
	datetime retrieved_time = retrieved.starts_at();
	EXPECT_EQ(2024, retrieved_time.year());
	EXPECT_EQ(6, retrieved_time.month());
	EXPECT_EQ(15, retrieved_time.day());
	EXPECT_EQ(14, retrieved_time.hour());
	EXPECT_EQ(30, retrieved_time.minute());
}

TEST_P(DateTimeIntegrationTest, UpdateDateTimeField) {
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

TEST_P(DateTimeIntegrationTest, QueryByDateRange) {
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

TEST_P(DateTimeIntegrationTest, NullDateTimeField) {
	Event event;
	event.set_name("TBD Event");
	// Don't set starts_at - should remain NULL
	event.save();

	query<Event> q;
	Event retrieved = q.find(event.id());

	// Accessing NULL datetime field
	const attribute& attr = retrieved.get("starts_at");
	EXPECT_TRUE(attr.is_null());
}

// ============================================================================
// DateTime Format Tests
// ============================================================================

TEST_P(DateTimeIntegrationTest, DateOnlyStorage) {
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

TEST_P(DateTimeIntegrationTest, RoundTripFormatting) {
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

TEST_P(DateTimeIntegrationTest, CreatedAtTimestamp) {
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

TEST_P(DateTimeIntegrationTest, ScheduleFutureEvent) {
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

// ============================================================================
// Native date/time columns (runtime contract: SQL date/time/timestamp columns
// are read as ar::datetime)
// ============================================================================

TEST_P(DateTimeIntegrationTest, NativeColumnReadsAsDatetime) {
	// Written as a SQL literal: ar::datetime values are bound as Unix timestamps, which a
	// native date/time column would not accept
	adapter_->query("INSERT INTO native_times (happened) VALUES ('2024-06-15 14:30:45')", {});

	NativeTime row = query<NativeTime>().first_or_throw();
	const attribute& happened = row.get("happened");
	ASSERT_EQ(type_date, happened.type());

	// Compare components, not the epoch value, so the result does not depend on the time zone
	const datetime value = happened.as_date();
	EXPECT_EQ(2024, value.year());
	EXPECT_EQ(6, value.month());
	EXPECT_EQ(15, value.day());
	EXPECT_EQ(14, value.hour());
	EXPECT_EQ(30, value.minute());
	EXPECT_EQ(45, value.second());
}

// ============================================================================
// Test Instantiation
// ============================================================================

INSTANTIATE_TEST_SUITE_P(AllAdapters, DateTimeIntegrationTest, ::testing::ValuesIn(enabled_adapters()),
						 adapter_param_name);
