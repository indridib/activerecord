// ==============================================================================
// attribute_test.cpp
//
// SPDX-FileCopyrightText: 2020-2023 Indridi Bjornsson
// SPDX-License-Identifier: MIT
// See file LICENSE.MIT for detail
//
// Unit tests for the attribute class
// ==============================================================================

#include <activerecord.h>
#include <attribute.h>
#include <ctime>
#include <datetime.h>
#include <gtest/gtest.h>

using namespace ar;

// ==============================================================================
// Type Construction Tests
// ==============================================================================

TEST(AttributeTest, ConstructNull) {
	attribute attr;
	EXPECT_EQ(type_null, attr.type());
	EXPECT_TRUE(attr.is_null());
}

TEST(AttributeTest, ConstructNullptr) {
	attribute attr(nullptr);
	EXPECT_EQ(type_null, attr.type());
	EXPECT_TRUE(attr.is_null());
}

TEST(AttributeTest, ConstructInteger) {
	attribute attr(42);
	EXPECT_EQ(type_integer, attr.type());
	EXPECT_FALSE(attr.is_null());
	EXPECT_EQ(42, attr.as_integer());
}

TEST(AttributeTest, ConstructInt64) {
	int64_t value = 9223372036854775807LL;
	attribute attr(value);
	EXPECT_EQ(type_integer, attr.type());
	EXPECT_EQ(value, attr.as_integer());
}

TEST(AttributeTest, ConstructBool) {
	attribute attr_true(true);
	EXPECT_EQ(type_bool, attr_true.type());
	EXPECT_TRUE(attr_true.as_bool());

	attribute attr_false(false);
	EXPECT_EQ(type_bool, attr_false.type());
	EXPECT_FALSE(attr_false.as_bool());
}

TEST(AttributeTest, ConstructDouble) {
	attribute attr(3.14159);
	EXPECT_EQ(type_decimal, attr.type());
	EXPECT_DOUBLE_EQ(3.14159, attr.as_real());
}

TEST(AttributeTest, ConstructString) {
	std::string str = "Hello, World!";
	attribute attr(str);
	EXPECT_EQ(type_string, attr.type());
	EXPECT_EQ(str, attr.as_string());
}

TEST(AttributeTest, ConstructCString) {
	attribute attr("C String");
	EXPECT_EQ(type_string, attr.type());
	EXPECT_EQ("C String", attr.as_string());
}

TEST(AttributeTest, ConstructDate) {
	std::time_t now = std::time(nullptr);
	datetime dt{ now };
	attribute attr{ dt };
	EXPECT_EQ(type_date, attr.type());
	EXPECT_EQ(now, attr.as_date().timestamp());
}

// ==============================================================================
// Type Conversion Tests
// ==============================================================================

TEST(AttributeTest, IntegerToString) {
	attribute attr(123);
	EXPECT_EQ("123", attr.to_string());
}

TEST(AttributeTest, DoubleToString) {
	attribute attr(3.14);
	std::string result = attr.to_string();
	EXPECT_TRUE(result.find("3.14") == 0);
}

TEST(AttributeTest, BoolToString) {
	attribute attr_true(true);
	EXPECT_EQ("1", attr_true.to_string());

	attribute attr_false(false);
	EXPECT_EQ("0", attr_false.to_string());
}

TEST(AttributeTest, StringToString) {
	attribute attr("test string");
	EXPECT_EQ("test string", attr.to_string());
}

TEST(AttributeTest, NullToString) {
	attribute attr;
	EXPECT_EQ("", attr.to_string());
}

// ==============================================================================
// as_numeric() Tests
// ==============================================================================

TEST(AttributeTest, IntegerAsNumeric) {
	attribute attr(42);
	EXPECT_EQ(42, attr.as_numeric());
}

TEST(AttributeTest, DoubleAsNumeric) {
	attribute attr(3.7);
	EXPECT_EQ(3, attr.as_numeric()); // Truncates to int64_t
}

TEST(AttributeTest, BoolAsNumeric) {
	attribute attr_true(true);
	EXPECT_EQ(1, attr_true.as_numeric());

	attribute attr_false(false);
	EXPECT_EQ(0, attr_false.as_numeric());
}

TEST(AttributeTest, NullAsNumeric) {
	attribute attr;
	EXPECT_EQ(0, attr.as_numeric());
}

TEST(AttributeTest, StringAsNumeric) {
	attribute attr("12345");
	EXPECT_EQ(12345, attr.as_numeric());
}

TEST(AttributeTest, InvalidStringAsNumericThrows) {
	attribute attr("not a number");
	EXPECT_THROW(attr.as_numeric(), std::invalid_argument);
}

TEST(AttributeTest, EmptyStringAsNumericThrows) {
	attribute attr(""); // Creates empty string
	EXPECT_THROW(attr.as_numeric(), std::invalid_argument);
}

// ==============================================================================
// JSON Serialization Tests
// ==============================================================================

TEST(AttributeTest, AttributeMapToJson) {
	attribute_map attrs;
	attrs["id"] = attribute(1);
	attrs["name"] = attribute("John Doe");
	attrs["age"] = attribute(30);
	attrs["active"] = attribute(true);
	attrs["score"] = attribute(95.5);

	std::string json = to_json_string(attrs);

	// Verify it contains the expected fields
	EXPECT_NE(std::string::npos, json.find("\"id\""));
	EXPECT_NE(std::string::npos, json.find("\"name\""));
	EXPECT_NE(std::string::npos, json.find("\"John Doe\""));
	EXPECT_NE(std::string::npos, json.find("true"));
}

TEST(AttributeTest, AttributeMapToJsonWithOnly) {
	attribute_map attrs;
	attrs["id"] = attribute(1);
	attrs["name"] = attribute("John");
	attrs["email"] = attribute("john@example.com");

	string_vector only = { "id", "name" };
	std::string json = to_json_string(attrs, only);

	EXPECT_NE(std::string::npos, json.find("\"id\""));
	EXPECT_NE(std::string::npos, json.find("\"name\""));
	EXPECT_EQ(std::string::npos, json.find("email")); // Should not be included
}

TEST(AttributeTest, AttributeMapToJsonWithExclude) {
	attribute_map attrs;
	attrs["id"] = attribute(1);
	attrs["name"] = attribute("John");
	attrs["password"] = attribute("secret");

	string_set exclude = { "password" };
	std::string json = to_json_string(attrs, {}, exclude);

	EXPECT_NE(std::string::npos, json.find("\"id\""));
	EXPECT_NE(std::string::npos, json.find("\"name\""));
	EXPECT_EQ(std::string::npos, json.find("password")); // Should be excluded
}

TEST(AttributeTest, AttributeMapToJsonEscapesSpecialCharacters) {
	attribute_map attrs;
	attrs["name"] = attribute("A\n\"B\"\\C");

	std::string json = to_json_string(attrs);

	EXPECT_NE(std::string::npos, json.find("\"name\""));
	EXPECT_NE(std::string::npos, json.find("A\\n\\\"B\\\"\\\\C"));
}

// ==============================================================================
// XML Serialization Tests
// ==============================================================================

TEST(AttributeTest, AttributeMapToXml) {
	attribute_map attrs;
	attrs["id"] = attribute(1);
	attrs["name"] = attribute("John Doe");

	std::string xml = to_xml_string(attrs, "user");

	EXPECT_NE(std::string::npos, xml.find("<user>"));
	EXPECT_NE(std::string::npos, xml.find("</user>"));
	EXPECT_NE(std::string::npos, xml.find("<id>1</id>"));
	EXPECT_NE(std::string::npos, xml.find("<name>John Doe</name>"));
}

TEST(AttributeTest, AttributeMapToXmlWithExclude) {
	attribute_map attrs;
	attrs["id"] = attribute(1);
	attrs["name"] = attribute("John");
	attrs["password"] = attribute("secret");

	string_set exclude = { "password" };
	std::string xml = to_xml_string(attrs, "user", {}, exclude);

	EXPECT_NE(std::string::npos, xml.find("<id>1</id>"));
	EXPECT_EQ(std::string::npos, xml.find("password")); // Should be excluded
}

TEST(AttributeTest, AttributeMapToXmlEscapesSpecialCharacters) {
	attribute_map attrs;
	attrs["name"] = attribute("a<&\"'b");

	std::string xml = to_xml_string(attrs, "user");

	EXPECT_NE(std::string::npos, xml.find("<name>"));
	EXPECT_NE(std::string::npos, xml.find("a&lt;&amp;&quot;&apos;b"));
}

// ==============================================================================
// Edge Cases
// ==============================================================================

TEST(AttributeTest, EmptyString) {
	attribute attr("");
	EXPECT_EQ(type_string, attr.type());
	EXPECT_EQ("", attr.as_string());
}

TEST(AttributeTest, NegativeInteger) {
	attribute attr(-42);
	EXPECT_EQ(type_integer, attr.type());
	EXPECT_EQ(-42, attr.as_integer());
}

TEST(AttributeTest, ZeroInteger) {
	attribute attr(0);
	EXPECT_EQ(type_integer, attr.type());
	EXPECT_EQ(0, attr.as_integer());
}

TEST(AttributeTest, ZeroDouble) {
	attribute attr(0.0);
	EXPECT_EQ(type_decimal, attr.type());
	EXPECT_DOUBLE_EQ(0.0, attr.as_real());
}
