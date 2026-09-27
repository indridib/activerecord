// ============================================================================
// postgres_adapter.cpp
//
// SPDX-FileCopyrightText: 2020-2023 Indridi Bjornsson
// SPDX-License-Identifier: MIT
// See file LICENSE.MIT for detail
//
// Implementation of PostgreSQL adapter
// ============================================================================

#include <activerecord.h>
#include <adapters/postgres_adapter.h>
#include <cctype>
#include <cstdint>
#include <cstdlib>
#include <libpq-fe.h>
#include <sstream>
#include <string>

namespace ar {

namespace {

// RAII wrapper for PGresult to prevent resource leaks
class pg_result_guard {
  public:
	explicit pg_result_guard(PGresult* res) : result_(res) {}
	~pg_result_guard() {
		if (result_ != nullptr) {
			PQclear(result_);
		}
	}

	// Delete copy/move to enforce unique ownership
	pg_result_guard(const pg_result_guard&) = delete;
	pg_result_guard& operator=(const pg_result_guard&) = delete;
	pg_result_guard(pg_result_guard&&) = delete;
	pg_result_guard& operator=(pg_result_guard&&) = delete;

	PGresult* get() const noexcept { return result_; }
	PGresult* release() noexcept {
		PGresult* temp{ result_ };
		result_ = nullptr;
		return temp;
	}

  private:
	PGresult* result_;
};

std::string normalize_postgres_datetime(std::string value) {
	// Accept common PostgreSQL output forms:
	// - "YYYY-MM-DD HH:MM:SS"
	// - "YYYY-MM-DDTHH:MM:SS"
	// - with fractional seconds and/or timezone suffix.
	for (char& c : value) {
		if (c == 'T') {
			c = ' ';
		}
	}

	const std::size_t space_pos{ value.find(' ') };
	if (space_pos != std::string::npos) {
		std::size_t tz_pos{ value.find_first_of("Zz+-", space_pos + 1) };
		if (tz_pos != std::string::npos) {
			value.erase(tz_pos);
		}

		std::size_t frac_pos{ value.find('.', space_pos + 1) };
		if (frac_pos != std::string::npos) {
			value.erase(frac_pos);
		}
	}

	return value;
}

std::string convert_placeholders_to_postgres(std::string_view sql) {
	std::string out;
	out.reserve(sql.size() + 16);

	int param_index{ 1 };
	std::string dollar_tag;
	enum class state_t : std::uint8_t { normal, single_quote, double_quote, line_comment, block_comment, dollar_quote };
	state_t state = state_t::normal;

	auto is_tag_char = [](char c) { return std::isalnum(static_cast<unsigned char>(c)) != 0 || c == '_'; };

	for (std::size_t i = 0; i < sql.size(); ++i) {
		const char c = sql[i];

		switch (state) {
		case state_t::normal:
			if (c == '\'') {
				state = state_t::single_quote;
				out += c;
				continue;
			}
			if (c == '"') {
				state = state_t::double_quote;
				out += c;
				continue;
			}
			if (c == '-' && i + 1 < sql.size() && sql[i + 1] == '-') {
				state = state_t::line_comment;
				out += c;
				out += sql[++i];
				continue;
			}
			if (c == '/' && i + 1 < sql.size() && sql[i + 1] == '*') {
				state = state_t::block_comment;
				out += c;
				out += sql[++i];
				continue;
			}
			if (c == '$') {
				std::size_t j{ i + 1 };
				while (j < sql.size() && is_tag_char(sql[j])) {
					++j;
				}
				if (j < sql.size() && sql[j] == '$') {
					dollar_tag.assign(sql.substr(i, j - i + 1));
					state = state_t::dollar_quote;
					out += dollar_tag;
					i = j;
					continue;
				}
			}
			if (c == '?') {
				out += "$" + std::to_string(param_index++);
				continue;
			}
			out += c;
			break;

		case state_t::single_quote:
			out += c;
			if (c == '\'' && i + 1 < sql.size() && sql[i + 1] == '\'') {
				out += sql[++i];
			} else if (c == '\'') {
				state = state_t::normal;
			}
			break;

		case state_t::double_quote:
			out += c;
			if (c == '"' && i + 1 < sql.size() && sql[i + 1] == '"') {
				out += sql[++i];
			} else if (c == '"') {
				state = state_t::normal;
			}
			break;

		case state_t::line_comment:
			out += c;
			if (c == '\n') {
				state = state_t::normal;
			}
			break;

		case state_t::block_comment:
			out += c;
			if (c == '*' && i + 1 < sql.size() && sql[i + 1] == '/') {
				out += sql[++i];
				state = state_t::normal;
			}
			break;

		case state_t::dollar_quote:
			if (!dollar_tag.empty() && i + dollar_tag.size() <= sql.size() &&
				sql.substr(i, dollar_tag.size()) == dollar_tag) {
				out += dollar_tag;
				i += dollar_tag.size() - 1;
				state = state_t::normal;
				continue;
			}
			out += c;
			break;
		}
	}

	return out;
}
} // namespace

// ============================================================================
// postgres_statement implementation
// ============================================================================

postgres_statement::postgres_statement(PGconn* db) : statement(), db_(db) {}

postgres_statement::~postgres_statement() {
	if (result_ != nullptr) {
		PQclear(result_);
		result_ = nullptr;
	}
}

void postgres_statement::execute(const std::string& command, const attribute_vector& values) {
	if (values.empty()) {
		// Simple query without parameters
		result_ = PQexec(db_, command.c_str());
	} else {
		// Prepared statement with parameters
		// PostgreSQL uses $1, $2, etc. for placeholders.
		// Convert '?' while preserving literals/comments.
		std::string pg_command{ convert_placeholders_to_postgres(command) };

		// Convert attribute values to string arrays for PostgreSQL
		std::vector<std::string> param_strings;
		std::vector<const char*> param_values;
		param_strings.reserve(values.size());
		param_values.reserve(values.size());

		for (const auto& param : values) {
			switch (param.type()) {
			case type_null:
				param_values.push_back(nullptr);
				break;

			case type_string:
				param_strings.push_back(param.as_string());
				param_values.push_back(param_strings.back().c_str());
				break;

			case type_integer:
				param_strings.push_back(std::to_string(param.as_integer()));
				param_values.push_back(param_strings.back().c_str());
				break;

			case type_date:
				param_strings.push_back(std::to_string(static_cast<long long>(param.as_date().timestamp())));
				param_values.push_back(param_strings.back().c_str());
				break;

			case type_decimal:
				param_strings.push_back(std::to_string(param.as_real()));
				param_values.push_back(param_strings.back().c_str());
				break;

			case type_bool:
				param_strings.emplace_back(param.as_bool() ? "t" : "f");
				param_values.push_back(param_strings.back().c_str());
				break;

			default:
				param_values.push_back(nullptr);
				break;
			}
		}

		// Execute the parameterized query
		result_ = PQexecParams(db_, pg_command.c_str(), static_cast<int>(param_values.size()),
							   nullptr, // let server infer param types
							   param_values.data(),
							   nullptr, // param lengths (null for text)
							   nullptr, // param formats (null for text)
							   0);		// result format (0 = text)
	}

	// Check for errors
	if (result_ == nullptr) {
		throw ex::activerecord("PostgreSQL error: query returned no result");
	}
	ExecStatusType status{ PQresultStatus(result_) };
	if (status != PGRES_COMMAND_OK && status != PGRES_TUPLES_OK) {
		std::string error{ "PostgreSQL error: " };
		error += PQresultErrorMessage(result_);
		PQclear(result_);
		result_ = nullptr;
		throw ex::activerecord(error);
	}

	// Initialize fetch state
	current_row_ = -1;
	num_rows_ = PQntuples(result_);
	num_fields_ = PQnfields(result_);
	affected_rows_ = 0;
	if (const char* tuples = PQcmdTuples(result_); tuples != nullptr && tuples[0] != '\0') {
		affected_rows_ = static_cast<std::size_t>(std::strtoull(tuples, nullptr, 10));
	}
}

bool postgres_statement::fetch() {
	if (result_ == nullptr || num_rows_ == 0) {
		return false;
	}

	current_row_++;
	return current_row_ < num_rows_;
}

void postgres_statement::get_attributes(attribute_map& attributes) {
	if (result_ == nullptr || current_row_ < 0 || current_row_ >= num_rows_) {
		return;
	}

	for (int i = 0; i < num_fields_; ++i) {
		std::string column_name{ PQfname(result_, i) };
		attributes.emplace(column_name, field_as_attribute(result_, current_row_, i));
	}
}

attribute postgres_statement::field_as_attribute(PGresult* result, int row, int column) {
	// Check for NULL
	if (PQgetisnull(result, row, column) != 0) {
		return {};
	}

	// Get the value as a string
	const char* value_str = PQgetvalue(result, row, column);

	// Get the PostgreSQL type OID
	Oid type_oid{ PQftype(result, column) };

	// Convert based on PostgreSQL type
	// Common PostgreSQL type OIDs (from pg_type.h)
	// These are standard and should be consistent across PostgreSQL versions
	switch (type_oid) {
	case 16: // BOOLOID
		return { value_str[0] == 't' || value_str[0] == 'T' };

	case 20: // INT8OID
	case 21: // INT2OID
	case 23: // INT4OID
		return { std::stoll(value_str) };

	case 700:  // FLOAT4OID
	case 701:  // FLOAT8OID
	case 1700: // NUMERICOID
		return { std::stod(value_str) };

	case 1114: // TIMESTAMPOID
	case 1184: // TIMESTAMPTZOID
	case 1082: // DATEOID
		return { datetime::parse(normalize_postgres_datetime(std::string(value_str))) };

	case 25:   // TEXTOID
	case 1043: // VARCHAROID
	case 1042: // BPCHAROID
	default:
		// Text and unknown types - return as string
		return { std::string(value_str) };
	}
}

// ============================================================================
// postgres_adapter implementation
// ============================================================================

postgres_adapter::postgres_adapter(const std::string& host, const std::string& user, const std::string& password,
								   const std::string& database, unsigned int port)
	: connection() {
	// Build connection string
	std::ostringstream conninfo;
	conninfo << "host=" << host << " port=" << port << " dbname=" << database << " user=" << user
			 << " password=" << password << " client_encoding=UTF8";

	// Connect to database
	db_ = PQconnectdb(conninfo.str().c_str());

	if (PQstatus(db_) != CONNECTION_OK) {
		std::string error{ "PostgreSQL connection error: " };
		error += PQerrorMessage(db_);
		PQfinish(db_);
		throw ex::activerecord(error);
	}
}

postgres_adapter::~postgres_adapter() {
	if (db_ != nullptr) {
		PQfinish(db_);
		db_ = nullptr;
	}
}

std::unique_ptr<statement> postgres_adapter::query(const std::string& sql, const attribute_vector& values) {
	auto stmt{ std::make_unique<postgres_statement>(db_) };
	stmt->execute(sql, values);
	return stmt;
}

void postgres_adapter::limit(std::string& sql, long offset, long limit) const {
	// PostgreSQL uses LIMIT with optional OFFSET
	if (limit >= 0) {
		sql += " LIMIT " + std::to_string(limit);
		if (offset >= 0) {
			sql += " OFFSET " + std::to_string(offset);
		}
	}
}

std::size_t postgres_adapter::insert_id() const {
	if (db_ == nullptr) {
		return 0;
	}

	// Use RAII wrapper for automatic cleanup
	pg_result_guard result_guard(PQexec(db_, "SELECT LASTVAL()"));
	PGresult* result{ result_guard.get() };

	if (result == nullptr) {
		return 0;
	}

	const ExecStatusType status{ PQresultStatus(result) };
	if (status != PGRES_TUPLES_OK || PQntuples(result) != 1 || PQnfields(result) != 1) {
		return 0;
	}

	const char* value = PQgetvalue(result, 0, 0);
	return (value != nullptr) ? static_cast<std::size_t>(std::strtoull(value, nullptr, 10)) : 0;
}

void postgres_adapter::begin_transaction() {
	query("BEGIN");
}

void postgres_adapter::commit() {
	query("COMMIT");
}

void postgres_adapter::rollback() {
	query("ROLLBACK");
}

std::string postgres_adapter::escape(std::string_view str) const {
	std::string escaped;
	escaped.reserve(str.size() + 2);
	escaped += '"';
	for (const char c : str) {
		if (c == '"') {
			escaped += '"';
		}
		escaped += c;
	}
	escaped += '"';
	return escaped;
}

} // namespace ar
