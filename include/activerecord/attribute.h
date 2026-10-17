// ============================================================================
// attribute.h
//
// SPDX-FileCopyrightText: 2020-2026 Indridi Bjornsson
// SPDX-License-Identifier: MIT
// See file LICENSE for details
//
// Dynamic attribute value (string, integer, float, bool, datetime or null) and its conversions
//

#pragma once

#include <activerecord/activerecord.h>
#include <activerecord/datetime.h>
#include <concepts>
#include <map>
#include <variant>

namespace ar {

// NOTE: PHP Active Record defines the following types:
//	STRING, INTEGER, DECIMAL, DATETIME, DATE, TIME
// NOLINTNEXTLINE(performance-enum-size) -- public unscoped enum; a uint8_t base would print as a char via ostream/gtest
enum attribute_type {
	type_null = 0,
	type_string = 1,
	type_integer = 2,
	type_decimal = 3,
	type_bool = 4,
	type_date = 5
};

// Variant storage for attributes - index matches attribute_type enum
using attribute_storage = std::variant<std::monostate, // 0 = type_null
									   std::string,	   // 1 = type_string
									   int64_t,		   // 2 = type_integer
									   double,		   // 3 = type_decimal
									   bool,		   // 4 = type_bool
									   datetime		   // 5 = type_date
									   >;

/**
 * a single attribute can be of a different type and can have different types of values
 */
class attribute {
  public:
	attribute() : data_(std::monostate{}) {}
	attribute(bool value) : data_(value) {}
	attribute(int64_t value) : data_(value) {}
	attribute(int value) : data_(static_cast<int64_t>(value)) {}
	// Whichever of long / long long is not int64_t on this platform (LLP64 vs LP64)
	template <typename I>
		requires((std::same_as<I, long> || std::same_as<I, long long>) && !std::same_as<I, int64_t>)
	attribute(I value) : data_(static_cast<int64_t>(value)) {}
	attribute(const std::string& value) : data_(value) {}
	attribute(std::string_view value) : data_(std::string(value)) {}
	attribute(const char* value) {
		if (value != nullptr) {
			data_ = std::string{ value };
		} else {
			data_ = std::monostate{};
		}
	}
	attribute(double value) : data_(value) {}
	attribute(const datetime& value) : data_(value) {}

	attribute_type type() const { return static_cast<attribute_type>(data_.index()); }

	bool is_null() const { return std::holds_alternative<std::monostate>(data_); }

	std::string as_string() const { return std::get<std::string>(data_); }
	bool as_bool() const { return std::get<bool>(data_); }
	int64_t as_integer() const { return std::get<int64_t>(data_); }
	double as_real() const { return std::get<double>(data_); }
	datetime as_date() const {
		// Handle datetime, integer (timestamp from DB), and string
		if (std::holds_alternative<datetime>(data_)) {
			return std::get<datetime>(data_);
		} else if (std::holds_alternative<int64_t>(data_)) {
			// Database stored as Unix timestamp
			return datetime{ static_cast<std::time_t>(std::get<int64_t>(data_)) };
		} else if (std::holds_alternative<std::string>(data_)) {
			// Parse from string (e.g., "2024-06-15 14:30:00")
			return datetime::parse(std::get<std::string>(data_));
		}
		return datetime{}; // Return empty datetime for other types
	}

	/**
	 * Returns a numeric value of the attribute if possible
	 * Value types that can be converted to numeric are: null, bool, integer, double, date
	 * @throws std::invalid_argument if it is unable to convert the value
	 */
	int64_t as_numeric() const {
		switch (type()) {
		case type_null:
			return 0;
		case type_string: {
			const auto& str = std::get<std::string>(data_);
			if (str.empty())
				throw std::invalid_argument("Unable to convert empty string attribute to numeric");
			return std::stoll(str);
		}
		case type_integer:
			return std::get<int64_t>(data_);
		case type_decimal:
			return static_cast<int64_t>(std::get<double>(data_));
		case type_bool:
			return (std::get<bool>(data_) ? 1 : 0);
		case type_date:
			return static_cast<int64_t>(std::get<datetime>(data_).timestamp());
		}
		throw std::invalid_argument("Unable to convert attribute to numeric");
	}

	/**
	 * Returns a string version of the attribute.
	 * @throws std::invalid_argument if it is unable to convert the value
	 */
	std::string to_string() const {
		switch (type()) {
		case type_null:
			return "";
		case type_string:
			return std::get<std::string>(data_);
		case type_integer:
			return std::to_string(std::get<int64_t>(data_));
		case type_decimal:
			return std::to_string(std::get<double>(data_));
		case type_bool:
			return std::to_string(static_cast<int>(std::get<bool>(data_)));
		case type_date:
			return std::get<datetime>(data_).to_string();
		}
		throw std::invalid_argument("Unable to convert attribute to string");
	}

  private:
	attribute_storage data_;
};

using attribute_map = std::map<std::string, attribute>;
using attribute_vector = std::vector<attribute>;

/**
 * Return all keys from attributes. If only is defined contains only those keys else we will select all keys
 * excluding those defined in excluded.
 *
 * @param attributes - attributes to use.
 * @param only - if not empty names of attributes that will be included.
 * @param exclude - if not empty names of attributes that will be excluded.
 * @return - vector of keys to use.
 */
string_vector get_attrib_keys(const attribute_map& attributes, const string_vector& only = {},
							  const string_set& exclude = {});

/**
 * Return attributes as JSON string.
 *
 * @param attributes - attributes to use.
 * @param only - if not empty names of attributes that will be included.
 * @param exclude - if not empty names of attributes that will be excluded.
 * @return - string JSON representation of the attributes.
 */
std::string to_json_string(const attribute_map& attributes, const string_vector& only = {},
						   const string_set& excluded = {});

/**
 * Return attributes as XML string.
 *
 * @param attributes - attributes to use.
 * @param only - if not empty names of attributes that will be included.
 * @param exclude - if not empty names of attributes that will be excluded.
 * @return - string XML representation of the attributes.
 */
std::string to_xml_string(const attribute_map& attributes, std::string_view obj_name, const string_vector& only = {},
						  const string_set& exclude = {});

} // namespace ar

namespace std {
std::string to_string(const ar::attribute_map& attributes);
inline std::string to_string(const ar::attribute& attrib) {
	return attrib.to_string();
}
} // namespace std
