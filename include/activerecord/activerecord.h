// ============================================================================
// activerecord.h
//
// SPDX-FileCopyrightText: 2020-2026 Indridi Bjornsson
// SPDX-License-Identifier: MIT
// See file LICENSE for details
//
// Contains general definitions for the activerecord project.
//
#pragma once

#include <format>
#include <set>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace ar {

using string_vector = std::vector<std::string>;
using string_set = std::set<std::string>;

namespace ex {

/**
 * base exception that all other exceptions thrown should be based on
 **/
class activerecord : public std::runtime_error {
  public:
	explicit activerecord(std::string_view msg) : std::runtime_error(std::string{ msg }) {}

	template <typename... Args>
		requires(sizeof...(Args) > 0)
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
