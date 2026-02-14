// ============================================================================
// datetime_test.cpp
//
// SPDX-FileCopyrightText: 2020-2026 Indridi Bjornsson
// SPDX-License-Identifier: MIT
// See file LICENSE.MIT for detail
//
// Unit tests for ar::datetime class
// ============================================================================

#include <gtest/gtest.h>
#include <datetime.h>
#include <attribute.h>

using namespace ar;

// ============================================================================
// Basic Construction Tests
// ============================================================================

TEST(DateTimeTest, DefaultConstructor) {
	datetime dt{};
	EXPECT_EQ(0, dt.timestamp());
	EXPECT_EQ("", dt.to_string());
}

TEST(DateTimeTest, ConstructFromTimestamp) {
	std::time_t now = std::time(nullptr);
	datetime dt{now};
	EXPECT_EQ(now, dt.timestamp());
}

TEST(DateTimeTest, ConstructFromComponents) {
	datetime dt{2024, 6, 15, 14, 30, 45};

	EXPECT_EQ(2024, dt.year());
	EXPECT_EQ(6, dt.month());
	EXPECT_EQ(15, dt.day());
	EXPECT_EQ(14, dt.hour());
	EXPECT_EQ(30, dt.minute());
	EXPECT_EQ(45, dt.second());
}

TEST(DateTimeTest, ConstructFromDateOnly) {
	datetime dt{2024, 12, 25};  // Christmas 2024

	EXPECT_EQ(2024, dt.year());
	EXPECT_EQ(12, dt.month());
	EXPECT_EQ(25, dt.day());
	EXPECT_EQ(0, dt.hour());
	EXPECT_EQ(0, dt.minute());
	EXPECT_EQ(0, dt.second());
}

// ============================================================================
// String Parsing Tests
// ============================================================================

TEST(DateTimeTest, ParseFullDateTime) {
	datetime dt = datetime::parse("2024-06-15 14:30:45");

	EXPECT_EQ(2024, dt.year());
	EXPECT_EQ(6, dt.month());
	EXPECT_EQ(15, dt.day());
	EXPECT_EQ(14, dt.hour());
	EXPECT_EQ(30, dt.minute());
	EXPECT_EQ(45, dt.second());
}

TEST(DateTimeTest, ParseDateOnly) {
	datetime dt = datetime::parse("2024-12-25");

	EXPECT_EQ(2024, dt.year());
	EXPECT_EQ(12, dt.month());
	EXPECT_EQ(25, dt.day());
	EXPECT_EQ(0, dt.hour());
	EXPECT_EQ(0, dt.minute());
	EXPECT_EQ(0, dt.second());
}

TEST(DateTimeTest, ConstructFromString) {
	datetime dt{"2025-01-01 00:00:00"};  // New Year 2025

	EXPECT_EQ(2025, dt.year());
	EXPECT_EQ(1, dt.month());
	EXPECT_EQ(1, dt.day());
	EXPECT_EQ(0, dt.hour());
}

TEST(DateTimeTest, ParseEmptyString) {
	datetime dt = datetime::parse("");
	EXPECT_EQ(0, dt.timestamp());
}

TEST(DateTimeTest, ParseInvalidStringThrows) {
	EXPECT_THROW(datetime::parse("not a date"), std::invalid_argument);
	EXPECT_THROW(datetime::parse("2024/06/15"), std::invalid_argument);  // Wrong format
}

// ============================================================================
// Formatting Tests
// ============================================================================

TEST(DateTimeTest, FormatFullDateTime) {
	datetime dt{2024, 6, 15, 14, 30, 45};
	EXPECT_EQ("2024-06-15 14:30:45", dt.to_string());
}

TEST(DateTimeTest, FormatDateOnly) {
	datetime dt{2024, 12, 25};
	EXPECT_EQ("2024-12-25", dt.to_date_string());
}

TEST(DateTimeTest, FormatTimeOnly) {
	datetime dt{2024, 6, 15, 14, 30, 45};
	EXPECT_EQ("14:30:45", dt.to_time_string());
}

TEST(DateTimeTest, FormatWithLeadingZeros) {
	datetime dt{2024, 1, 5, 9, 8, 7};  // Single digit month, day, time
	EXPECT_EQ("2024-01-05 09:08:07", dt.to_string());
}

// ============================================================================
// Comparison Tests
// ============================================================================

TEST(DateTimeTest, EqualityComparison) {
	datetime dt1{2024, 6, 15, 12, 0, 0};
	datetime dt2{2024, 6, 15, 12, 0, 0};
	datetime dt3{2024, 6, 15, 13, 0, 0};

	EXPECT_TRUE(dt1 == dt2);
	EXPECT_FALSE(dt1 == dt3);
	EXPECT_TRUE(dt1 != dt3);
}

TEST(DateTimeTest, LessThanComparison) {
	datetime earlier{2024, 6, 15, 12, 0, 0};
	datetime later{2024, 6, 15, 13, 0, 0};

	EXPECT_TRUE(earlier < later);
	EXPECT_FALSE(later < earlier);
	EXPECT_TRUE(earlier <= later);
	EXPECT_TRUE(later > earlier);
	EXPECT_TRUE(later >= earlier);
}

// ============================================================================
// Static Method Tests
// ============================================================================

TEST(DateTimeTest, NowReturnsCurrentTime) {
	datetime now = datetime::now();
	std::time_t current = std::time(nullptr);

	// Should be within 1 second of each other
	EXPECT_NEAR(current, now.timestamp(), 1);
}

// ============================================================================
// Attribute Integration Tests
// ============================================================================

TEST(DateTimeAttributeTest, ConstructAttributeFromDateTime) {
	datetime dt{2024, 6, 15, 14, 30, 45};
	attribute attr{dt};

	EXPECT_EQ(type_date, attr.type());
	EXPECT_FALSE(attr.is_null());
}

TEST(DateTimeAttributeTest, RetrieveDateTimeFromAttribute) {
	datetime dt{2024, 12, 25, 18, 0, 0};
	attribute attr{dt};

	datetime retrieved = attr.as_date();
	EXPECT_EQ(2024, retrieved.year());
	EXPECT_EQ(12, retrieved.month());
	EXPECT_EQ(25, retrieved.day());
	EXPECT_EQ(18, retrieved.hour());
}

TEST(DateTimeAttributeTest, AttributeToString) {
	datetime dt{2024, 6, 15, 14, 30, 45};
	attribute attr{dt};

	EXPECT_EQ("2024-06-15 14:30:45", attr.to_string());
}

TEST(DateTimeAttributeTest, AttributeAsNumeric) {
	datetime dt{2024, 6, 15, 14, 30, 45};
	attribute attr{dt};

	int64_t timestamp = attr.as_numeric();
	EXPECT_EQ(dt.timestamp(), timestamp);
}

// ============================================================================
// Database-Style Usage Tests
// ============================================================================

TEST(DateTimeUsageTest, CreateAndRetrieveDate) {
	// Simulate storing a date
	datetime created_at = datetime::now();
	attribute attr{created_at};

	// Simulate retrieving from database
	datetime retrieved = attr.as_date();

	EXPECT_EQ(created_at.timestamp(), retrieved.timestamp());
}

TEST(DateTimeUsageTest, ParseDatabaseDateString) {
	// Simulate reading "2024-06-15 14:30:45" from database
	std::string db_value = "2024-06-15 14:30:45";
	datetime dt = datetime::parse(db_value);

	EXPECT_EQ(2024, dt.year());
	EXPECT_EQ(6, dt.month());
	EXPECT_EQ(15, dt.day());
	EXPECT_EQ(14, dt.hour());
	EXPECT_EQ(30, dt.minute());
	EXPECT_EQ(45, dt.second());
}

TEST(DateTimeUsageTest, FormatForDatabaseInsert) {
	datetime dt{2024, 6, 15, 14, 30, 45};

	// Format for database INSERT
	std::string sql_value = dt.to_string();
	EXPECT_EQ("2024-06-15 14:30:45", sql_value);

	// Verify round-trip
	datetime parsed = datetime::parse(sql_value);
	EXPECT_EQ(dt.timestamp(), parsed.timestamp());
}

TEST(DateTimeUsageTest, DateOnlyValues) {
	// Date column (no time component)
	datetime birthday{1990, 5, 15};

	EXPECT_EQ("1990-05-15", birthday.to_date_string());
	EXPECT_EQ("1990-05-15 00:00:00", birthday.to_string());
}
