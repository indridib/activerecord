// ============================================================================
// postgres_adapter.cpp
//
// SPDX-FileCopyrightText: 2020-2023 Indridi Bjornsson
// SPDX-License-Identifier: MIT
// See file LICENSE.MIT for detail
//
// Implementation of PostgreSQL adapter
// ============================================================================

#include <adapters/postgres_adapter.h>
#include <activerecord.h>
#include <libpq-fe.h>
#include <sstream>

namespace ar {

// ============================================================================
// postgres_statement implementation
// ============================================================================

postgres_statement::postgres_statement(PGconn* db)
	: statement()
	, db_(db)
	, result_(nullptr)
	, current_row_(-1)
	, num_rows_(0)
	, num_fields_(0)
{}

postgres_statement::~postgres_statement()
{
	if (result_) {
		PQclear(result_);
		result_ = nullptr;
	}
}

void postgres_statement::execute(const std::string& command, const attribute_vector& values)
{
	if (values.empty()) {
		// Simple query without parameters
		result_ = PQexec(db_, command.c_str());
	} else {
		// Prepared statement with parameters
		// PostgreSQL uses $1, $2, etc. for placeholders
		// We need to convert ? to $1, $2, etc.
		std::string pg_command = command;
		int param_index = 1;
		size_t pos = 0;
		while ((pos = pg_command.find('?', pos)) != std::string::npos) {
			pg_command.replace(pos, 1, "$" + std::to_string(param_index++));
			pos += 2;
		}

		// Convert attribute values to string arrays for PostgreSQL
		std::vector<std::string> param_strings;
		std::vector<const char*> param_values;
		param_strings.reserve(values.size());
		param_values.reserve(values.size());

		for (const auto& param : values) {
			switch(param.type()) {
				case type_null:
					param_values.push_back(nullptr);
					break;

				case type_string:
					param_strings.push_back(param.as_string());
					param_values.push_back(param_strings.back().c_str());
					break;

				case type_integer:
				case type_date:
					param_strings.push_back(std::to_string(param.as_integer()));
					param_values.push_back(param_strings.back().c_str());
					break;

				case type_decimal:
					param_strings.push_back(std::to_string(param.as_real()));
					param_values.push_back(param_strings.back().c_str());
					break;

				case type_bool:
					param_strings.push_back(param.as_bool() ? "t" : "f");
					param_values.push_back(param_strings.back().c_str());
					break;

				default:
					param_values.push_back(nullptr);
					break;
			}
		}

		// Execute the parameterized query
		result_ = PQexecParams(db_,
		                      pg_command.c_str(),
		                      static_cast<int>(param_values.size()),
		                      nullptr,  // let server infer param types
		                      param_values.data(),
		                      nullptr,  // param lengths (null for text)
		                      nullptr,  // param formats (null for text)
		                      0);       // result format (0 = text)
	}

	// Check for errors
	ExecStatusType status = PQresultStatus(result_);
	if (status != PGRES_COMMAND_OK && status != PGRES_TUPLES_OK) {
		std::string error = "PostgreSQL error: ";
		error += PQresultErrorMessage(result_);
		PQclear(result_);
		result_ = nullptr;
		throw ex::activerecord(error);
	}

	// Initialize fetch state
	current_row_ = -1;
	num_rows_ = PQntuples(result_);
	num_fields_ = PQnfields(result_);
}

bool postgres_statement::fetch()
{
	if (!result_ || num_rows_ == 0) {
		return false;
	}

	current_row_++;
	return current_row_ < num_rows_;
}

void postgres_statement::get_attributes(attribute_map& attributes)
{
	if (!result_ || current_row_ < 0 || current_row_ >= num_rows_) {
		return;
	}

	for (int i = 0; i < num_fields_; ++i) {
		std::string column_name = PQfname(result_, i);
		attributes.emplace(column_name, field_as_attribute(result_, current_row_, i));
	}
}

attribute postgres_statement::field_as_attribute(PGresult* result, int row, int column)
{
	// Check for NULL
	if (PQgetisnull(result, row, column)) {
		return attribute();
	}

	// Get the value as a string
	const char* value_str = PQgetvalue(result, row, column);

	// Get the PostgreSQL type OID
	Oid type_oid = PQftype(result, column);

	// Convert based on PostgreSQL type
	// Common PostgreSQL type OIDs (from pg_type.h)
	// These are standard and should be consistent across PostgreSQL versions
	switch(type_oid) {
		case 16:  // BOOLOID
			return attribute(value_str[0] == 't' || value_str[0] == 'T');

		case 20:  // INT8OID
		case 21:  // INT2OID
		case 23:  // INT4OID
			return attribute(std::stoll(value_str));

		case 700: // FLOAT4OID
		case 701: // FLOAT8OID
		case 1700: // NUMERICOID
			return attribute(std::stod(value_str));

		case 1114: // TIMESTAMPOID
		case 1184: // TIMESTAMPTZOID
		case 1082: // DATEOID
			// For now, return dates as integers (could be enhanced)
			// In production, you'd parse the timestamp string
			return attribute();

		case 25:  // TEXTOID
		case 1043: // VARCHAROID
		case 1042: // BPCHAROID
		default:
			// Text and unknown types - return as string
			return attribute(std::string(value_str));
	}
}

// ============================================================================
// postgres_adapter implementation
// ============================================================================

postgres_adapter::postgres_adapter(const std::string& host, const std::string& user,
                                   const std::string& password, const std::string& database,
                                   unsigned int port)
	: connection()
	, db_(nullptr)
{
	// Build connection string
	std::ostringstream conninfo;
	conninfo << "host=" << host
	        << " port=" << port
	        << " dbname=" << database
	        << " user=" << user
	        << " password=" << password
	        << " client_encoding=UTF8";

	// Connect to database
	db_ = PQconnectdb(conninfo.str().c_str());

	if (PQstatus(db_) != CONNECTION_OK) {
		std::string error = "PostgreSQL connection error: ";
		error += PQerrorMessage(db_);
		PQfinish(db_);
		throw ex::activerecord(error);
	}
}

postgres_adapter::~postgres_adapter()
{
	if (db_) {
		PQfinish(db_);
		db_ = nullptr;
	}
}

std::unique_ptr<statement> postgres_adapter::query(const std::string& sql, const attribute_vector& values)
{
	auto stmt = std::make_unique<postgres_statement>(db_);
	stmt->execute(sql, values);
	return stmt;
}

void postgres_adapter::limit(std::string& sql, long offset, long limit) const
{
	// PostgreSQL uses LIMIT with optional OFFSET
	if (limit >= 0) {
		sql += " LIMIT " + std::to_string(limit);
		if (offset >= 0) {
			sql += " OFFSET " + std::to_string(offset);
		}
	}
}

std::size_t postgres_adapter::insert_id() const
{
	if (!db_) {
		return 0;
	}
	// PostgreSQL requires using RETURNING id clause or lastval() function
	// TODO: Implement proper RETURNING support in INSERT query generation
	// For now, this is a placeholder that returns 0
	return 0;
}

} // namespace ar
