// ============================================================================
// parse_number.h
//
// SPDX-FileCopyrightText: 2020-2026 Indridi Bjornsson
// SPDX-License-Identifier: MIT
// See file LICENSE for details
//
// Number parsing for adapters that read numeric columns as text
//

#pragma once

#include <activerecord/activerecord.h>
#include <charconv>
#include <string_view>
#include <system_error>

namespace ar::detail {

/**
 * Parses the text form of a floating-point column. Unlike std::stod it accepts subnormal values
 * (e.g. 5e-324) and does not depend on the locale. Throws ex::activerecord if text is not a number.
 */
inline double parse_double(std::string_view text) {
	double value{ 0.0 };
	const auto [end, ec] = std::from_chars(text.data(), text.data() + text.size(), value);
	if (ec != std::errc{} || end != text.data() + text.size()) {
		throw ex::activerecord("Cannot read \"{}\" as a floating-point value", text);
	}
	return value;
}

} // namespace ar::detail
