// ============================================================================
// errors.h
//
// SPDX-FileCopyrightText: 2020-2026 Indridi Bjornsson
// SPDX-License-Identifier: MIT
// See file LICENSE.MIT for detail
//

#pragma once

#include <activerecord.h>
#include <map>

namespace ar {

class errors {
  public:
	void add(std::string_view attribute, std::string_view message) {
		messages_[std::string(attribute)].emplace_back(message);
	}

	void clear() noexcept { messages_.clear(); }

	bool empty() const noexcept { return messages_.empty(); }

	std::size_t count() const noexcept {
		std::size_t total{ 0 };
		for (const auto& [_, messages] : messages_) {
			total += messages.size();
		}
		return total;
	}

	const string_vector& on(std::string_view attribute) const noexcept {
		const auto iter = messages_.find(std::string(attribute));
		if (iter == messages_.end()) {
			return empty_messages_;
		}
		return iter->second;
	}

	string_vector full_messages() const {
		string_vector result;
		result.reserve(count());
		for (const auto& [attribute, messages] : messages_) {
			for (const auto& message : messages) {
				if (attribute.empty()) {
					result.emplace_back(message);
				} else {
					result.emplace_back(std::format("{} {}", attribute, message));
				}
			}
		}
		return result;
	}

  private:
	std::map<std::string, string_vector> messages_{};
	inline static const string_vector empty_messages_{};
};

} // namespace ar
