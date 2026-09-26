// ============================================================================
// typed_model.h
//
// SPDX-FileCopyrightText: 2020-2026 Indridi Bjornsson
// SPDX-License-Identifier: MIT
// See file LICENSE.MIT for detail
//

#pragma once

#include <attribute.h>
#include <model.h>
#include <optional>
#include <tuple>
#include <type_traits>
#include <validations.h>

namespace ar {

template <class MemberPtr> struct member_pointer_traits;

template <class Owner, class Member> struct member_pointer_traits<Member Owner::*> {
	using owner_type = Owner;
	using member_type = Member;
};

template <auto MemberPtr> struct column_def {
	using traits = member_pointer_traits<decltype(MemberPtr)>;
	using owner_type = typename traits::owner_type;
	using member_type = typename traits::member_type;

	const char* name;
	static constexpr auto member_ptr = MemberPtr;
};

template <auto MemberPtr> constexpr auto col(const char* name) {
	return column_def<MemberPtr>{ name };
}

template <class... Columns> constexpr auto columns(Columns... cols) {
	return std::tuple<Columns...>{ cols... };
}

namespace detail {

template <class T> struct is_optional : std::false_type {};
template <class T> struct is_optional<std::optional<T>> : std::true_type {};

template <class T> constexpr bool is_optional_v = is_optional<T>::value;

inline bool attribute_equal(const attribute& lhs, const attribute& rhs) {
	if (lhs.type() != rhs.type()) {
		return false;
	}
	switch (lhs.type()) {
	case type_null:
		return true;
	case type_string:
		return lhs.as_string() == rhs.as_string();
	case type_integer:
		return lhs.as_integer() == rhs.as_integer();
	case type_decimal:
		return lhs.as_real() == rhs.as_real();
	case type_bool:
		return lhs.as_bool() == rhs.as_bool();
	case type_date:
		return lhs.as_date().timestamp() == rhs.as_date().timestamp();
	}
	return false;
}

template <class FieldType> FieldType from_attribute(const attribute& value) {
	using raw_type = std::remove_cvref_t<FieldType>;

	if constexpr (is_optional_v<raw_type>) {
		using value_type = typename raw_type::value_type;
		if (value.is_null()) {
			return std::nullopt;
		}
		return from_attribute<value_type>(value);
	} else if constexpr (std::is_same_v<raw_type, std::string>) {
		if (value.type() == type_string) {
			return value.as_string();
		}
		return value.to_string();
	} else if constexpr (std::is_same_v<raw_type, bool>) {
		if (value.type() == type_bool) {
			return value.as_bool();
		}
		return value.as_numeric() != 0;
	} else if constexpr (std::is_enum_v<raw_type>) {
		using underlying_type = std::underlying_type_t<raw_type>;
		return static_cast<raw_type>(static_cast<underlying_type>(value.as_numeric()));
	} else if constexpr (std::is_integral_v<raw_type>) {
		return static_cast<raw_type>(value.as_numeric());
	} else if constexpr (std::is_floating_point_v<raw_type>) {
		if (value.type() == type_decimal) {
			return static_cast<raw_type>(value.as_real());
		}
		return static_cast<raw_type>(value.as_numeric());
	} else if constexpr (std::is_same_v<raw_type, datetime>) {
		return value.as_date();
	} else {
		static_assert(!sizeof(raw_type), "Unsupported typed_model field type");
	}
}

template <class FieldType> attribute to_attribute(const FieldType& value) {
	using raw_type = std::remove_cvref_t<FieldType>;

	if constexpr (is_optional_v<raw_type>) {
		if (!value.has_value()) {
			return attribute{};
		}
		return to_attribute(*value);
	} else if constexpr (std::is_same_v<raw_type, std::string> || std::is_same_v<raw_type, bool> ||
						 std::is_same_v<raw_type, datetime>) {
		// Types attribute stores directly
		return attribute{ value };
	} else if constexpr (std::is_enum_v<raw_type>) {
		using underlying_type = std::underlying_type_t<raw_type>;
		return attribute{ static_cast<int64_t>(static_cast<underlying_type>(value)) };
	} else if constexpr (std::is_integral_v<raw_type>) {
		return attribute{ static_cast<int64_t>(value) };
	} else if constexpr (std::is_floating_point_v<raw_type>) {
		return attribute{ static_cast<double>(value) };
	} else {
		static_assert(!sizeof(raw_type), "Unsupported typed_model field type");
	}
}

} // namespace detail

template <class T, bool TrackRecordMetadata = true> class typed_model : public model {
  public:
	static constexpr std::string_view st_table_name() noexcept {
		static_assert(requires { T::table_name; }, "typed_model requires T::table_name");
		return T::table_name;
	}

	static constexpr std::string_view st_primary_key() noexcept {
		static_assert(requires { T::primary_key; }, "typed_model requires T::primary_key");
		return T::primary_key;
	}

	typed_model() : model(st_table_name(), st_primary_key()) {
		this->set_record_tracking_policy(TrackRecordMetadata);
	}

	bool save(bool should_validate = true) {
		sync_to_attributes();
		if (should_validate && !run_validations()) {
			return false;
		}
		const bool saved{ model::save(false) };
		if (saved) {
			// Pull adapter/model-managed values (e.g. insert id, timestamps, version)
			// back into typed members, then refresh snapshot.
			sync_from_attributes();
		}
		return saved;
	}

	bool is_valid() { return run_validations(); }

	bool is_invalid() { return !run_validations(); }

	void sync_from_attributes() {
		auto& self = static_cast<T&>(*this);
		std::apply(
			[&](auto... c) {
				(sync_from_column(self, c), ...);
			},
			T::columns());
		refresh_snapshot();
	}

	void sync_to_attributes() {
		auto& self = static_cast<T&>(*this);
		std::apply(
			[&](auto... c) {
				(sync_to_column(self, c), ...);
			},
			T::columns());
	}

  private:
	template <class Column> void sync_from_column(T& self, const Column& column) {
		static_assert(std::is_same_v<typename Column::owner_type, T>, "column owner must match typed_model type");

		const auto iter = attributes_.find(column.name);
		if (iter == attributes_.end()) {
			return;
		}

		using member_type = typename Column::member_type;
		self.*(Column::member_ptr) = detail::from_attribute<member_type>(iter->second);
	}

	template <class Column> void sync_to_column(const T& self, const Column& column) {
		static_assert(std::is_same_v<typename Column::owner_type, T>, "column owner must match typed_model type");

		const attribute next_value{ detail::to_attribute(self.*(Column::member_ptr)) };
		const std::string column_name{ column.name };

		if (!new_record_ && column_name == std::string(get_primary_key())) {
			const auto current = attributes_.find(column_name);
			if (current != attributes_.end() && !detail::attribute_equal(current->second, next_value)) {
				throw ex::readonly("Primary key \"{}\" cannot be modified on persisted {}", get_primary_key(),
								   table_name());
			}
		}

		const auto snapshot = snapshot_.find(column_name);
		if (snapshot == snapshot_.end() || !detail::attribute_equal(snapshot->second, next_value)) {
			attributes_[column_name] = next_value;
			flag_dirty(column_name);
		}
	}

	void refresh_snapshot() {
		snapshot_.clear();
		auto& self = static_cast<T&>(*this);
		std::apply(
			[&](auto... c) {
				((snapshot_[std::string(c.name)] =
					  detail::to_attribute(self.*(std::remove_cvref_t<decltype(c)>::member_ptr))),
				 ...);
			},
			T::columns());
	}

	bool run_validations() {
		sync_to_attributes();
		this->clear_errors();
		if constexpr (requires { T::validations(); }) {
			auto& self = static_cast<T&>(*this);
			auto validation_rules = T::validations();
			detail::run_validation_rules(self, this->mutable_errors(), validation_rules);
			return !this->has_errors();
		} else {
			return true;
		}
	}

	attribute_map snapshot_{};
};

} // namespace ar
