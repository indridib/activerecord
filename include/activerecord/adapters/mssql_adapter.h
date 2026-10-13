// ============================================================================
// mssql_adapter.h
//
// SPDX-FileCopyrightText: 2020-2026 Indridi Bjornsson
// SPDX-License-Identifier: MIT
// See file LICENSE for details
//
// A Microsoft SQL Server adapter class using ODBC
//

#pragma once

#include <activerecord/activerecord.h>
#include <activerecord/connection.h>

// Forward declarations for ODBC types (actual header included in .cpp). Both the Windows SDK and
// unixODBC define these handles as void*, so these redeclarations are identical to theirs.
using SQLHANDLE = void*;
using SQLHENV = void*;
using SQLHDBC = void*;
using SQLHSTMT = void*;

namespace ar {

/**
 * MS-SQL statement class using ODBC
 */
class mssql_statement : public statement {
  public:
	mssql_statement(SQLHDBC dbc);
	~mssql_statement() override;

	void execute(const std::string& command, const attribute_vector& values) override;
	bool fetch() override;
	void get_attributes(attribute_map& attributes) override;
	std::size_t affected_rows() const noexcept override { return affected_rows_; }

	// After executing "INSERT ...; SELECT SCOPE_IDENTITY()": skip the row-count results of the
	// INSERT (and of any triggers) and return the selected identity, or 0 if there is none.
	std::size_t read_insert_id();

  private:
	static attribute field_as_attribute(SQLHSTMT stmt, unsigned short column_index); // SQLUSMALLINT

	SQLHDBC dbc_;					  // Non-owning pointer to database connection
	SQLHSTMT stmt_{ nullptr };		  // Owned statement handle (freed in destructor)
	unsigned short num_columns_{ 0 }; // Number of columns in result
	bool has_data_{ false };		  // Whether we have result data
	std::size_t affected_rows_{ 0 };  // Rows affected by last execute
};

/**
 * Adapter for Microsoft SQL Server database via ODBC
 */
class mssql_adapter : public connection {
  public:
	// Connection via DSN
	mssql_adapter(const std::string& dsn, const std::string& user, const std::string& password);

	// Connection via connection string
	explicit mssql_adapter(const std::string& connection_string);

	// Connection via server, database, user, password
	mssql_adapter(const std::string& server, const std::string& database, const std::string& user,
				  const std::string& password, unsigned int port = 1433);

	~mssql_adapter() override;

	std::unique_ptr<statement> query(const std::string& sql, const attribute_vector& values = {}) override;
	void limit(std::string& sql, long offset, long limit) const override;
	std::size_t insert_id() const override;
	void begin_transaction() override;
	void commit() override;
	void rollback() override;
	std::string escape(std::string_view str) const override;

	bool accepts_limit_and_order_for_update_and_delete() const override { return true; }

  private:
	void connect(const std::string& connection_string);

	SQLHENV env_{ nullptr };		  // ODBC environment handle
	SQLHDBC dbc_{ nullptr };		  // ODBC connection handle
	std::size_t last_insert_id_{ 0 }; // Identity of the row inserted by the last INSERT via query()
};

} // namespace ar
