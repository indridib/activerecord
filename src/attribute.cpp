// ============================================================================
// attribute.cpp
//
// SPDX-FileCopyrightText: 2020-2026 Indridi Bjornsson
// SPDX-License-Identifier: MIT
// See file LICENSE.MIT for detail
//

#include <attribute.h>
#include <format>

namespace ar {

	string_vector get_attrib_keys(const attribute_map& attributes, const string_vector& only, const string_set& exclude) noexcept {
		string_vector attrib_names{ only };
		if (attrib_names.empty()) {
			// use all keys
			attrib_names.reserve(attributes.size());
			for(const auto& [key, value] : attributes) {
				if (exclude.find(key) == exclude.end()) {
					attrib_names.emplace_back(key);
				}
			}

		}
		return attrib_names;
	}



	std::string to_json_string(const attribute_map& attributes, const string_vector& only, const string_set& exclude) noexcept {
		std::string result {"{"};
		string_vector attrib_keys{ get_attrib_keys(attributes, only, exclude) };

		for(const auto& name : attrib_keys) {
			auto iter = attributes.find(name);
			if (iter == attributes.end())	continue;

			const auto& [key, value] = *iter;

			if (result.size() > 1) {
				result += ", ";
			}

			// FixMe: There should be no need to escape the name but we need to escape the value
			// That is for string
			switch(value.type()) {
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

				// case type_string:
				// case type_date:
				default:
					result += std::format("\"{}\": \"{}\"", key, value.to_string());
					break;
			}
		}
		result+= "}";
		return result;
	}

	std::string to_xml_string(const attribute_map& attributes, std::string_view obj_name, const string_vector& only, const string_set& exclude) noexcept {
		std::string result = std::format("<{}>", obj_name);
		string_vector attrib_keys{ get_attrib_keys(attributes, only, exclude) };

		for(const auto& name : attrib_keys) {
			auto iter = attributes.find(name);
			if (iter == attributes.end())	continue;

			const auto& [key, value] = *iter;

			// FixMe: There should be no need to escape the name but we need to escape the value
			std::string value_str { value.to_string() };
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

	std::string to_string(const ar::attribute_map& attributes) noexcept {
		std::string result;
		for(const auto& [name, value] : attributes) {
			if (!result.empty())	result += " ,";
			result += std::format("{}: {}", name, value.to_string());
		}
		return result;
	}

} // namespace std
