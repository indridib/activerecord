// ============================================================================
// mysql_adapter.h
//
// SPDX-FileCopyrightText: 2020-2023 Indridi Bjornsson
// SPDX-License-Identifier: MIT
// See file LICENSE.MIT for detail
//
// A MySQL adapter class using MySQL C API
// ============================================================================

#pragma once

#include <activerecord.h>
#include <connection.h>

// Forward declarations for MySQL types (actual header included in .cpp)
struct MYSQL;
struct MYSQL_STMT;
struct MYSQL_BIND;
struct MYSQL_RES;

namespace ar {

/**
 * MySQL statement class
 */
class mysql_statement : public statement {
  public:
	mysql_statement(MYSQL* db);
	~mysql_statement() override;

	void execute(const std::string& command, const attribute_vector& values) override;
	bool fetch() override;
	void get_attributes(attribute_map& attributes) override;
	std::size_t affected_rows() const noexcept override { return affected_rows_; }

  private:
	static attribute field_as_attribute(MYSQL_RES* metadata, MYSQL_BIND* bind, unsigned long* lengths,
										int column_index);

	// Helper struct for RAII management of MYSQL_BIND resources
	struct bind_buffer {
		std::vector<char> data;
		using mysql_null_flag = unsigned char;
		std::unique_ptr<mysql_null_flag> is_null;

		bind_buffer() : is_null(std::make_unique<mysql_null_flag>(0)) {}
	};

	MYSQL* db_;							   // Non-owning pointer to database connection
	MYSQL_STMT* stmt_;					   // Owned prepared statement (closed in destructor)
	MYSQL_RES* metadata_;				   // Result metadata
	std::vector<MYSQL_BIND> result_binds_; // Result bindings (RAII)
	std::vector<unsigned long> lengths_;   // Field lengths (RAII)
	std::vector<bind_buffer> buffers_;	   // Buffers for result data (RAII)
	int num_fields_;					   // Number of result fields
	bool has_result_;					   // Whether statement has results
	std::size_t affected_rows_;			   // Rows affected by last execute
};

/**
 * Adapter for MySQL database
 */
class mysql_adapter : public connection {
  public:
	mysql_adapter(const std::string& host, const std::string& user, const std::string& password,
				  const std::string& database, unsigned int port = 3306);
	~mysql_adapter() override;

	std::unique_ptr<statement> query(const std::string& sql, const attribute_vector& values = {}) override;
	void limit(std::string& sql, long offset, long limit) const override;
	std::size_t insert_id() const override;
	void begin_transaction() override;
	void commit() override;
	void rollback() override;
	std::string escape(std::string_view str) const override;
	std::string default_values_insert_clause() const override;

	bool accepts_limit_and_order_for_update_and_delete() const override { return true; }

  private:
	MYSQL* db_{ nullptr }; // Owned database handle (closed in destructor)
};

} // namespace ar
