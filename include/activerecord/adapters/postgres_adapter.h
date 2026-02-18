// ============================================================================
// postgres_adapter.h
//
// SPDX-FileCopyrightText: 2020-2023 Indridi Bjornsson
// SPDX-License-Identifier: MIT
// See file LICENSE.MIT for detail
//
// A PostgreSQL adapter class using libpq
// ============================================================================

#pragma once

#include <activerecord.h>
#include <connection.h>

// Forward declarations for PostgreSQL types (actual header included in .cpp)
struct pg_conn;
struct pg_result;
typedef struct pg_conn PGconn;
typedef struct pg_result PGresult;

namespace ar {

/**
 * PostgreSQL statement class
 */
class postgres_statement : public statement {
  public:
	postgres_statement(PGconn* db);
	~postgres_statement() override;

	void execute(const std::string& command, const attribute_vector& values) override;
	bool fetch() override;
	void get_attributes(attribute_map& attributes) override;
	std::size_t affected_rows() const noexcept override { return affected_rows_; }

  private:
	static attribute field_as_attribute(PGresult* result, int row, int column);

	PGconn* db_;				// Non-owning pointer to database connection
	PGresult* result_;			// Owned result set (cleared in destructor)
	int current_row_;			// Current row index
	int num_rows_;				// Total number of rows
	int num_fields_;			// Number of columns
	std::size_t affected_rows_; // Rows affected by last execute
};

/**
 * Adapter for PostgreSQL database
 */
class postgres_adapter : public connection {
  public:
	postgres_adapter(const std::string& host, const std::string& user, const std::string& password,
					 const std::string& database, unsigned int port = 5432);
	~postgres_adapter() override;

	std::unique_ptr<statement> query(const std::string& sql, const attribute_vector& values = {}) override;
	void limit(std::string& sql, long offset, long limit) const override;
	std::size_t insert_id() const override;
	void begin_transaction() override;
	void commit() override;
	void rollback() override;
	std::string escape(std::string_view str) const override;

	bool accepts_limit_and_order_for_update_and_delete() const override { return true; }

  private:
	PGconn* db_{ nullptr }; // Owned database connection (closed in destructor)
};

} // namespace ar
