// ============================================================================
// activerecord.h
//
// SPDX-FileCopyrightText: 2020-2023 Indridi Bjornsson
// SPDX-License-Identifier: MIT
// See file LICENSE.MIT for detail
//
// Contains general definitions for the activerecord project.
// 
#pragma once

#include <string>
#include <string_view>
#include <vector>
#include <set>


#include <stdexcept>

namespace ar {

	typedef std::vector<std::string>	string_vector;
	typedef std::set<std::string>		string_set;

	namespace ex {

		/**
		 * base exception that all other exceptions thrown should be based on
		 **/
		class activerecord : public std::runtime_error {
		public:
			activerecord(const std::string& msg) : std::runtime_error(msg) {}
		};

		class config : public activerecord {
		public:
			config(const std::string& msg) : activerecord(msg) {}
		};

		class readonly : public activerecord {
		public:
			readonly(const std::string& msg) : activerecord(msg) {}
		};
		class item_not_found : public activerecord {
		public:
			item_not_found(const std::string& msg) : activerecord(msg) {}
		};
		class attribute_not_found : public activerecord {
		public:
			attribute_not_found(const std::string& msg) : activerecord(msg) {}
		};

	} // namespace ex

} // namespace ar

