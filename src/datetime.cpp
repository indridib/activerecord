// ============================================================================
// datetime.cpp
//
// SPDX-FileCopyrightText: 2020-2026 Indridi Bjornsson
// SPDX-License-Identifier: MIT
// See file LICENSE.MIT for detail
//
// Date/time implementation
// ============================================================================

#include <datetime.h>
#include <format>
#include <cstring>
#include <stdexcept>

// Platform-specific includes for thread-safe time functions
#ifdef _WIN32
#include <time.h>
#else
#include <time.h>
#endif

namespace ar {

// Thread-safe helper to convert time_t to tm structure
static bool safe_localtime(const std::time_t* timer, std::tm* buf) {
#ifdef _WIN32
	// Windows: use localtime_s (arguments reversed compared to POSIX)
	return localtime_s(buf, timer) == 0;
#else
	// POSIX: use localtime_r
	return localtime_r(timer, buf) != nullptr;
#endif
}

datetime::datetime(const std::string& date_str) {
	*this = parse(date_str);
}

datetime::datetime(int year, int month, int day, int hour, int min, int sec) {
	std::tm timeinfo{};
	timeinfo.tm_year = year - 1900;  // Years since 1900
	timeinfo.tm_mon = month - 1;     // Months since January (0-11)
	timeinfo.tm_mday = day;
	timeinfo.tm_hour = hour;
	timeinfo.tm_min = min;
	timeinfo.tm_sec = sec;
	timeinfo.tm_isdst = -1;          // Let mktime determine DST

	timestamp_ = std::mktime(&timeinfo);
	if (timestamp_ == -1) {
		throw std::invalid_argument("Invalid date/time components");
	}
}

std::string datetime::to_string() const {
	if (timestamp_ == 0) {
		return "";
	}

	std::tm timeinfo{};
	if (!safe_localtime(&timestamp_, &timeinfo)) {
		return "";
	}

	return std::format("{:04d}-{:02d}-{:02d} {:02d}:{:02d}:{:02d}",
		timeinfo.tm_year + 1900,
		timeinfo.tm_mon + 1,
		timeinfo.tm_mday,
		timeinfo.tm_hour,
		timeinfo.tm_min,
		timeinfo.tm_sec);
}

std::string datetime::to_date_string() const {
	if (timestamp_ == 0) {
		return "";
	}

	std::tm timeinfo{};
	if (!safe_localtime(&timestamp_, &timeinfo)) {
		return "";
	}

	return std::format("{:04d}-{:02d}-{:02d}",
		timeinfo.tm_year + 1900,
		timeinfo.tm_mon + 1,
		timeinfo.tm_mday);
}

std::string datetime::to_time_string() const {
	if (timestamp_ == 0) {
		return "";
	}

	std::tm timeinfo{};
	if (!safe_localtime(&timestamp_, &timeinfo)) {
		return "";
	}

	return std::format("{:02d}:{:02d}:{:02d}",
		timeinfo.tm_hour,
		timeinfo.tm_min,
		timeinfo.tm_sec);
}

int datetime::year() const {
	std::tm timeinfo{};
	return safe_localtime(&timestamp_, &timeinfo) ? timeinfo.tm_year + 1900 : 0;
}

int datetime::month() const {
	std::tm timeinfo{};
	return safe_localtime(&timestamp_, &timeinfo) ? timeinfo.tm_mon + 1 : 0;
}

int datetime::day() const {
	std::tm timeinfo{};
	return safe_localtime(&timestamp_, &timeinfo) ? timeinfo.tm_mday : 0;
}

int datetime::hour() const {
	std::tm timeinfo{};
	return safe_localtime(&timestamp_, &timeinfo) ? timeinfo.tm_hour : 0;
}

int datetime::minute() const {
	std::tm timeinfo{};
	return safe_localtime(&timestamp_, &timeinfo) ? timeinfo.tm_min : 0;
}

int datetime::second() const {
	std::tm timeinfo{};
	return safe_localtime(&timestamp_, &timeinfo) ? timeinfo.tm_sec : 0;
}

datetime datetime::now() {
	return datetime{std::time(nullptr)};
}

datetime datetime::parse(const std::string& str) {
	if (str.empty()) {
		return datetime{};
	}

	std::tm timeinfo{};
	int year{0}, month{0}, day{0}, hour{0}, min{0}, sec{0};

	// Try parsing "YYYY-MM-DD HH:MM:SS"
	if (std::sscanf(str.c_str(), "%d-%d-%d %d:%d:%d",
		&year, &month, &day, &hour, &min, &sec) == 6) {
		// Validate parsed values
		if (year < 1970 || year > 9999) {
			throw std::invalid_argument(std::format("Invalid year in date/time string: '{}' (year={})", str, year));
		}
		if (month < 1 || month > 12) {
			throw std::invalid_argument(std::format("Invalid month in date/time string: '{}' (month={})", str, month));
		}
		if (day < 1 || day > 31) {
			throw std::invalid_argument(std::format("Invalid day in date/time string: '{}' (day={})", str, day));
		}
		if (hour < 0 || hour > 23) {
			throw std::invalid_argument(std::format("Invalid hour in date/time string: '{}' (hour={})", str, hour));
		}
		if (min < 0 || min > 59) {
			throw std::invalid_argument(std::format("Invalid minute in date/time string: '{}' (min={})", str, min));
		}
		if (sec < 0 || sec > 59) {
			throw std::invalid_argument(std::format("Invalid second in date/time string: '{}' (sec={})", str, sec));
		}
		return datetime{year, month, day, hour, min, sec};
	}

	// Try parsing "YYYY-MM-DD"
	if (std::sscanf(str.c_str(), "%d-%d-%d", &year, &month, &day) == 3) {
		// Validate parsed values
		if (year < 1970 || year > 9999) {
			throw std::invalid_argument(std::format("Invalid year in date string: '{}' (year={})", str, year));
		}
		if (month < 1 || month > 12) {
			throw std::invalid_argument(std::format("Invalid month in date string: '{}' (month={})", str, month));
		}
		if (day < 1 || day > 31) {
			throw std::invalid_argument(std::format("Invalid day in date string: '{}' (day={})", str, day));
		}
		return datetime{year, month, day};
	}

	throw std::invalid_argument(std::format("Unable to parse date/time string: '{}'", str));
}

} // namespace ar
