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
#include <algorithm>
#include <cstring>
#include <type_traits>
#include <string>

namespace ar {

namespace {
std::string normalize_mysql_datetime(std::string value)
{
	for (char& c : value) {
		if (c == 'T') {
			c = ' ';
		}
	}

	const std::size_t space_pos{value.find(' ')};
	if (space_pos != std::string::npos) {
		std::size_t tz_pos{value.find_first_of("Zz+-", space_pos + 1)};
		if (tz_pos != std::string::npos) {
			value.erase(tz_pos);
		}
	}

	const std::size_t frac_pos{value.find('.')};
	if (frac_pos != std::string::npos) {
		value.erase(frac_pos);
	}

	return value;
}
} // namespace

// ============================================================================
// mysql_statement implementation
// ============================================================================

mysql_statement::mysql_statement(MYSQL* db)
	: statement()
	, db_(db)
	, stmt_(nullptr)
	, metadata_(nullptr)
	, num_fields_(0)
	, has_result_(false)
	, affected_rows_(0)
{}

mysql_statement::~mysql_statement()
{
	// Vectors (result_binds_, lengths_, buffers_) clean themselves up via RAII
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

	// Bind parameters if any.
	if (!values.empty()) {
		std::vector<MYSQL_BIND> param_binds(values.size());
		std::memset(param_binds.data(), 0, sizeof(MYSQL_BIND) * values.size());
		std::vector<unsigned long> string_lengths(values.size(), 0U);
		using mysql_null_flag = std::remove_pointer_t<decltype(MYSQL_BIND{}.is_null)>;
		std::vector<mysql_null_flag> is_null(values.size(), static_cast<mysql_null_flag>(0));

		// CRITICAL: Reserve capacity to prevent reallocation. We store pointers to elements
		// of these vectors in param_binds, so the vectors must not reallocate during the loop.
		// Reserving values.size() guarantees no reallocation even if all values are of one type.
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
					is_null[i] = true;
					param_binds[i].is_null = &is_null[i];
					break;

				case type_string:
					string_values.emplace_back(param.as_string());
					param_binds[i].buffer_type = MYSQL_TYPE_STRING;
					param_binds[i].buffer = const_cast<char*>(string_values.back().c_str());
					string_lengths[i] = static_cast<unsigned long>(string_values.back().size());
					param_binds[i].buffer_length = string_lengths[i];
					param_binds[i].length = &string_lengths[i];
					break;

				case type_integer:
					int_values.emplace_back(static_cast<long long>(param.as_integer()));
					param_binds[i].buffer_type = MYSQL_TYPE_LONGLONG;
					param_binds[i].buffer = &int_values.back();
					break;

				case type_date:
					int_values.emplace_back(static_cast<long long>(param.as_date().timestamp()));
					param_binds[i].buffer_type = MYSQL_TYPE_LONGLONG;
					param_binds[i].buffer = &int_values.back();
					break;

				case type_decimal:
					double_values.emplace_back(param.as_real());
					param_binds[i].buffer_type = MYSQL_TYPE_DOUBLE;
					param_binds[i].buffer = &double_values.back();
					break;

				case type_bool:
					int_values.emplace_back(param.as_bool() ? 1LL : 0LL);
					param_binds[i].buffer_type = MYSQL_TYPE_TINY;
					param_binds[i].buffer = &int_values.back();
					break;

				default:
					param_binds[i].buffer_type = MYSQL_TYPE_NULL;
					is_null[i] = true;
					param_binds[i].is_null = &is_null[i];
					break;
			}
		}

		if (mysql_stmt_bind_param(stmt_, param_binds.data())) {
			std::string error = "MySQL bind error: ";
			error += mysql_stmt_error(stmt_);
			throw ex::activerecord(error);
		}

		// Execute while bound parameter storage is still alive.
		if (mysql_stmt_execute(stmt_)) {
			std::string error = "MySQL execute error: ";
			error += mysql_stmt_error(stmt_);
			throw ex::activerecord(error);
		}
	} else {
		// Execute the statement
		if (mysql_stmt_execute(stmt_)) {
			std::string error = "MySQL execute error: ";
			error += mysql_stmt_error(stmt_);
			throw ex::activerecord(error);
		}
	}

	// Prepare result bindings if this is a SELECT query
	metadata_ = mysql_stmt_result_metadata(stmt_);
	const auto rows = mysql_stmt_affected_rows(stmt_);
	affected_rows_ = (rows == static_cast<decltype(rows)>(-1)) ? 0 : static_cast<std::size_t>(rows);
	if (metadata_) {
		has_result_ = true;
		num_fields_ = mysql_num_fields(metadata_);

		// Ask server to compute max_length for each column so we can size buffers better.
		my_bool update_max_length = 1;
		mysql_stmt_attr_set(stmt_, STMT_ATTR_UPDATE_MAX_LENGTH, &update_max_length);

		// Use RAII vectors instead of raw arrays to prevent memory leaks
		result_binds_.resize(num_fields_);
		lengths_.resize(num_fields_, 0UL);
		buffers_.resize(num_fields_);
		std::memset(result_binds_.data(), 0, sizeof(MYSQL_BIND) * num_fields_);

		if (mysql_stmt_store_result(stmt_)) {
			std::string error = "MySQL store result error: ";
			error += mysql_stmt_error(stmt_);
			throw ex::activerecord(error);
		}

		MYSQL_FIELD* fields = mysql_fetch_fields(metadata_);
		if (!fields) {
			throw ex::activerecord("MySQL: Failed to fetch field metadata");
		}

		for (int i = 0; i < num_fields_; ++i) {
			result_binds_[i].buffer_type = MYSQL_TYPE_STRING;
			const unsigned long preferred = fields[i].max_length;
			const unsigned long buffer_size = std::max(preferred + 1UL, 256UL);

			// Allocate buffer in RAII vector
			buffers_[i].data.resize(buffer_size, '\0');

			result_binds_[i].buffer_length = buffer_size;
			result_binds_[i].buffer = buffers_[i].data.data();
			result_binds_[i].length = &lengths_[i];
			result_binds_[i].is_null = buffers_[i].is_null.get();
		}

		if (mysql_stmt_bind_result(stmt_, result_binds_.data())) {
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
		// Resize and fetch full column values for truncated fields.
		for (int i = 0; i < num_fields_; ++i) {
			if (lengths_[i] <= result_binds_[i].buffer_length) {
				continue;
			}

			unsigned long full_length = lengths_[i];
			// Resize buffer using RAII vector instead of raw allocation
			buffers_[i].data.resize(full_length + 1, '\0');

			MYSQL_BIND fetch_bind{};
			fetch_bind.buffer_type = MYSQL_TYPE_STRING;
			fetch_bind.buffer = buffers_[i].data.data();
			fetch_bind.buffer_length = full_length + 1;
			fetch_bind.length = &full_length;
			fetch_bind.is_null = buffers_[i].is_null.get();

			if (mysql_stmt_fetch_column(stmt_, &fetch_bind, static_cast<unsigned int>(i), 0) != 0) {
				std::string error = "MySQL fetch column error: ";
				error += mysql_stmt_error(stmt_);
				throw ex::activerecord(error);
			}

			// Update bind info (buffer pointer already points to the resized vector)
			result_binds_[i].buffer_length = full_length + 1;
			lengths_[i] = full_length;
		}
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
	if (!fields) {
		throw ex::activerecord("MySQL: Failed to fetch field metadata");
	}

	for (int i = 0; i < num_fields_; ++i) {
		std::string column_name = fields[i].name;
		attributes.emplace(column_name, field_as_attribute(metadata_, result_binds_.data(), lengths_.data(), i));
	}
}

attribute mysql_statement::field_as_attribute(MYSQL_RES* metadata, MYSQL_BIND* binds, unsigned long* lengths, int column_index)
{
	MYSQL_FIELD* fields = mysql_fetch_fields(metadata);
	MYSQL_FIELD& field = fields[column_index];

	// Check for NULL value
	if (binds[column_index].is_null != nullptr && *binds[column_index].is_null) {
		return attribute();
	}

	const char* raw_value = static_cast<const char*>(binds[column_index].buffer);
	const std::string value = (raw_value != nullptr && lengths[column_index] > 0)
		? std::string(raw_value, lengths[column_index])
		: std::string{};

	// Convert based on MySQL field type
	switch(field.type) {
		case MYSQL_TYPE_TINY:
		case MYSQL_TYPE_SHORT:
		case MYSQL_TYPE_LONG:
		case MYSQL_TYPE_LONGLONG:
		case MYSQL_TYPE_INT24:
			return value.empty() ? attribute() : attribute(static_cast<int64_t>(std::stoll(value)));

		case MYSQL_TYPE_FLOAT:
		case MYSQL_TYPE_DOUBLE:
		case MYSQL_TYPE_DECIMAL:
		case MYSQL_TYPE_NEWDECIMAL:
			return value.empty() ? attribute() : attribute(std::stod(value));

		case MYSQL_TYPE_TIMESTAMP:
		case MYSQL_TYPE_DATETIME:
		case MYSQL_TYPE_DATE:
			return value.empty() ? attribute() : attribute(datetime::parse(normalize_mysql_datetime(value)));

		case MYSQL_TYPE_TIME:
			return value.empty() ? attribute() : attribute(datetime::parse(std::string("1970-01-01 ") + normalize_mysql_datetime(value)));

		case MYSQL_TYPE_STRING:
		case MYSQL_TYPE_VAR_STRING:
		case MYSQL_TYPE_VARCHAR:
		case MYSQL_TYPE_BLOB:
		case MYSQL_TYPE_TINY_BLOB:
		case MYSQL_TYPE_MEDIUM_BLOB:
		case MYSQL_TYPE_LONG_BLOB:
		default:
			return attribute(value);
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

void mysql_adapter::begin_transaction()
{
	query("START TRANSACTION");
}

void mysql_adapter::commit()
{
	query("COMMIT");
}

void mysql_adapter::rollback()
{
	query("ROLLBACK");
}

std::string mysql_adapter::escape(std::string_view str) const
{
	return connection::quote_name(str);
}

std::string mysql_adapter::default_values_insert_clause() const
{
	return "() VALUES ()";
}

} // namespace ar
