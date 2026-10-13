// ============================================================================
// sqlite_adapter.cpp
//
// SPDX-FileCopyrightText: 2020-2026 Indridi Bjornsson
// SPDX-License-Identifier: MIT
// See file LICENSE for details
//
// Implementation of SQLite adapter
//

#include <activerecord/activerecord.h>
#include <activerecord/adapters/sqlite_adapter.h>
#include <cctype>
#include <sqlite3.h>
#include <string>

namespace ar {

namespace {
bool contains_token_ci(std::string_view text, std::string_view token) {
	if (text.empty() || token.empty() || token.size() > text.size()) {
		return false;
	}
	for (std::size_t i = 0; i + token.size() <= text.size(); ++i) {
		std::size_t j{ 0 };
		for (; j < token.size(); ++j) {
			const char a = static_cast<char>(std::tolower(static_cast<unsigned char>(text[i + j])));
			const char b = static_cast<char>(std::tolower(static_cast<unsigned char>(token[j])));
			if (a != b) {
				break;
			}
		}
		if (j == token.size()) {
			return true;
		}
	}
	return false;
}

bool looks_like_datetime_column(sqlite3_stmt* stmt, int column_index) {
	const char* decl_type = sqlite3_column_decltype(stmt, column_index);
	if (decl_type != nullptr) {
		const std::string_view dt{ decl_type };
		if (contains_token_ci(dt, "date") || contains_token_ci(dt, "time")) {
			return true;
		}
	}

	const char* column_name = sqlite3_column_name(stmt, column_index);
	if (column_name == nullptr) {
		return false;
	}

	const std::string name{ column_name };
	return name.ends_with("_at") || name.ends_with("_date") || name.ends_with("_time");
}

std::string normalize_sqlite_datetime(std::string value) {
	for (char& c : value) {
		if (c == 'T') {
			c = ' ';
		}
	}

	const std::size_t space_pos{ value.find(' ') };
	if (space_pos != std::string::npos) {
		const std::size_t tz_pos{ value.find_first_of("Zz+-", space_pos + 1) };
		if (tz_pos != std::string::npos) {
			value.erase(tz_pos);
		}
	}

	const std::size_t frac_pos{ value.find('.') };
	if (frac_pos != std::string::npos) {
		value.erase(frac_pos);
	}

	return value;
}
} // namespace

// ============================================================================
// sqlite_statement implementation
// ============================================================================

static_assert(SQLITE_OK == 0, "sqlite_statement::first_step_result_ default assumes SQLITE_OK == 0");

sqlite_statement::sqlite_statement(sqlite3* db) : statement(), db_(db) {}

sqlite_statement::~sqlite_statement() {
	if (stmt_ != nullptr) {
		sqlite3_finalize(stmt_);
		stmt_ = nullptr;
	}
}

void sqlite_statement::execute(const std::string& command, const attribute_vector& values) {
	// Prepare the SQL statement
	int rc{ sqlite3_prepare_v2(db_, command.c_str(), static_cast<int>(command.length()), &stmt_, nullptr) };
	if (rc != SQLITE_OK) {
		std::string error{ "SQLite prepare error: " };
		error += sqlite3_errmsg(db_);
		throw ex::activerecord(error);
	}

	// Bind parameters (SQLite uses ? placeholders directly, 1-based indexing)
	for (size_t i = 0; i < values.size(); ++i) {
		int param_index{ static_cast<int>(i + 1) };
		const attribute& param = values[i];

		switch (param.type()) {
		case type_null:
			rc = sqlite3_bind_null(stmt_, param_index);
			break;

		case type_string:
			rc = sqlite3_bind_text(stmt_, param_index, param.as_string().c_str(), -1, SQLITE_TRANSIENT);
			break;

		case type_integer:
			rc = sqlite3_bind_int64(stmt_, param_index, param.as_integer());
			break;

		case type_decimal:
			rc = sqlite3_bind_double(stmt_, param_index, param.as_real());
			break;

		case type_bool:
			rc = sqlite3_bind_int(stmt_, param_index, param.as_bool() ? 1 : 0);
			break;

		case type_date:
			// Store datetime as Unix timestamp (INTEGER)
			rc = sqlite3_bind_int64(stmt_, param_index, param.as_date().timestamp());
			break;

		default:
			rc = sqlite3_bind_null(stmt_, param_index);
			break;
		}

		if (rc != SQLITE_OK) {
			std::string error{ "SQLite bind error: " };
			error += sqlite3_errmsg(db_);
			throw ex::activerecord(error);
		}
	}

	// Execute the statement immediately (first step)
	// This ensures DDL/DML statements execute, and SELECT statements get their first row
	first_step_result_ = sqlite3_step(stmt_);
	used_first_result_ = false;
	affected_rows_ = (sqlite3_stmt_readonly(stmt_) != 0) ? 0 : static_cast<std::size_t>(sqlite3_changes(db_));

	if (first_step_result_ != SQLITE_ROW && first_step_result_ != SQLITE_DONE) {
		std::string error{ "SQLite execute error: " };
		error += sqlite3_errmsg(db_);
		throw ex::activerecord(error);
	}
}

bool sqlite_statement::fetch() {
	// On first fetch, use the result from execute()
	int rc;
	if (!used_first_result_) {
		rc = first_step_result_;
		used_first_result_ = true;
	} else {
		rc = sqlite3_step(stmt_);
	}
	return (rc == SQLITE_ROW);
}

void sqlite_statement::get_attributes(attribute_map& attributes) {
	int column_count{ sqlite3_column_count(stmt_) };

	for (int i = 0; i < column_count; i++) {
		const char* column_name = sqlite3_column_name(stmt_, i);
		if (column_name != nullptr) {
			attributes.emplace(column_name, field_as_attribute(stmt_, i));
		}
	}
}

attribute sqlite_statement::field_as_attribute(sqlite3_stmt* stmt, int column_index) {
	const int type{ sqlite3_column_type(stmt, column_index) };
	const bool is_datetime{ looks_like_datetime_column(stmt, column_index) };

	switch (type) {
	case SQLITE_NULL:
		return {};

	case SQLITE_INTEGER:
		if (is_datetime) {
			return { datetime{ static_cast<std::time_t>(sqlite3_column_int64(stmt, column_index)) } };
		}
		return { sqlite3_column_int64(stmt, column_index) };

	case SQLITE_FLOAT:
		return { sqlite3_column_double(stmt, column_index) };

	case SQLITE_TEXT: {
		const unsigned char* text = sqlite3_column_text(stmt, column_index);
		const std::string value{ text != nullptr ? reinterpret_cast<const char*>(text) : "" };
		if (is_datetime && !value.empty()) {
			return { datetime::parse(normalize_sqlite_datetime(value)) };
		}
		return { value };
	}

	case SQLITE_BLOB:
		// For now, return null for BLOB data
		// Could be enhanced later to handle binary data
	default:
		return {};
	}
}

// ============================================================================
// sqlite_adapter implementation
// ============================================================================

sqlite_adapter::sqlite_adapter(const std::string& db_path, const std::string&, const std::string&) : connection() {
	// Note: SQLite doesn't use username/password at the library level

	int rc{ sqlite3_open(db_path.c_str(), &db_) };
	if (rc != SQLITE_OK) {
		std::string error{ "SQLite connection error: " };
		if (db_ != nullptr) {
			error += sqlite3_errmsg(db_);
			sqlite3_close(db_);
		} else {
			error += "Failed to allocate database handle";
		}
		db_ = nullptr;
		throw ex::activerecord(error);
	}
}

sqlite_adapter::~sqlite_adapter() {
	if (db_ != nullptr) {
		sqlite3_close_v2(db_); // v2 waits for statements to finalize
		db_ = nullptr;
	}
}

std::unique_ptr<statement> sqlite_adapter::query(const std::string& sql, const attribute_vector& values) {
	std::unique_ptr<statement> stmt(new sqlite_statement(db_));
	stmt->execute(sql, values);
	return stmt;
}

void sqlite_adapter::limit(std::string& sql, long offset, long limit) const {
	// SQLite LIMIT syntax: LIMIT number_rows OFFSET offset_value
	if (limit >= 0) {
		sql += " LIMIT " + std::to_string(limit);
		if (offset >= 0) {
			sql += " OFFSET " + std::to_string(offset);
		}
	}
}

std::size_t sqlite_adapter::insert_id() const {
	if (db_ == nullptr) {
		return 0;
	}
	return static_cast<std::size_t>(sqlite3_last_insert_rowid(db_));
}

void sqlite_adapter::begin_transaction() {
	query("BEGIN TRANSACTION");
}

void sqlite_adapter::commit() {
	query("COMMIT");
}

void sqlite_adapter::rollback() {
	query("ROLLBACK");
}

} // namespace ar
