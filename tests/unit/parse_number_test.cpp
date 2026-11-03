// ============================================================================
// parse_number_test.cpp
//
// SPDX-FileCopyrightText: 2020-2026 Indridi Bjornsson
// SPDX-License-Identifier: MIT
// See file LICENSE for details
//
// Unit tests for the adapters' internal number parser
//

#include "../../src/adapters/parse_number.h"

#include <cmath>
#include <gtest/gtest.h>
#include <limits>

using ar::detail::parse_double;

TEST(ParseNumberTest, FiniteValuesAndExponents) {
	EXPECT_EQ(1.234567891, parse_double("1.234567891"));
	EXPECT_EQ(-98765.4321, parse_double("-98765.4321"));
	EXPECT_EQ(1e-10, parse_double("1e-10"));
	EXPECT_EQ(1e20, parse_double("1E+20"));
	EXPECT_EQ(1.234567891, parse_double("1.234567891E0")); // SQL Server's float-to-text form
	EXPECT_EQ(std::numeric_limits<double>::max(), parse_double("1.7976931348623157e308"));
}

TEST(ParseNumberTest, SubnormalAndSignedZero) {
	EXPECT_EQ(std::numeric_limits<double>::denorm_min(), parse_double("5e-324"));
	EXPECT_EQ(-std::numeric_limits<double>::denorm_min(), parse_double("-5e-324"));
	EXPECT_TRUE(std::signbit(parse_double("-0")));
	EXPECT_FALSE(std::signbit(parse_double("0")));
}

TEST(ParseNumberTest, PostgresSpecialValues) {
	EXPECT_EQ(std::numeric_limits<double>::infinity(), parse_double("Infinity"));
	EXPECT_EQ(-std::numeric_limits<double>::infinity(), parse_double("-Infinity"));
	EXPECT_TRUE(std::isnan(parse_double("NaN")));
}

TEST(ParseNumberTest, RejectsWhatIsNotExactlyANumber) {
	EXPECT_THROW(parse_double(""), ar::ex::activerecord);
	EXPECT_THROW(parse_double("abc"), ar::ex::activerecord);
	EXPECT_THROW(parse_double("1.5x"), ar::ex::activerecord);
	EXPECT_THROW(parse_double(" 1.5"), ar::ex::activerecord); // whitespace is not trimmed
	EXPECT_THROW(parse_double("1.5 "), ar::ex::activerecord);
	EXPECT_THROW(parse_double("+1.5"), ar::ex::activerecord);  // from_chars accepts no leading '+'
	EXPECT_THROW(parse_double("1e400"), ar::ex::activerecord); // out of range
}
