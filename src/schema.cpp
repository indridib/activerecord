// ============================================================================
// schema.cpp
//
// SPDX-FileCopyrightText: 2020-2026 Indridi Bjornsson
// SPDX-License-Identifier: MIT
// See file LICENSE for details
//
// Validation of table_schema values
//

#include <activerecord/schema.h>
#include <cmath>
#include <format>
#include <limits>
#include <set>

namespace ar {

namespace {

void check_name(std::string_view table, std::string_view what, std::string_view name) {
	if (name.empty()) {
		throw ex::activerecord("Table {}: {} name is empty", table, what);
	}
	if (name.size() > max_name_length) {
		throw ex::activerecord("Table {}: {} name \"{}\" is longer than {} bytes", table, what, name, max_name_length);
	}
	if (name.find('.') != std::string_view::npos || name.find('\0') != std::string_view::npos) {
		throw ex::activerecord("Table {}: {} name \"{}\" contains '.' or NUL", table, what, name);
	}
}

bool valid_type(column_type type) {
	switch (type) {
	case column_type::bigint:
	case column_type::integer:
	case column_type::real:
	case column_type::decimal:
	case column_type::boolean:
	case column_type::string:
	case column_type::text:
	case column_type::datetime:
		return true;
	}
	return false;
}

bool valid_action(fk_action action) {
	switch (action) {
	case fk_action::no_action:
	case fk_action::cascade:
	case fk_action::set_null:
		return true;
	}
	return false;
}

void check_default(std::string_view table, const column_schema& column) {
	const auto fail = [&] {
		throw ex::activerecord("Table {}, column {}: default value does not fit the column type", table, column.name);
	};
	switch (column.type) {
	case column_type::bigint:
		if (!std::holds_alternative<std::monostate>(column.default_value) &&
			!std::holds_alternative<std::int64_t>(column.default_value)) {
			fail();
		}
		break;
	case column_type::integer:
		if (const auto* value = std::get_if<std::int64_t>(&column.default_value)) {
			if (*value < std::numeric_limits<std::int32_t>::min() ||
				*value > std::numeric_limits<std::int32_t>::max()) {
				fail();
			}
		} else if (!std::holds_alternative<std::monostate>(column.default_value)) {
			fail();
		}
		break;
	case column_type::real:
	case column_type::decimal:
		if (const auto* value = std::get_if<double>(&column.default_value)) {
			if (!std::isfinite(*value)) {
				fail();
			}
		} else if (!std::holds_alternative<std::monostate>(column.default_value)) {
			fail();
		}
		break;
	case column_type::boolean:
		if (!std::holds_alternative<std::monostate>(column.default_value) &&
			!std::holds_alternative<bool>(column.default_value)) {
			fail();
		}
		break;
	case column_type::string:
	case column_type::text:
	case column_type::datetime:
		if (!std::holds_alternative<std::monostate>(column.default_value)) {
			fail();
		}
		break;
	}
}

} // namespace

std::string index_name(std::string_view table, std::string_view column) {
	return std::format("ix_{}_{}_{}", table.size(), table, column);
}

void validate(const table_schema& schema) {
	const std::string_view table{ schema.name };
	check_name(table, "table", table);
	check_name(table, "primary key", schema.primary_key);

	std::set<std::string_view> names;
	const column_schema* primary_key{ nullptr };
	for (const column_schema& column : schema.columns) {
		check_name(table, "column", column.name);
		if (!names.insert(column.name).second) {
			throw ex::activerecord("Table {}: column {} appears twice", table, column.name);
		}
		if (!valid_type(column.type)) {
			throw ex::activerecord("Table {}, column {}: invalid column type", table, column.name);
		}
		if (column.type == column_type::string && column.length == 0) {
			throw ex::activerecord("Table {}, column {}: string length must be greater than 0", table, column.name);
		}
		if (column.type == column_type::decimal &&
			(column.precision < 1 || column.precision > 38 || column.scale < 0 || column.scale > column.precision)) {
			throw ex::activerecord("Table {}, column {}: invalid decimal({}, {})", table, column.name, column.precision,
								   column.scale);
		}
		check_default(table, column);
		if (column.index) {
			check_name(table, "index", index_name(table, column.name));
		}
		if (column.name == schema.primary_key) {
			primary_key = &column;
		}
	}

	if (primary_key == nullptr) {
		throw ex::activerecord("Table {}: primary key column {} is missing", table, schema.primary_key);
	}
	if (primary_key->type != column_type::bigint || primary_key->nullable ||
		!std::holds_alternative<std::monostate>(primary_key->default_value)) {
		throw ex::activerecord("Table {}: primary key {} must be a non-nullable bigint without a default", table,
							   schema.primary_key);
	}

	for (const foreign_key& key : schema.foreign_keys) {
		check_name(table, "foreign key column", key.column);
		check_name(table, "referenced table", key.ref_table);
		check_name(table, "referenced column", key.ref_column);
		if (!valid_action(key.on_delete) || !valid_action(key.on_update)) {
			throw ex::activerecord("Table {}, column {}: invalid foreign-key action", table, key.column);
		}
		const column_schema* column{ nullptr };
		for (const column_schema& candidate : schema.columns) {
			if (candidate.name == key.column) {
				column = &candidate;
			}
		}
		if (column == nullptr || column->type != column_type::bigint) {
			throw ex::activerecord("Table {}: foreign key column {} must be a bigint column of the table", table,
								   key.column);
		}
		if ((key.on_delete == fk_action::set_null || key.on_update == fk_action::set_null) && !column->nullable) {
			throw ex::activerecord("Table {}, column {}: set_null needs a nullable column", table, key.column);
		}
	}
}

namespace detail {

std::string_view modifier_name(std::uint16_t bit) {
	switch (bit) {
	case modifier_bit::limit:
		return "limit";
	case modifier_bit::text:
		return "text";
	case modifier_bit::decimal:
		return "decimal";
	case modifier_bit::integer:
		return "integer";
	case modifier_bit::not_null:
		return "not_null";
	case modifier_bit::default_value:
		return "default_value";
	case modifier_bit::unique:
		return "unique";
	case modifier_bit::index:
		return "index";
	case modifier_bit::on_delete:
		return "on_delete";
	case modifier_bit::on_update:
		return "on_update";
	default:
		return "modifier";
	}
}

} // namespace detail

} // namespace ar
