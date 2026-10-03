// ============================================================================
// attribute.cpp
//
// SPDX-FileCopyrightText: 2020-2026 Indridi Bjornsson
// SPDX-License-Identifier: MIT
// See file LICENSE.MIT for detail
//

#include <activerecord/attribute.h>
#include <format>

namespace ar {

namespace {
std::string escape_json(std::string_view value) {
	std::string out;
	out.reserve(value.size());
	for (const char c : value) {
		switch (c) {
		case '\"':
			out += "\\\"";
			break;
		case '\\':
			out += "\\\\";
			break;
		case '\b':
			out += "\\b";
			break;
		case '\f':
			out += "\\f";
			break;
		case '\n':
			out += "\\n";
			break;
		case '\r':
			out += "\\r";
			break;
		case '\t':
			out += "\\t";
			break;
		default:
			if (static_cast<unsigned char>(c) < 0x20U) {
				const std::size_t uc{ static_cast<unsigned char>(c) };
				out += "\\u00";
				constexpr std::string_view hex{ "0123456789ABCDEF" };
				out += hex[(uc >> 4U) & 0x0FU];
				out += hex[uc & 0x0FU];
			} else {
				out += c;
			}
			break;
		}
	}
	return out;
}

std::string escape_xml(std::string_view value) {
	std::string out;
	out.reserve(value.size());
	for (const char c : value) {
		switch (c) {
		case '&':
			out += "&amp;";
			break;
		case '<':
			out += "&lt;";
			break;
		case '>':
			out += "&gt;";
			break;
		case '\"':
			out += "&quot;";
			break;
		case '\'':
			out += "&apos;";
			break;
		default:
			out += c;
			break;
		}
	}
	return out;
}
} // namespace

string_vector get_attrib_keys(const attribute_map& attributes, const string_vector& only,
							  const string_set& exclude) noexcept {
	string_vector attrib_names{ only };
	if (attrib_names.empty()) {
		// use all keys
		attrib_names.reserve(attributes.size());
		for (const auto& [key, value] : attributes) {
			if (exclude.find(key) == exclude.end()) {
				attrib_names.emplace_back(key);
			}
		}
	}
	return attrib_names;
}

std::string to_json_string(const attribute_map& attributes, const string_vector& only, const string_set& exclude) {
	std::string result{ "{" };
	string_vector attrib_keys{ get_attrib_keys(attributes, only, exclude) };

	for (const auto& name : attrib_keys) {
		auto iter{ attributes.find(name) };
		if (iter == attributes.end())
			continue;

		const auto& [key, value] = *iter;

		if (result.size() > 1) {
			result += ", ";
		}

		// Attribute names are expected to be valid column names, so only values are escaped.
		switch (value.type()) {
		case type_null:
			result += std::format("\"{}\": null", key);
			break;
		case type_bool:
			result += std::format("\"{}\": {}", key, value.as_bool() ? "true" : "false");
			break;
		case type_integer:
		case type_decimal: // no quotes needed
			result += std::format("\"{}\": {}", key, value.to_string());
			break;

		default:
			result += std::format(R"("{}": "{}")", key, escape_json(value.to_string()));
			break;
		}
	}
	result += "}";
	return result;
}

std::string to_xml_string(const attribute_map& attributes, std::string_view obj_name, const string_vector& only,
						  const string_set& exclude) {
	std::string result{ std::format("<{}>", obj_name) };
	string_vector attrib_keys{ get_attrib_keys(attributes, only, exclude) };

	for (const auto& name : attrib_keys) {
		auto iter{ attributes.find(name) };
		if (iter == attributes.end())
			continue;

		const auto& [key, value] = *iter;

		// Attribute names are expected to be valid column names, so only values are escaped.
		const std::string value_str{ escape_xml(value.to_string()) };
		if (value_str.empty()) {
			result += std::format("<{} />", key);
		} else {
			result += std::format("<{}>{}</{}>", key, value_str, key);
		}
	}
	result += std::format("</{}>", obj_name);
	return result;
}

} // namespace ar

namespace std {

std::string to_string(const ar::attribute_map& attributes) {
	std::string result;
	for (const auto& [name, value] : attributes) {
		if (!result.empty())
			result += " ,";
		result += std::format("{}: {}", name, value.to_string());
	}
	return result;
}

} // namespace std
