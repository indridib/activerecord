// ============================================================================
// sqlite_adapter.h
//
// SPDX-FileCopyrightText: 2020-2023 Indridi Bjornsson
// SPDX-License-Identifier: MIT
// See file LICENSE.MIT for detail
//
// A SQLite adapter class using sqlite3.h directly
// ============================================================================

#pragma once

#include <activerecord.h>
#include <connection.h>

// Forward declarations for SQLite types (actual header included in .cpp)
struct sqlite3;
struct sqlite3_stmt;

namespace ar {

/**
 * SQLite statement class
 */
class sqlite_statement : public statement {
public:
	sqlite_statement(sqlite3* db);
	~sqlite_statement() override;

	void execute(const std::string& command, const attribute_vector& values) override;
	bool fetch() override;
	void get_attributes(attribute_map& attributes) override;
	std::size_t affected_rows() const noexcept override { return affected_rows_; }

private:
	static attribute field_as_attribute(sqlite3_stmt* stmt, int column_index);

	sqlite3* db_;           // Non-owning pointer to database connection
	sqlite3_stmt* stmt_;    // Owned prepared statement (finalized in destructor)
	int first_step_result_; // Result from first sqlite3_step() call
	bool used_first_result_;// Whether first result has been consumed by fetch()
	std::size_t affected_rows_;
};

/**
 * Adapter for SQLite database
 */
class sqlite_adapter : public connection {
public:
	sqlite_adapter(const std::string& db_path, const std::string& user = {}, const std::string& password = {});
	~sqlite_adapter() override;

	std::unique_ptr<statement> query(const std::string& sql, const attribute_vector& values = {}) override;
	void limit(std::string& sql, long offset, long limit) const override;
	std::size_t insert_id() const override;

	void begin_transaction() override;
	void commit() override;
	void rollback() override;

	bool accepts_limit_and_order_for_update_and_delete() const override { return true; }

private:
	sqlite3* db_;  // Owned database handle (closed in destructor)
};

} // namespace ar
