// ============================================================================
// schema.h
//
// SPDX-FileCopyrightText: 2020-2026 Indridi Bjornsson
// SPDX-License-Identifier: MIT
// See file LICENSE for details
//
// Table schema of a typed_model<T>, built from its columns() and their modifiers
//

#pragma once

#include <activerecord/activerecord.h>
#include <activerecord/datetime.h>
#include <activerecord/typed_model.h>
#include <concepts>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

namespace ar {

enum class column_type : std::uint8_t { bigint, integer, real, decimal, boolean, string, text, datetime };

using column_default = std::variant<std::monostate, std::int64_t, double, bool>;

struct column_schema {
	std::string name{};
	column_type type{ column_type::bigint };
	std::size_t length{ 0 };
	int precision{ 0 };
	int scale{ 0 };
	bool nullable{ false };
	bool unique{ false };
	bool index{ false };
	column_default default_value{};
};

struct foreign_key {
	std::string column{};
	std::string ref_table{};
	std::string ref_column{};
	fk_action on_delete{ fk_action::no_action };
	fk_action on_update{ fk_action::no_action };
};

/** A table as plain data: the input of DDL generation. Build it with table_schema_for<T>() or by hand. */
struct table_schema {
	std::string name{};
	std::string primary_key{};
	std::vector<column_schema> columns{};
	std::vector<foreign_key> foreign_keys{};
};

/** Longest table, column or index name accepted: PostgreSQL's limit, in bytes. */
inline constexpr std::size_t max_name_length{ 63 };

/** Name of the index that index() creates: ix_<length of table>_<table>_<column>. */
std::string index_name(std::string_view table, std::string_view column);

/**
 * Checks the invariants of a finished schema (names, primary key, types, defaults, foreign keys).
 * Names must be non-empty, at most 63 bytes, and contain no '.', NUL or identifier delimiter
 * (` " [ ]). Throws ex::activerecord naming the table and column.
 */
void validate(const table_schema& schema);

namespace detail {

/** DEFAULT literal of a numeric or boolean default, with the adapter's spelling of true and false. */
std::string numeric_literal_sql(const column_schema& column, std::string_view true_sql, std::string_view false_sql);

/** Bytes MySQL stores for DECIMAL(precision, scale): 4 per 9 digits on each side of the point. */
std::uint64_t mysql_decimal_bytes(int precision, int scale);

template <class T>
concept typed_model_type = std::derived_from<T, typed_model<T, true>> || std::derived_from<T, typed_model<T, false>>;

// bool comes first: std::is_unsigned_v<bool> is true
template <class V>
concept schema_value = std::same_as<V, bool> || std::same_as<V, std::string> || std::same_as<V, datetime> ||
					   std::same_as<V, float> || std::same_as<V, double> || (std::integral<V> && std::is_signed_v<V>) ||
					   (std::is_enum_v<V> && std::is_signed_v<std::underlying_type_t<V>>);

/** Throws unless exactly one column of T is named T::primary_key and maps a non-optional std::int64_t. */
template <class T> void check_primary_key(std::string_view role) {
	const std::string_view primary_key{ T::primary_key };
	int count{ 0 };
	bool int64{ true };
	std::apply(
		[&](const auto&... columns) {
			const auto visit = [&]<class Column>(const Column& column) {
				if (column.name != nullptr && primary_key == column.name) {
					++count;
					int64 = int64 && std::same_as<typename Column::member_type, std::int64_t>;
				}
			};
			(visit(columns), ...);
		},
		T::columns());
	if (count != 1) {
		throw ex::activerecord("{} table {}: primary key \"{}\" must be mapped exactly once, found {}", role,
							   T::table_name, primary_key, count);
	}
	if (!int64) {
		throw ex::activerecord("{} table {}: primary key \"{}\" must be a non-optional std::int64_t", role,
							   T::table_name, primary_key);
	}
}

struct column_result {
	column_schema column{};
	std::optional<foreign_key> reference{};
};

std::string_view modifier_name(std::uint16_t bit);

/** Checks one descriptor's modifier history, then converts it. Throws ex::activerecord. */
template <class Column> column_result make_column(const Column& descriptor, const table_schema& schema) {
	const std::string_view table{ schema.name };
	const std::string_view pk{ schema.primary_key };
	using value_type = typename Column::value_type;
	using reference_type = typename Column::reference_type;
	static_assert(schema_value<value_type>,
				  "table_schema_for: unsupported member type (unsigned, long double and other types have no column)");

	if (descriptor.name == nullptr) {
		throw ex::activerecord("Table {}: column without a name", table);
	}
	const std::string_view name{ descriptor.name };
	const column_options& options{ descriptor.options };
	const auto applied = [&](std::uint16_t bit) { return (options.applied & bit) != 0; };

	for (std::uint16_t bit{ 1 }; bit != 0 && bit <= modifier_bit::on_update;
		 bit = static_cast<std::uint16_t>(bit << 1U)) {
		if ((options.repeated & bit) != 0) {
			throw ex::activerecord("Table {}, column {}: {}() used more than once", table, name, modifier_name(bit));
		}
	}
	if (options.default_value.invalid) {
		throw ex::activerecord("Table {}, column {}: default value does not fit the column", table, name);
	}
	if (applied(modifier_bit::limit) && options.limit == 0) {
		throw ex::activerecord("Table {}, column {}: limit() must be greater than 0", table, name);
	}
	if (applied(modifier_bit::limit) && applied(modifier_bit::text)) {
		throw ex::activerecord("Table {}, column {}: text() and limit() exclude each other", table, name);
	}
	if (applied(modifier_bit::decimal) &&
		(options.precision < 1 || options.precision > 38 || options.scale < 0 || options.scale > options.precision)) {
		throw ex::activerecord(
			"Table {}, column {}: decimal({}, {}) needs 1 <= precision <= 38 and 0 <= scale <= precision", table, name,
			options.precision, options.scale);
	}
	if constexpr (std::is_void_v<reference_type>) {
		if (applied(modifier_bit::on_delete) || applied(modifier_bit::on_update)) {
			throw ex::activerecord("Table {}, column {}: on_delete()/on_update() without references()", table, name);
		}
	}
	if (name == pk && (applied(modifier_bit::integer) || applied(modifier_bit::default_value))) {
		throw ex::activerecord("Table {}, column {}: the primary key takes no integer() or default_value()", table,
							   name);
	}
	if (applied(modifier_bit::integer) && options.default_value.kind == default_literal::kind_type::integer &&
		!std::in_range<std::int32_t>(options.default_value.integer)) {
		throw ex::activerecord("Table {}, column {}: default {} does not fit integer()", table, name,
							   options.default_value.integer);
	}

	column_result result{};
	column_schema& column{ result.column };
	column.name = name;
	column.nullable = is_optional_v<typename Column::member_type> && !applied(modifier_bit::not_null);
	column.unique = applied(modifier_bit::unique);
	column.index = applied(modifier_bit::index);
	if constexpr (std::same_as<value_type, bool>) {
		column.type = column_type::boolean;
	} else if constexpr (std::same_as<value_type, std::string>) {
		column.type = applied(modifier_bit::text) ? column_type::text : column_type::string;
		column.length = applied(modifier_bit::text) ? 0 : (applied(modifier_bit::limit) ? options.limit : 255);
	} else if constexpr (std::same_as<value_type, datetime>) {
		column.type = column_type::datetime;
	} else if constexpr (std::floating_point<value_type>) {
		column.type = applied(modifier_bit::decimal) ? column_type::decimal : column_type::real;
		column.precision = options.precision;
		column.scale = options.scale;
	} else {
		column.type = applied(modifier_bit::integer) ? column_type::integer : column_type::bigint;
	}

	switch (options.default_value.kind) {
	case default_literal::kind_type::none:
		break;
	case default_literal::kind_type::integer:
		column.default_value = options.default_value.integer;
		break;
	case default_literal::kind_type::real:
		column.default_value = options.default_value.real;
		break;
	case default_literal::kind_type::boolean:
		column.default_value = options.default_value.boolean;
		break;
	}

	if constexpr (!std::is_void_v<reference_type>) {
		static_assert(typed_model_type<reference_type>, "references<U>(): U must be a typed_model");
		check_primary_key<reference_type>("Referenced");
		result.reference =
			foreign_key{ std::string{ name }, std::string{ reference_type::table_name },
						 std::string{ reference_type::primary_key }, options.on_delete, options.on_update };
	}
	return result;
}

} // namespace detail

/**
 * Builds and validates the schema of T from T::columns(). Throws ex::activerecord on any invalid column,
 * primary key or foreign key. Every model named in references<U>() must be complete where this is used;
 * only U's primary key is inspected.
 */
template <class T> table_schema table_schema_for() {
	static_assert(detail::typed_model_type<T>, "table_schema_for<T>: T must be a typed_model");
	detail::check_primary_key<T>("Table");

	table_schema schema{ std::string{ T::table_name }, std::string{ T::primary_key }, {}, {} };
	std::apply(
		[&](const auto&... columns) {
			const auto add = [&](const auto& descriptor) {
				auto result{ detail::make_column(descriptor, schema) };
				schema.columns.push_back(std::move(result.column));
				if (result.reference) {
					schema.foreign_keys.push_back(std::move(*result.reference));
				}
			};
			(add(columns), ...);
		},
		T::columns());
	validate(schema);
	return schema;
}

} // namespace ar
