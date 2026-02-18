// ============================================================================
// mssql_adapter.cpp
//
// SPDX-FileCopyrightText: 2020-2023 Indridi Bjornsson
// SPDX-License-Identifier: MIT
// See file LICENSE.MIT for detail
//
// Implementation of MS-SQL Server adapter using ODBC
// ============================================================================

#include <activerecord.h>
#include <adapters/mssql_adapter.h>

#ifdef _WIN32
#include <windows.h>
#endif
#include <cctype>
#include <cstring>
#include <sql.h>
#include <sqlext.h>
#include <sstream>
#include <string>

namespace ar {

// Helper function to check ODBC return codes
static void check_odbc_error(SQLRETURN ret, SQLSMALLINT handle_type, SQLHANDLE handle, const std::string& context) {
	if (ret != SQL_SUCCESS && ret != SQL_SUCCESS_WITH_INFO) {
		SQLCHAR sql_state[6];
		SQLCHAR message[SQL_MAX_MESSAGE_LENGTH];
		SQLINTEGER native_error;
		SQLSMALLINT text_length;

		SQLGetDiagRec(handle_type, handle, 1, sql_state, &native_error, message, sizeof(message), &text_length);

		std::string error{ context + ": " };
		error += reinterpret_cast<char*>(message);
		error += " (SQLSTATE: " + std::string(reinterpret_cast<char*>(sql_state)) + ")";

		throw ex::activerecord(error);
	}
}

namespace {
std::string normalize_mssql_datetime(std::string value) {
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

bool contains_order_by(std::string_view sql) {
	for (std::size_t i{ 0 }; i + 8 <= sql.size(); ++i) {
		if (std::tolower(static_cast<unsigned char>(sql[i])) == 'o' &&
			std::tolower(static_cast<unsigned char>(sql[i + 1])) == 'r' &&
			std::tolower(static_cast<unsigned char>(sql[i + 2])) == 'd' &&
			std::tolower(static_cast<unsigned char>(sql[i + 3])) == 'e' &&
			std::tolower(static_cast<unsigned char>(sql[i + 4])) == 'r' &&
			std::isspace(static_cast<unsigned char>(sql[i + 5])) &&
			std::tolower(static_cast<unsigned char>(sql[i + 6])) == 'b' &&
			std::tolower(static_cast<unsigned char>(sql[i + 7])) == 'y') {
			return true;
		}
	}
	return false;
}
} // namespace

// ============================================================================
// mssql_statement implementation
// ============================================================================

mssql_statement::mssql_statement(SQLHDBC dbc)
	: statement(), dbc_(dbc), stmt_(nullptr), num_columns_(0), has_data_(false), affected_rows_(0) {
	SQLRETURN ret{ SQLAllocHandle(SQL_HANDLE_STMT, dbc_, &stmt_) };
	check_odbc_error(ret, SQL_HANDLE_DBC, dbc_, "Failed to allocate statement handle");
}

mssql_statement::~mssql_statement() {
	if (stmt_) {
		SQLFreeHandle(SQL_HANDLE_STMT, stmt_);
		stmt_ = nullptr;
	}
}

void mssql_statement::execute(const std::string& command, const attribute_vector& values) {
	SQLRETURN ret;

	// Prepare the statement
	ret = SQLPrepare(stmt_, (SQLCHAR*)command.c_str(), SQL_NTS);
	check_odbc_error(ret, SQL_HANDLE_STMT, stmt_, "SQLPrepare failed");

	// Bind parameters if any
	if (!values.empty()) {
		// Storage for parameter data
		std::vector<SQLLEN> indicators(values.size());
		std::vector<SQLBIGINT> int_values(values.size());
		std::vector<double> double_values(values.size());
		std::vector<std::string> string_values(values.size());

		for (size_t i = 0; i < values.size(); ++i) {
			SQLUSMALLINT param_number{ static_cast<SQLUSMALLINT>(i + 1) };
			const attribute& param = values[i];

			switch (param.type()) {
			case type_null:
				indicators[i] = SQL_NULL_DATA;
				ret = SQLBindParameter(stmt_, param_number, SQL_PARAM_INPUT, SQL_C_SLONG, SQL_INTEGER, 0, 0, nullptr, 0,
									   &indicators[i]);
				break;

			case type_string:
				string_values[i] = param.as_string();
				indicators[i] = SQL_NTS;
				ret = SQLBindParameter(stmt_, param_number, SQL_PARAM_INPUT, SQL_C_CHAR, SQL_VARCHAR,
									   string_values[i].length(), 0, (SQLPOINTER)string_values[i].c_str(),
									   string_values[i].length(), &indicators[i]);
				break;

			case type_integer:
				int_values[i] = param.as_integer();
				indicators[i] = 0;
				ret = SQLBindParameter(stmt_, param_number, SQL_PARAM_INPUT, SQL_C_SBIGINT, SQL_BIGINT, 0, 0,
									   &int_values[i], 0, &indicators[i]);
				break;

			case type_date:
				int_values[i] = static_cast<SQLBIGINT>(param.as_date().timestamp());
				indicators[i] = 0;
				ret = SQLBindParameter(stmt_, param_number, SQL_PARAM_INPUT, SQL_C_SBIGINT, SQL_BIGINT, 0, 0,
									   &int_values[i], 0, &indicators[i]);
				break;

			case type_decimal:
				double_values[i] = param.as_real();
				indicators[i] = 0;
				ret = SQLBindParameter(stmt_, param_number, SQL_PARAM_INPUT, SQL_C_DOUBLE, SQL_DOUBLE, 0, 0,
									   &double_values[i], 0, &indicators[i]);
				break;

			case type_bool:
				int_values[i] = param.as_bool() ? 1 : 0;
				indicators[i] = 0;
				ret = SQLBindParameter(stmt_, param_number, SQL_PARAM_INPUT, SQL_C_BIT, SQL_BIT, 0, 0, &int_values[i],
									   0, &indicators[i]);
				break;

			default:
				indicators[i] = SQL_NULL_DATA;
				ret = SQLBindParameter(stmt_, param_number, SQL_PARAM_INPUT, SQL_C_SLONG, SQL_INTEGER, 0, 0, nullptr, 0,
									   &indicators[i]);
				break;
			}

			check_odbc_error(ret, SQL_HANDLE_STMT, stmt_, "SQLBindParameter failed");
		}

		// Execute while bound parameter storage is still alive.
		ret = SQLExecute(stmt_);
		check_odbc_error(ret, SQL_HANDLE_STMT, stmt_, "SQLExecute failed");
	} else {
		// Execute the statement
		ret = SQLExecute(stmt_);
		check_odbc_error(ret, SQL_HANDLE_STMT, stmt_, "SQLExecute failed");
	}

	// Get number of columns
	SQLSMALLINT cols;
	ret = SQLNumResultCols(stmt_, &cols);
	check_odbc_error(ret, SQL_HANDLE_STMT, stmt_, "SQLNumResultCols failed");

	num_columns_ = cols;
	has_data_ = (num_columns_ > 0);
	affected_rows_ = 0;
	SQLLEN row_count{ 0 };
	ret = SQLRowCount(stmt_, &row_count);
	if ((ret == SQL_SUCCESS || ret == SQL_SUCCESS_WITH_INFO) && row_count > 0) {
		affected_rows_ = static_cast<std::size_t>(row_count);
	}
}

bool mssql_statement::fetch() {
	if (!has_data_) {
		return false;
	}

	SQLRETURN ret{ SQLFetch(stmt_) };

	if (ret == SQL_SUCCESS || ret == SQL_SUCCESS_WITH_INFO) {
		return true;
	} else if (ret == SQL_NO_DATA) {
		return false;
	} else {
		check_odbc_error(ret, SQL_HANDLE_STMT, stmt_, "SQLFetch failed");
		return false;
	}
}

void mssql_statement::get_attributes(attribute_map& attributes) {
	if (!has_data_ || num_columns_ == 0) {
		return;
	}

	for (int i = 1; i <= num_columns_; ++i) {
		// Get column name
		SQLCHAR column_name[256];
		SQLSMALLINT name_length;
		SQLRETURN ret = SQLDescribeCol(stmt_, i, column_name, sizeof(column_name), &name_length, nullptr, nullptr,
									   nullptr, nullptr);
		check_odbc_error(ret, SQL_HANDLE_STMT, stmt_, "SQLDescribeCol failed");

		std::string col_name{ reinterpret_cast<char*>(column_name) };
		attributes.emplace(col_name, field_as_attribute(stmt_, i));
	}
}

attribute mssql_statement::field_as_attribute(SQLHSTMT stmt, int column_index) {
	// Get column type
	SQLSMALLINT data_type;
	SQLRETURN ret = SQLDescribeCol(stmt, column_index, nullptr, 0, nullptr, &data_type, nullptr, nullptr, nullptr);

	if (ret != SQL_SUCCESS && ret != SQL_SUCCESS_WITH_INFO) {
		return attribute();
	}

	// Read text data in chunks to avoid fixed-size truncation.
	SQLLEN indicator{ 0 };
	std::string value_str;
	value_str.reserve(128);
	constexpr SQLLEN chunk_size = 2048;
	char chunk[chunk_size];

	while (true) {
		std::memset(chunk, 0, sizeof(chunk));
		ret = SQLGetData(stmt, column_index, SQL_C_CHAR, chunk, sizeof(chunk), &indicator);

		if (indicator == SQL_NULL_DATA) {
			return attribute();
		}
		if (ret != SQL_SUCCESS && ret != SQL_SUCCESS_WITH_INFO) {
			return attribute();
		}

		value_str += chunk;

		// SQL_SUCCESS means full value consumed in this call.
		if (ret == SQL_SUCCESS) {
			break;
		}
	}

	// Convert based on SQL type
	switch (data_type) {
	case SQL_BIT:
		return attribute(!value_str.empty() && (value_str[0] == '1' || value_str[0] == 't' || value_str[0] == 'T'));

	case SQL_TINYINT:
	case SQL_SMALLINT:
	case SQL_INTEGER:
	case SQL_BIGINT:
		return attribute(std::stoll(value_str));

	case SQL_REAL:
	case SQL_FLOAT:
	case SQL_DOUBLE:
	case SQL_DECIMAL:
	case SQL_NUMERIC:
		return attribute(std::stod(value_str));

	case SQL_TYPE_DATE:
	case SQL_TYPE_TIMESTAMP:
		return attribute(datetime::parse(normalize_mssql_datetime(value_str)));

	case SQL_TYPE_TIME:
		return attribute(datetime::parse(std::string("1970-01-01 ") + normalize_mssql_datetime(value_str)));

	case SQL_CHAR:
	case SQL_VARCHAR:
	case SQL_LONGVARCHAR:
	case SQL_WCHAR:
	case SQL_WVARCHAR:
	case SQL_WLONGVARCHAR:
	default:
		return attribute(value_str);
	}
}

// ============================================================================
// mssql_adapter implementation
// ============================================================================

mssql_adapter::mssql_adapter(const std::string& dsn, const std::string& user, const std::string& password)
	: connection() {
	std::ostringstream connstr;
	connstr << "DSN=" << dsn << ";UID=" << user << ";PWD=" << password << ";";
	connect(connstr.str());
}

mssql_adapter::mssql_adapter(const std::string& connection_string) : connection() {
	connect(connection_string);
}

mssql_adapter::mssql_adapter(const std::string& server, const std::string& database, const std::string& user,
							 const std::string& password, unsigned int port)
	: connection() {
	std::ostringstream connstr;
	connstr << "DRIVER={ODBC Driver 17 for SQL Server};"
			<< "SERVER=" << server << "," << port << ";"
			<< "DATABASE=" << database << ";"
			<< "UID=" << user << ";"
			<< "PWD=" << password << ";";
	connect(connstr.str());
}

void mssql_adapter::connect(const std::string& connection_string) {
	SQLRETURN ret;

	// Allocate environment handle
	ret = SQLAllocHandle(SQL_HANDLE_ENV, SQL_NULL_HANDLE, &env_);
	check_odbc_error(ret, SQL_HANDLE_ENV, env_, "Failed to allocate environment handle");

	// Set ODBC version
	ret = SQLSetEnvAttr(env_, SQL_ATTR_ODBC_VERSION, (SQLPOINTER)SQL_OV_ODBC3, 0);
	check_odbc_error(ret, SQL_HANDLE_ENV, env_, "Failed to set ODBC version");

	// Allocate connection handle
	ret = SQLAllocHandle(SQL_HANDLE_DBC, env_, &dbc_);
	check_odbc_error(ret, SQL_HANDLE_ENV, env_, "Failed to allocate connection handle");

	// Connect to database
	SQLCHAR out_conn_str[1024];
	SQLSMALLINT out_conn_str_len;

	ret = SQLDriverConnect(dbc_, nullptr, (SQLCHAR*)connection_string.c_str(), SQL_NTS, out_conn_str,
						   sizeof(out_conn_str), &out_conn_str_len, SQL_DRIVER_NOPROMPT);

	if (ret != SQL_SUCCESS && ret != SQL_SUCCESS_WITH_INFO) {
		try {
			check_odbc_error(ret, SQL_HANDLE_DBC, dbc_, "Failed to connect to database");
		} catch (...) {
			SQLFreeHandle(SQL_HANDLE_DBC, dbc_);
			SQLFreeHandle(SQL_HANDLE_ENV, env_);
			dbc_ = nullptr;
			env_ = nullptr;
			throw;
		}
	}
}

mssql_adapter::~mssql_adapter() {
	if (dbc_) {
		SQLDisconnect(dbc_);
		SQLFreeHandle(SQL_HANDLE_DBC, dbc_);
		dbc_ = nullptr;
	}
	if (env_) {
		SQLFreeHandle(SQL_HANDLE_ENV, env_);
		env_ = nullptr;
	}
}

std::unique_ptr<statement> mssql_adapter::query(const std::string& sql, const attribute_vector& values) {
	auto stmt{ std::make_unique<mssql_statement>(dbc_) };
	stmt->execute(sql, values);
	return stmt;
}

void mssql_adapter::limit(std::string& sql, long offset, long limit) const {
	// SQL Server 2012+ uses OFFSET ... ROWS FETCH NEXT ... ROWS ONLY
	// This requires an ORDER BY clause
	if (offset >= 0 || limit >= 0) {
		if (!contains_order_by(sql)) {
			sql += " ORDER BY (SELECT 0)";
		}
		if (offset >= 0) {
			sql += " OFFSET " + std::to_string(offset) + " ROWS";
		} else {
			sql += " OFFSET 0 ROWS";
		}

		if (limit >= 0) {
			sql += " FETCH NEXT " + std::to_string(limit) + " ROWS ONLY";
		}
	}
}

std::size_t mssql_adapter::insert_id() const {
	if (!dbc_) {
		return 0;
	}
	SQLHSTMT stmt{ nullptr };
	SQLRETURN ret{ SQLAllocHandle(SQL_HANDLE_STMT, dbc_, &stmt) };
	if (ret != SQL_SUCCESS && ret != SQL_SUCCESS_WITH_INFO) {
		return 0;
	}

	const char* sql = "SELECT CAST(SCOPE_IDENTITY() AS BIGINT)";
	ret = SQLExecDirect(stmt, (SQLCHAR*)sql, SQL_NTS);
	if (ret != SQL_SUCCESS && ret != SQL_SUCCESS_WITH_INFO) {
		SQLFreeHandle(SQL_HANDLE_STMT, stmt);
		return 0;
	}

	SQLBIGINT id{ 0 };
	SQLLEN indicator{ 0 };
	ret = SQLBindCol(stmt, 1, SQL_C_SBIGINT, &id, 0, &indicator);
	if (ret != SQL_SUCCESS && ret != SQL_SUCCESS_WITH_INFO) {
		SQLFreeHandle(SQL_HANDLE_STMT, stmt);
		return 0;
	}

	ret = SQLFetch(stmt);
	SQLFreeHandle(SQL_HANDLE_STMT, stmt);
	if ((ret == SQL_SUCCESS || ret == SQL_SUCCESS_WITH_INFO) && indicator != SQL_NULL_DATA) {
		return static_cast<std::size_t>(id);
	}
	return 0;
}

void mssql_adapter::begin_transaction() {
	query("BEGIN TRANSACTION");
}

void mssql_adapter::commit() {
	query("COMMIT TRANSACTION");
}

void mssql_adapter::rollback() {
	query("ROLLBACK TRANSACTION");
}

std::string mssql_adapter::escape(std::string_view str) const {
	std::string escaped;
	escaped.reserve(str.size() + 2);
	escaped += '[';
	for (const char c : str) {
		if (c == ']') {
			escaped += ']';
		}
		escaped += c;
	}
	escaped += ']';
	return escaped;
}

} // namespace ar
