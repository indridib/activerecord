// ============================================================================
// validations.h
//
// SPDX-FileCopyrightText: 2020-2026 Indridi Bjornsson
// SPDX-License-Identifier: MIT
// See file LICENSE.MIT for detail
//

#pragma once

#include <attribute.h>
#include <errors.h>
#include <optional>
#include <regex>
#include <tuple>

namespace ar {

struct presence_rule {
	std::string_view attribute{};

	template <class Record> void validate(const Record& record, errors& validation_errors) const {
		const auto& attrs = record.attributes();
		const auto iter = attrs.find(std::string(attribute));
		if (iter == attrs.end() || iter->second.is_null()) {
			validation_errors.add(attribute, "can't be blank");
			return;
		}
		if (iter->second.type() == type_string && iter->second.as_string().empty()) {
			validation_errors.add(attribute, "can't be blank");
		}
	}
};

struct length_options {
	std::optional<std::size_t> min{};
	std::optional<std::size_t> max{};
};

struct length_rule {
	std::string_view attribute{};
	length_options options{};

	template <class Record> void validate(const Record& record, errors& validation_errors) const {
		const auto& attrs = record.attributes();
		const auto iter = attrs.find(std::string(attribute));
		if (iter == attrs.end() || iter->second.is_null()) {
			return;
		}
		if (iter->second.type() != type_string) {
			return;
		}
		const auto len = iter->second.as_string().size();
		if (options.min && len < *options.min) {
			validation_errors.add(attribute, std::format("is too short (minimum is {})", *options.min));
		}
		if (options.max && len > *options.max) {
			validation_errors.add(attribute, std::format("is too long (maximum is {})", *options.max));
		}
	}
};

struct numericality_options {
	std::optional<int64_t> greater_than{};
	std::optional<int64_t> greater_than_or_equal_to{};
	std::optional<int64_t> less_than{};
	std::optional<int64_t> less_than_or_equal_to{};
	std::optional<int64_t> equal_to{};
	bool only_integer{ false };
};

struct numericality_rule {
	std::string_view attribute{};
	numericality_options options{};

	template <class Record> void validate(const Record& record, errors& validation_errors) const {
		const auto& attrs = record.attributes();
		const auto iter = attrs.find(std::string(attribute));
		if (iter == attrs.end() || iter->second.is_null()) {
			return;
		}

		const auto& value = iter->second;
		if (options.only_integer && value.type() == type_decimal) {
			validation_errors.add(attribute, "must be an integer");
			return;
		}

		int64_t numeric_value{ 0 };
		try {
			numeric_value = value.as_numeric();
		} catch (const std::exception&) {
			validation_errors.add(attribute, "is not a number");
			return;
		}

		if (options.greater_than && numeric_value <= *options.greater_than) {
			validation_errors.add(attribute, std::format("must be greater than {}", *options.greater_than));
		}
		if (options.greater_than_or_equal_to && numeric_value < *options.greater_than_or_equal_to) {
			validation_errors.add(
				attribute, std::format("must be greater than or equal to {}", *options.greater_than_or_equal_to));
		}
		if (options.less_than && numeric_value >= *options.less_than) {
			validation_errors.add(attribute, std::format("must be less than {}", *options.less_than));
		}
		if (options.less_than_or_equal_to && numeric_value > *options.less_than_or_equal_to) {
			validation_errors.add(attribute,
								  std::format("must be less than or equal to {}", *options.less_than_or_equal_to));
		}
		if (options.equal_to && numeric_value != *options.equal_to) {
			validation_errors.add(attribute, std::format("must be equal to {}", *options.equal_to));
		}
	}
};

struct format_rule {
	std::string_view attribute{};
	std::regex pattern{};
	std::string message{ "is invalid" };

	template <class Record> void validate(const Record& record, errors& validation_errors) const {
		const auto& attrs = record.attributes();
		const auto iter = attrs.find(std::string(attribute));
		if (iter == attrs.end() || iter->second.is_null()) {
			return;
		}
		const auto value = (iter->second.type() == type_string) ? iter->second.as_string() : iter->second.to_string();
		if (!std::regex_match(value, pattern)) {
			validation_errors.add(attribute, message);
		}
	}
};

constexpr auto presence(std::string_view attribute) {
	return presence_rule{ attribute };
}

constexpr auto length(std::string_view attribute, length_options options) {
	return length_rule{ attribute, options };
}

constexpr auto numericality(std::string_view attribute, numericality_options options) {
	return numericality_rule{ attribute, options };
}

inline auto format(std::string_view attribute, const std::regex& pattern, std::string message = "is invalid") {
	return format_rule{ attribute, pattern, std::move(message) };
}

template <class... Rules> constexpr auto validate(Rules... rules) {
	return std::tuple<Rules...>{ rules... };
}

namespace detail {

template <class Record, class RuleTuple>
void run_validation_rules(const Record& record, errors& out, RuleTuple&& rules) {
	std::apply([&](const auto&... rule) { (rule.validate(record, out), ...); }, std::forward<RuleTuple>(rules));
}

} // namespace detail

} // namespace ar
