// ============================================================================
// sqlite_adapter.cpp
//
// SPDX-FileCopyrightText: 2020-2023 Indridi Bjornsson
// SPDX-License-Identifier: MIT
// See file LICENSE.MIT for detail
//
// Implementation of SQLite adapter
// ============================================================================

#include <adapters/sqlite_adapter.h>
#include <activerecord.h>
#include <sqlite3.h>

namespace ar {

// ============================================================================
// sqlite_statement implementation
// ============================================================================

sqlite_statement::sqlite_statement(sqlite3* db)
	: statement()
	, db_(db)
	, stmt_(nullptr)
	, first_step_result_(SQLITE_OK)
	, used_first_result_(false)
{}

sqlite_statement::~sqlite_statement()
{
	if (stmt_) {
		sqlite3_finalize(stmt_);
		stmt_ = nullptr;
	}
}

void sqlite_statement::execute(const std::string& command, const attribute_vector& values)
{
	// Prepare the SQL statement
	int rc = sqlite3_prepare_v2(db_, command.c_str(), static_cast<int>(command.length()), &stmt_, nullptr);
	if (rc != SQLITE_OK) {
		std::string error = "SQLite prepare error: ";
		error += sqlite3_errmsg(db_);
		throw ex::activerecord(error);
	}

	// Bind parameters (SQLite uses ? placeholders directly, 1-based indexing)
	for (size_t i = 0; i < values.size(); ++i) {
		int param_index = static_cast<int>(i + 1);
		const attribute& param = values[i];

		switch(param.type()) {
			case type_null:
				rc = sqlite3_bind_null(stmt_, param_index);
				break;

			case type_string:
				rc = sqlite3_bind_text(stmt_, param_index, param.as_string().c_str(),
									  -1, SQLITE_TRANSIENT);
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
			std::string error = "SQLite bind error: ";
			error += sqlite3_errmsg(db_);
			throw ex::activerecord(error);
		}
	}

	// Execute the statement immediately (first step)
	// This ensures DDL/DML statements execute, and SELECT statements get their first row
	first_step_result_ = sqlite3_step(stmt_);
	used_first_result_ = false;

	if (first_step_result_ != SQLITE_ROW && first_step_result_ != SQLITE_DONE) {
		std::string error = "SQLite execute error: ";
		error += sqlite3_errmsg(db_);
		throw ex::activerecord(error);
	}
}

bool sqlite_statement::fetch()
{
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

void sqlite_statement::get_attributes(attribute_map& attributes)
{
	int column_count = sqlite3_column_count(stmt_);

	for(int i = 0; i < column_count; i++) {
		const char* column_name = sqlite3_column_name(stmt_, i);
		if (column_name) {
			attributes.emplace(column_name, field_as_attribute(stmt_, i));
		}
	}
}

attribute sqlite_statement::field_as_attribute(sqlite3_stmt* stmt, int column_index)
{
	int type = sqlite3_column_type(stmt, column_index);

	switch(type) {
		case SQLITE_NULL:
			return attribute();

		case SQLITE_INTEGER:
			return attribute(sqlite3_column_int64(stmt, column_index));

		case SQLITE_FLOAT:
			return attribute(sqlite3_column_double(stmt, column_index));

		case SQLITE_TEXT: {
			const unsigned char* text = sqlite3_column_text(stmt, column_index);
			return attribute(text ? reinterpret_cast<const char*>(text) : "");
		}

		case SQLITE_BLOB:
			// For now, return null for BLOB data
			// Could be enhanced later to handle binary data
			return attribute();

		default:
			return attribute();
	}
}

// ============================================================================
// sqlite_adapter implementation
// ============================================================================

sqlite_adapter::sqlite_adapter(const std::string& db_path, const std::string&, const std::string&)
	: connection()
	, db_(nullptr)
{
	// Note: SQLite doesn't use username/password at the library level

	int rc = sqlite3_open(db_path.c_str(), &db_);
	if (rc != SQLITE_OK) {
		std::string error = "SQLite connection error: ";
		if (db_) {
			error += sqlite3_errmsg(db_);
			sqlite3_close(db_);
		} else {
			error += "Failed to allocate database handle";
		}
		db_ = nullptr;
		throw ex::activerecord(error);
	}
}

sqlite_adapter::~sqlite_adapter()
{
	if (db_) {
		sqlite3_close_v2(db_);  // v2 waits for statements to finalize
		db_ = nullptr;
	}
}

std::unique_ptr<statement> sqlite_adapter::query(const std::string& sql, const attribute_vector& values)
{
	std::unique_ptr<statement> stmt(new sqlite_statement(db_));
	stmt->execute(sql, values);
	return stmt;
}

void sqlite_adapter::limit(std::string& sql, long offset, long limit) const
{
	// SQLite LIMIT syntax: LIMIT number_rows OFFSET offset_value
	if (limit >= 0) {
		sql += " LIMIT " + std::to_string(limit);
		if (offset >= 0) {
			sql += " OFFSET " + std::to_string(offset);
		}
	}
}

std::size_t sqlite_adapter::insert_id() const
{
	if (!db_) {
		return 0;
	}
	return static_cast<std::size_t>(sqlite3_last_insert_rowid(db_));
}

void sqlite_adapter::begin_transaction()
{
	query("BEGIN TRANSACTION");
}

void sqlite_adapter::commit()
{
	query("COMMIT");
}

void sqlite_adapter::rollback()
{
	query("ROLLBACK");
}

} // namespace ar
