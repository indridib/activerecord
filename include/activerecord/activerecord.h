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
#include <format>
#include <utility>


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
			explicit activerecord(std::string_view msg) : std::runtime_error(std::string{msg}) {}

			template <typename... Args>
			requires (sizeof...(Args) > 0)
			activerecord(std::format_string<Args...> fmt, Args&&... args)
				: std::runtime_error(std::format(fmt, std::forward<Args>(args)...)) {}
		};

		class config : public activerecord {
		public:
			using activerecord::activerecord;
		};

		class readonly : public activerecord {
		public:
			using activerecord::activerecord;
		};
		class item_not_found : public activerecord {
		public:
			using activerecord::activerecord;
		};
		class attribute_not_found : public activerecord {
		public:
			using activerecord::activerecord;
		};
		class stale_record : public activerecord {
		public:
			using activerecord::activerecord;
		};

	} // namespace ex

} // namespace ar

