// ============================================================================
// mysql_adapter.cpp
//
// SPDX-FileCopyrightText: 2020-2023 Indridi Bjornsson
// SPDX-License-Identifier: MIT
// See file LICENSE.MIT for detail
//
// Implementation of MySQL adapter
// ============================================================================

#include <adapters/mysql_adapter.h>
#include <activerecord.h>
#include <mysql/mysql.h>
#include <cstring>

namespace ar {

// ============================================================================
// mysql_statement implementation
// ============================================================================

mysql_statement::mysql_statement(MYSQL* db)
	: statement()
	, db_(db)
	, stmt_(nullptr)
	, metadata_(nullptr)
	, result_binds_(nullptr)
	, lengths_(nullptr)
	, num_fields_(0)
	, has_result_(false)
{}

mysql_statement::~mysql_statement()
{
	if (result_binds_) {
		delete[] result_binds_;
		result_binds_ = nullptr;
	}
	if (lengths_) {
		delete[] lengths_;
		lengths_ = nullptr;
	}
	if (metadata_) {
		mysql_free_result(metadata_);
		metadata_ = nullptr;
	}
	if (stmt_) {
		mysql_stmt_close(stmt_);
		stmt_ = nullptr;
	}
}

void mysql_statement::execute(const std::string& command, const attribute_vector& values)
{
	// Initialize statement
	stmt_ = mysql_stmt_init(db_);
	if (!stmt_) {
		throw ex::activerecord("MySQL: Failed to initialize statement");
	}

	// Prepare the SQL statement
	if (mysql_stmt_prepare(stmt_, command.c_str(), static_cast<unsigned long>(command.length()))) {
		std::string error = "MySQL prepare error: ";
		error += mysql_stmt_error(stmt_);
		throw ex::activerecord(error);
	}

	// Bind parameters if any
	if (!values.empty()) {
		MYSQL_BIND* param_binds = new MYSQL_BIND[values.size()];
		std::memset(param_binds, 0, sizeof(MYSQL_BIND) * values.size());

		// Storage for parameter data
		std::vector<long long> int_values;
		std::vector<double> double_values;
		std::vector<std::string> string_values;

		int_values.reserve(values.size());
		double_values.reserve(values.size());
		string_values.reserve(values.size());

		for (size_t i = 0; i < values.size(); ++i) {
			const attribute& param = values[i];

			switch(param.type()) {
				case type_null:
					param_binds[i].buffer_type = MYSQL_TYPE_NULL;
					break;

				case type_string: {
					string_values.push_back(param.as_string());
					param_binds[i].buffer_type = MYSQL_TYPE_STRING;
					param_binds[i].buffer = const_cast<char*>(string_values.back().c_str());
					param_binds[i].buffer_length = static_cast<unsigned long>(string_values.back().length());
					break;
				}

				case type_integer: {
					int_values.push_back(param.as_integer());
					param_binds[i].buffer_type = MYSQL_TYPE_LONGLONG;
					param_binds[i].buffer = &int_values.back();
					break;
				}

				case type_date: {
					// Store datetime as Unix timestamp
					int_values.push_back(param.as_date().timestamp());
					param_binds[i].buffer_type = MYSQL_TYPE_LONGLONG;
					param_binds[i].buffer = &int_values.back();
					break;
				}

				case type_decimal: {
					double_values.push_back(param.as_real());
					param_binds[i].buffer_type = MYSQL_TYPE_DOUBLE;
					param_binds[i].buffer = &double_values.back();
					break;
				}

				case type_bool: {
					int_values.push_back(param.as_bool() ? 1 : 0);
					param_binds[i].buffer_type = MYSQL_TYPE_TINY;
					param_binds[i].buffer = &int_values.back();
					break;
				}

				default:
					param_binds[i].buffer_type = MYSQL_TYPE_NULL;
					break;
			}
		}

		if (mysql_stmt_bind_param(stmt_, param_binds)) {
			delete[] param_binds;
			std::string error = "MySQL bind error: ";
			error += mysql_stmt_error(stmt_);
			throw ex::activerecord(error);
		}

		delete[] param_binds;
	}

	// Execute the statement
	if (mysql_stmt_execute(stmt_)) {
		std::string error = "MySQL execute error: ";
		error += mysql_stmt_error(stmt_);
		throw ex::activerecord(error);
	}

	// Prepare result bindings if this is a SELECT query
	metadata_ = mysql_stmt_result_metadata(stmt_);
	if (metadata_) {
		has_result_ = true;
		num_fields_ = mysql_num_fields(metadata_);

		result_binds_ = new MYSQL_BIND[num_fields_];
		lengths_ = new unsigned long[num_fields_];
		std::memset(result_binds_, 0, sizeof(MYSQL_BIND) * num_fields_);

		// For now, fetch all columns as strings for simplicity
		// This can be optimized later to use proper types
		for (int i = 0; i < num_fields_; ++i) {
			result_binds_[i].buffer_type = MYSQL_TYPE_STRING;
			result_binds_[i].buffer = nullptr;  // Will be allocated dynamically
			result_binds_[i].buffer_length = 0;
			result_binds_[i].length = &lengths_[i];
		}

		if (mysql_stmt_bind_result(stmt_, result_binds_)) {
			std::string error = "MySQL bind result error: ";
			error += mysql_stmt_error(stmt_);
			throw ex::activerecord(error);
		}
	}
}

bool mysql_statement::fetch()
{
	if (!has_result_) {
		return false;
	}

	int result = mysql_stmt_fetch(stmt_);

	if (result == 0) {
		return true;  // Row fetched successfully
	} else if (result == MYSQL_NO_DATA) {
		return false;  // No more rows
	} else if (result == MYSQL_DATA_TRUNCATED) {
		// Data was truncated, but we can still use it
		return true;
	} else {
		// Error occurred
		std::string error = "MySQL fetch error: ";
		error += mysql_stmt_error(stmt_);
		throw ex::activerecord(error);
	}
}

void mysql_statement::get_attributes(attribute_map& attributes)
{
	if (!has_result_ || !metadata_) {
		return;
	}

	MYSQL_FIELD* fields = mysql_fetch_fields(metadata_);

	for (int i = 0; i < num_fields_; ++i) {
		std::string column_name = fields[i].name;
		attributes.emplace(column_name, field_as_attribute(metadata_, result_binds_, lengths_, i));
	}
}

attribute mysql_statement::field_as_attribute(MYSQL_RES* metadata, MYSQL_BIND* binds, unsigned long* lengths, int column_index)
{
	MYSQL_FIELD* fields = mysql_fetch_fields(metadata);
	MYSQL_FIELD& field = fields[column_index];

	// Check for NULL value
	if (binds[column_index].is_null_value) {
		return attribute();
	}

	// Convert based on MySQL field type
	switch(field.type) {
		case MYSQL_TYPE_TINY:
		case MYSQL_TYPE_SHORT:
		case MYSQL_TYPE_LONG:
		case MYSQL_TYPE_LONGLONG:
		case MYSQL_TYPE_INT24:
			// Integer types
			if (binds[column_index].buffer) {
				long long value = *static_cast<long long*>(binds[column_index].buffer);
				return attribute(static_cast<int64_t>(value));
			}
			return attribute();

		case MYSQL_TYPE_FLOAT:
		case MYSQL_TYPE_DOUBLE:
		case MYSQL_TYPE_DECIMAL:
		case MYSQL_TYPE_NEWDECIMAL:
			// Floating point types
			if (binds[column_index].buffer) {
				double value = *static_cast<double*>(binds[column_index].buffer);
				return attribute(value);
			}
			return attribute();

		case MYSQL_TYPE_TIMESTAMP:
		case MYSQL_TYPE_DATETIME:
		case MYSQL_TYPE_DATE:
			// Date/time types - store as timestamp
			// For now, return as integer (Unix timestamp would need conversion)
			return attribute();

		case MYSQL_TYPE_STRING:
		case MYSQL_TYPE_VAR_STRING:
		case MYSQL_TYPE_VARCHAR:
		case MYSQL_TYPE_BLOB:
		case MYSQL_TYPE_TINY_BLOB:
		case MYSQL_TYPE_MEDIUM_BLOB:
		case MYSQL_TYPE_LONG_BLOB:
		default:
			// String/blob types - return as string
			if (binds[column_index].buffer && lengths[column_index] > 0) {
				std::string value(static_cast<char*>(binds[column_index].buffer), lengths[column_index]);
				return attribute(value);
			}
			return attribute();
	}
}

// ============================================================================
// mysql_adapter implementation
// ============================================================================

mysql_adapter::mysql_adapter(const std::string& host, const std::string& user,
                             const std::string& password, const std::string& database,
                             unsigned int port)
	: connection()
	, db_(nullptr)
{
	// Initialize MySQL library
	db_ = mysql_init(nullptr);
	if (!db_) {
		throw ex::activerecord("MySQL: Failed to initialize");
	}

	// Connect to database
	if (!mysql_real_connect(db_, host.c_str(), user.c_str(), password.c_str(),
	                        database.c_str(), port, nullptr, 0)) {
		std::string error = "MySQL connection error: ";
		error += mysql_error(db_);
		mysql_close(db_);
		throw ex::activerecord(error);
	}

	// Set charset to UTF-8
	mysql_set_character_set(db_, "utf8mb4");
}

mysql_adapter::~mysql_adapter()
{
	if (db_) {
		mysql_close(db_);
		db_ = nullptr;
	}
}

std::unique_ptr<statement> mysql_adapter::query(const std::string& sql, const attribute_vector& values)
{
	auto stmt = std::make_unique<mysql_statement>(db_);
	stmt->execute(sql, values);
	return stmt;
}

void mysql_adapter::limit(std::string& sql, long offset, long limit) const
{
	// MySQL uses LIMIT with optional OFFSET
	if (limit >= 0) {
		sql += " LIMIT " + std::to_string(limit);
		if (offset >= 0) {
			sql += " OFFSET " + std::to_string(offset);
		}
	}
}

std::size_t mysql_adapter::insert_id() const
{
	if (!db_) {
		return 0;
	}
	return static_cast<std::size_t>(mysql_insert_id(db_));
}

} // namespace ar
