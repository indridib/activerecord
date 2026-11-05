// ============================================================================
// ddl.cpp
//
// SPDX-FileCopyrightText: 2020-2026 Indridi Bjornsson
// SPDX-License-Identifier: MIT
// See file LICENSE for details
//
// Shared CREATE TABLE plan and the connection's default DDL functions
//

#include <activerecord/connection.h>
#include <activerecord/schema.h>
#include <array>
#include <format>
#include <set>
#include <string>
#include <variant>

namespace ar {

namespace {

[[noreturn]] void unsupported() {
	throw ex::activerecord("DDL is not supported by this adapter");
}

std::string ascii_folded(std::string_view name) {
	std::string folded{ name };
	for (char& c : folded) {
		if (c >= 'A' && c <= 'Z') {
			c = static_cast<char>(c - 'A' + 'a');
		}
	}
	return folded;
}

std::string_view action_sql(fk_action action) {
	return action == fk_action::cascade ? "CASCADE" : "SET NULL";
}

} // namespace

std::string connection::column_type_sql(const column_schema& /*column*/) const {
	unsupported();
}

std::string connection::primary_key_sql() const {
	unsupported();
}

std::string connection::literal_sql(const column_schema& /*column*/) const {
	unsupported();
}

bool connection::table_exists(std::string_view /*name*/) {
	unsupported();
}

std::vector<std::string> connection::create_table_plan(const table_schema& schema) const {
	validate(schema);
	std::set<std::string> folded;
	for (const column_schema& column : schema.columns) {
		if (!folded.insert(ascii_folded(column.name)).second) {
			throw ex::activerecord("Table {}: column {} differs from another column only in case", schema.name,
								   column.name);
		}
	}
	check_table(schema);

	std::string body;
	for (const column_schema& column : schema.columns) {
		if (!body.empty()) {
			body += ", ";
		}
		if (column.name == schema.primary_key) {
			body += std::format("{} {}", escape(column.name), primary_key_sql());
			continue;
		}
		body += std::format("{} {} {}", escape(column.name), column_type_sql(column),
							column.nullable ? "NULL" : "NOT NULL");
		if (!std::holds_alternative<std::monostate>(column.default_value)) {
			body += " DEFAULT " + literal_sql(column);
		}
		if (column.unique) {
			body += " UNIQUE";
		}
	}
	for (const foreign_key& key : schema.foreign_keys) {
		body += std::format(", FOREIGN KEY ({}) REFERENCES {} ({})", escape(key.column), escape(key.ref_table),
							escape(key.ref_column));
		if (key.on_delete != fk_action::no_action) {
			body += std::format(" ON DELETE {}", action_sql(key.on_delete));
		}
		if (key.on_update != fk_action::no_action) {
			body += std::format(" ON UPDATE {}", action_sql(key.on_update));
		}
	}

	std::vector<std::string> plan{ std::format("CREATE TABLE {} ({}){}", escape(schema.name), body,
											   table_suffix_sql()) };
	for (const column_schema& column : schema.columns) {
		if (column.index) {
			plan.push_back(std::format("CREATE INDEX {} ON {} ({})", escape(index_name(schema.name, column.name)),
									   escape(schema.name), escape(column.name)));
		}
	}
	return plan;
}

namespace detail {

std::string numeric_literal_sql(const column_schema& column, std::string_view true_sql, std::string_view false_sql) {
	if (const auto* value = std::get_if<std::int64_t>(&column.default_value)) {
		return std::format("{}", *value);
	}
	if (const auto* value = std::get_if<double>(&column.default_value)) {
		return std::format("{}", *value);
	}
	if (const auto* value = std::get_if<bool>(&column.default_value)) {
		return std::string{ *value ? true_sql : false_sql };
	}
	throw ex::activerecord("Column {} has no default value", column.name);
}

std::uint64_t mysql_decimal_bytes(int precision, int scale) {
	constexpr std::array<std::uint64_t, 9> remainder_bytes{ 0, 1, 1, 2, 2, 3, 3, 4, 4 };
	const auto bytes = [&](int digits) {
		const auto d{ static_cast<std::uint64_t>(digits) };
		return (d / 9) * 4 + remainder_bytes[d % 9];
	};
	return bytes(precision - scale) + bytes(scale);
}

} // namespace detail

} // namespace ar
