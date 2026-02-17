// ============================================================================
// datetime.h
//
// SPDX-FileCopyrightText: 2020-2026 Indridi Bjornsson
// SPDX-License-Identifier: MIT
// See file LICENSE.MIT for detail
//
// Date/time handling for ActiveRecord
// ============================================================================

#pragma once

#include <activerecord.h>
#include <ctime>
#include <string>

namespace ar {

/**
 * Simple date/time class for database date/time values
 *
 * Supports:
 * - Construction from time_t, strings, or individual components
 * - Formatting to database-friendly strings
 * - Parsing from common date/time formats
 */
class datetime {
  public:
	// Constructors
	datetime() : timestamp_(0) {}
	datetime(std::time_t t) : timestamp_(t) {}
	datetime(const std::string& date_str);
	datetime(int year, int month, int day, int hour = 0, int min = 0, int sec = 0);

	// Accessors
	std::time_t timestamp() const { return timestamp_; }

	// Formatting
	std::string to_string() const;		// Format as "YYYY-MM-DD HH:MM:SS"
	std::string to_date_string() const; // Format as "YYYY-MM-DD"
	std::string to_time_string() const; // Format as "HH:MM:SS"

	// Components
	int year() const;
	int month() const;
	int day() const;
	int hour() const;
	int minute() const;
	int second() const;

	// Static helpers
	static datetime now();						   // Current date/time
	static datetime parse(const std::string& str); // Parse from string

	// Comparison operators
	bool operator==(const datetime& other) const { return timestamp_ == other.timestamp_; }
	bool operator!=(const datetime& other) const { return timestamp_ != other.timestamp_; }
	bool operator<(const datetime& other) const { return timestamp_ < other.timestamp_; }
	bool operator<=(const datetime& other) const { return timestamp_ <= other.timestamp_; }
	bool operator>(const datetime& other) const { return timestamp_ > other.timestamp_; }
	bool operator>=(const datetime& other) const { return timestamp_ >= other.timestamp_; }

  private:
	std::time_t timestamp_;
};

} // namespace ar
