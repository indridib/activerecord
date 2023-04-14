// ============================================================================
// attribute.cpp
//
// SPDX-FileCopyrightText: 2020-2023 Indridi Bjornsson
// SPDX-License-Identifier: MIT
// See file LICENSE.MIT for detail
//

#include <attribute.h>

namespace ar {

	string_vector get_attrib_keys(const attribute_map& attributes, const string_vector& only, const string_set& exclude) noexcept {
		string_vector attrib_names{ only };
		if (attrib_names.empty()) {
			// use all keys
			attrib_names.reserve(attributes.size());
			for(const auto& attrib_iter : attributes) {
				if (exclude.find(attrib_iter.first) == exclude.end()) {
					attrib_names.emplace_back(attrib_iter.first);
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

			if (result.size() > 1) {
				result += ", ";
			}

			// FixMe: There should be no need to escape the name but we need to escape the value
			// That is for string
			result += "\"" + iter->first + "\": ";
			switch(iter->second.type()) {
				case type_null: 
					result += "null";
					break;
				case type_bool:
					result += (boost::get<bool>(iter->second) ? "true" : "false");
					break;
				case type_integer:
				case type_decimal: // no quotes needed
					result += iter->second.to_string();
					break;

				// case type_string:
				// case type_date:
				default:
					result += "\"" + iter->second.to_string() + "\"";
					break;

			}
		}
		result+= "}";
		return result;
	}

	std::string to_xml_string(const attribute_map& attributes, const std::string& obj_name, const string_vector& only, const string_set& exclude) noexcept {
		std::string result {"<" + obj_name + ">"};
		string_vector attrib_keys{ get_attrib_keys(attributes, only, exclude) };

		for(const auto& name : attrib_keys) {
			auto iter = attributes.find(name);
			if (iter == attributes.end())	continue;

			// FixMe: There should be no need to escape the name but we need to escape the value
			std::string value { iter->second.to_string() };
			if (value.empty()) {
				result += "<" + iter->first + " />";
			} else {
				result += "<" + iter->first + ">" + iter->second.to_string() + "</" + iter->first + ">";
			}
		}
		result += "</" + obj_name + ">";
		return result;
	}
	
} // namespace ar

namespace std {

	std::string to_string(const ar::attribute_map& attributes) noexcept {
		std::string result;
		for(const auto& iter : attributes) {
			if (!result.empty())	result += " ,";
			result += iter.first + ": " + iter.second.to_string();
		}
		return result;
	}

} // namespace std
