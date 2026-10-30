// ============================================================================
// typed_model.h
//
// SPDX-FileCopyrightText: 2020-2026 Indridi Bjornsson
// SPDX-License-Identifier: MIT
// See file LICENSE for details
//
// typed_model<T>: CRTP model whose struct members map to table columns
//

#pragma once

#include <activerecord/attribute.h>
#include <activerecord/model.h>
#include <activerecord/validations.h>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <string>
#include <tuple>
#include <type_traits>
#include <utility>

namespace ar {

template <class MemberPtr> struct member_pointer_traits;

template <class Owner, class Member> struct member_pointer_traits<Member Owner::*> {
	using owner_type = Owner;
	using member_type = Member;
};

namespace detail {

template <class T> struct is_optional : std::false_type {};
template <class T> struct is_optional<std::optional<T>> : std::true_type {};

template <class T> constexpr bool is_optional_v = is_optional<T>::value;

template <class T> struct unwrap_optional {
	using type = T;
};
template <class T> struct unwrap_optional<std::optional<T>> {
	using type = T;
};

template <class T> using unwrap_optional_t = typename unwrap_optional<T>::type;

} // namespace detail

/** Foreign-key action for ON DELETE / ON UPDATE. */
enum class fk_action : std::uint8_t { no_action, cascade, set_null };

/** One bit per column modifier, for column_options::applied and column_options::repeated. */
namespace modifier_bit {
inline constexpr std::uint16_t limit{ 1U << 0U };
inline constexpr std::uint16_t text{ 1U << 1U };
inline constexpr std::uint16_t decimal{ 1U << 2U };
inline constexpr std::uint16_t integer{ 1U << 3U };
inline constexpr std::uint16_t not_null{ 1U << 4U };
inline constexpr std::uint16_t default_value{ 1U << 5U };
inline constexpr std::uint16_t unique{ 1U << 6U };
inline constexpr std::uint16_t index{ 1U << 7U };
inline constexpr std::uint16_t on_delete{ 1U << 8U };
inline constexpr std::uint16_t on_update{ 1U << 9U };
} // namespace modifier_bit

/** A column default as given to default_value(); invalid when the value did not fit the member's kind. */
struct default_literal {
	enum class kind_type : std::uint8_t { none, integer, real, boolean };

	kind_type kind{ kind_type::none };
	bool invalid{ false };
	std::int64_t integer{ 0 };
	double real{ 0.0 };
	bool boolean{ false };
};

/** Schema details of one column, set by the column_def modifiers. They never affect reading or writing. */
struct column_options {
	std::uint16_t applied{ 0 };
	std::uint16_t repeated{ 0 };
	std::size_t limit{ 0 };
	int precision{ 0 };
	int scale{ 0 };
	default_literal default_value{};
	fk_action on_delete{ fk_action::no_action };
	fk_action on_update{ fk_action::no_action };
};

namespace detail {

template <class Member, class V>
concept default_value_for =
	(std::same_as<Member, bool> && std::same_as<V, bool>) || (std::is_enum_v<Member> && std::same_as<V, Member>) ||
	(std::integral<Member> && !std::same_as<Member, bool> && std::integral<V> && !std::same_as<V, bool>) ||
	(std::floating_point<Member> && (std::integral<V> || std::floating_point<V>) && !std::same_as<V, bool>);

template <class Member, class V> constexpr default_literal make_default(V value) {
	default_literal literal{};
	if constexpr (std::same_as<Member, bool>) {
		literal.kind = default_literal::kind_type::boolean;
		literal.boolean = value;
	} else if constexpr (std::is_enum_v<Member>) {
		literal.kind = default_literal::kind_type::integer;
		const auto underlying{ static_cast<std::underlying_type_t<Member>>(value) };
		literal.invalid = !std::in_range<std::int64_t>(underlying);
		literal.integer = literal.invalid ? 0 : static_cast<std::int64_t>(underlying);
	} else if constexpr (std::integral<Member>) {
		literal.kind = default_literal::kind_type::integer;
		literal.invalid = !std::in_range<std::int64_t>(value);
		literal.integer = literal.invalid ? 0 : static_cast<std::int64_t>(value);
	} else if constexpr (std::integral<V>) {
		literal.kind = default_literal::kind_type::real;
		literal.real = static_cast<double>(value);
	} else {
		literal.kind = default_literal::kind_type::real;
		// NaN compares unequal to itself; infinities and too-wide values fall outside double's range
		literal.invalid = value != value || value > std::numeric_limits<double>::max() ||
						  value < std::numeric_limits<double>::lowest();
		literal.real = literal.invalid ? 0.0 : static_cast<double>(value);
	}
	return literal;
}

} // namespace detail

/**
 * Maps a struct member to a column. Ref is the model a references<U>() column points to (void: none).
 * The modifiers only describe the table for table_schema_for<T>(); a wrong-kind modifier does not exist.
 */
template <auto MemberPtr, class Ref = void> struct column_def {
	using traits = member_pointer_traits<decltype(MemberPtr)>;
	using owner_type = typename traits::owner_type;
	using member_type = typename traits::member_type;
	using value_type = detail::unwrap_optional_t<member_type>;
	using reference_type = Ref;

	const char* name{ nullptr };
	column_options options{};
	static constexpr auto member_ptr = MemberPtr;

	/** VARCHAR(n) instead of the default 255. */
	constexpr column_def limit(std::size_t n) const
		requires std::same_as<value_type, std::string>
	{
		auto column{ marked(modifier_bit::limit) };
		column.options.limit = n;
		return column;
	}

	/** Unbounded text instead of VARCHAR. */
	constexpr column_def text() const
		requires std::same_as<value_type, std::string>
	{
		return marked(modifier_bit::text);
	}

	/** Exact DECIMAL(precision, scale); the C++ value stays floating-point. */
	// NOLINTNEXTLINE(bugprone-easily-swappable-parameters) -- public API; see docs/hardening.md
	constexpr column_def decimal(int precision, int scale) const
		requires std::floating_point<value_type> && (!std::same_as<value_type, long double>)
	{
		auto column{ marked(modifier_bit::decimal) };
		column.options.precision = precision;
		column.options.scale = scale;
		return column;
	}

	/** 32-bit INT instead of BIGINT. */
	constexpr column_def integer() const
		requires std::integral<value_type> && (!std::same_as<value_type, bool>)
	{
		return marked(modifier_bit::integer);
	}

	/** NOT NULL although the member is optional; saving nullopt then fails in the database. */
	constexpr column_def not_null() const
		requires detail::is_optional_v<member_type>
	{
		return marked(modifier_bit::not_null);
	}

	/** DEFAULT in CREATE TABLE only; a typed save always writes the member's value. */
	template <class V>
		requires detail::default_value_for<value_type, V>
	constexpr column_def default_value(V value) const {
		auto column{ marked(modifier_bit::default_value) };
		column.options.default_value = detail::make_default<value_type>(value);
		return column;
	}

	constexpr column_def unique() const { return marked(modifier_bit::unique); }

	constexpr column_def index() const { return marked(modifier_bit::index); }

	/** Foreign key to U's primary key. U may still be incomplete here. */
	template <class U>
		requires std::is_void_v<Ref> && (!std::is_void_v<U>) && std::same_as<value_type, std::int64_t>
	constexpr column_def<MemberPtr, U> references() const {
		return column_def<MemberPtr, U>{ name, options };
	}

	constexpr column_def on_delete(fk_action action) const {
		auto column{ marked(modifier_bit::on_delete) };
		column.options.on_delete = action;
		return column;
	}

	constexpr column_def on_update(fk_action action) const {
		auto column{ marked(modifier_bit::on_update) };
		column.options.on_update = action;
		return column;
	}

  private:
	constexpr column_def marked(std::uint16_t bit) const {
		auto column{ *this };
		if ((column.options.applied & bit) != 0) {
			column.options.repeated = static_cast<std::uint16_t>(column.options.repeated | bit);
		}
		column.options.applied = static_cast<std::uint16_t>(column.options.applied | bit);
		return column;
	}
};

template <auto MemberPtr> constexpr auto col(const char* name) {
	return column_def<MemberPtr>{ name };
}

template <class... Columns> constexpr auto columns(Columns... cols) {
	return std::tuple<Columns...>{ cols... };
}

namespace detail {

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

	typed_model() : model(st_table_name(), st_primary_key()) { this->set_record_tracking_policy(TrackRecordMetadata); }

	/**
	 * Saves the record. Throws ex::readonly for a persisted record whose primary key was not loaded
	 * (e.g. queried with select() without it), since its row cannot be identified.
	 */
	bool save(bool should_validate = true) {
		if (!new_record_ && !attributes_.contains(std::string(get_primary_key()))) {
			throw ex::readonly("Persisted {} cannot be saved without its primary key \"{}\"", table_name(),
							   get_primary_key());
		}
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
		std::apply([&](auto... c) { (sync_from_column(self, c), ...); }, T::columns());
		refresh_snapshot();
	}

	void sync_to_attributes() {
		auto& self = static_cast<T&>(*this);
		std::apply([&](auto... c) { (sync_to_column(self, c), ...); }, T::columns());
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
			if (current == attributes_.end()) {
				return;
			}
			if (!detail::attribute_equal(current->second, next_value)) {
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
