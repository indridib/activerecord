// ============================================================================
// test_adapters.h
//
// SPDX-FileCopyrightText: 2020-2026 Indridi Bjornsson
// SPDX-License-Identifier: MIT
// See file LICENSE for details
//
// Shared support for integration tests that run against every enabled adapter:
// connection settings, per-adapter DDL fragments, and a parameterized fixture that
// connects (failing, not skipping, when an enabled adapter is unreachable), creates
// the fixture's tables and drops them afterwards.
//

#pragma once

#include <activerecord/activerecord.h>
#include <activerecord/config.h>
#include <activerecord/connection.h>
#include <cstdint>
#include <format>
#include <gtest/gtest.h>
#include <memory>
#include <ranges>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#ifdef AR_SQLITE
#include <activerecord/adapters/sqlite_adapter.h>
#endif
#ifdef AR_MYSQL
#include <activerecord/adapters/mysql_adapter.h>
#endif
#ifdef AR_POSTGRES
#include <activerecord/adapters/postgres_adapter.h>
#endif
#ifdef AR_MSSQL
#include <activerecord/adapters/mssql_adapter.h>
#endif

namespace ar::test {

enum class AdapterType : std::uint8_t { SQLite, MySQL, PostgreSQL, MSSQL };

inline std::string adapter_name(AdapterType type) {
	switch (type) {
	case AdapterType::SQLite:
		return "SQLite";
	case AdapterType::MySQL:
		return "MySQL";
	case AdapterType::PostgreSQL:
		return "PostgreSQL";
	case AdapterType::MSSQL:
		return "MSSQL";
	}
	return "Unknown";
}

// gtest parameter-name generator: AllAdapters/<Fixture>.<Test>/<Adapter>
inline std::string adapter_param_name(const ::testing::TestParamInfo<AdapterType>& info) {
	return adapter_name(info.param);
}

// Adapters compiled into this build
inline std::vector<AdapterType> enabled_adapters() {
	std::vector<AdapterType> adapters;
#ifdef AR_SQLITE
	adapters.push_back(AdapterType::SQLite);
#endif
#ifdef AR_MYSQL
	adapters.push_back(AdapterType::MySQL);
#endif
#ifdef AR_POSTGRES
	adapters.push_back(AdapterType::PostgreSQL);
#endif
#ifdef AR_MSSQL
	adapters.push_back(AdapterType::MSSQL);
#endif
	return adapters;
}

// Test servers and credentials: see docs/hardening.md ("WSL database servers", "Windows SQL Server")
inline std::shared_ptr<connection> create_connection(AdapterType type) {
	switch (type) {
#ifdef AR_SQLITE
	case AdapterType::SQLite:
		return std::make_shared<sqlite_adapter>(":memory:");
#endif
#ifdef AR_MYSQL
	case AdapterType::MySQL:
		// 127.0.0.1 forces TCP: "localhost" makes the client use a Unix socket whose default
		// path depends on how libmysql was built
		return std::make_shared<mysql_adapter>("127.0.0.1", "test", "test", "test_db", 3306);
#endif
#ifdef AR_POSTGRES
	case AdapterType::PostgreSQL:
		return std::make_shared<postgres_adapter>("localhost", "test", "test", "test_db", 5432);
#endif
#ifdef AR_MSSQL
	case AdapterType::MSSQL:
		return std::make_shared<mssql_adapter>("localhost", "test_db", "test", "test", 1433);
#endif
	default:
		throw std::runtime_error("Adapter not enabled in this build: " + adapter_name(type));
	}
}

// ------------------------------------------------------------------------------
// Per-adapter DDL fragments
// ------------------------------------------------------------------------------

// Auto-increment integer primary key named "id"
inline std::string pk_column(AdapterType type) {
	switch (type) {
	case AdapterType::SQLite:
		return "id INTEGER PRIMARY KEY AUTOINCREMENT";
	case AdapterType::MySQL:
		return "id INT AUTO_INCREMENT PRIMARY KEY";
	case AdapterType::PostgreSQL:
		return "id SERIAL PRIMARY KEY";
	case AdapterType::MSSQL:
		return "id INT IDENTITY(1,1) PRIMARY KEY";
	}
	return {};
}

// Short string column; SQL Server cannot compare TEXT with "="
inline std::string text_type(AdapterType type) {
	return type == AdapterType::SQLite ? "TEXT" : "VARCHAR(255)";
}

inline std::string bool_type(AdapterType type) {
	return type == AdapterType::MSSQL ? "BIT" : "BOOLEAN";
}

inline std::string bool_true_literal(AdapterType type) {
	return type == AdapterType::PostgreSQL ? "true" : "1";
}

// ar::datetime is stored as a Unix timestamp (project convention)
inline std::string timestamp_type(AdapterType type) {
	return type == AdapterType::SQLite ? "INTEGER" : "BIGINT";
}

// The adapter's native date/time column type, for testing that reads convert to ar::datetime
inline std::string native_datetime_type(AdapterType type) {
	switch (type) {
	case AdapterType::SQLite:
	case AdapterType::MySQL:
		return "DATETIME";
	case AdapterType::PostgreSQL:
		return "TIMESTAMP";
	case AdapterType::MSSQL:
		return "DATETIME2";
	}
	return {};
}

// Column list of the shared "users" table
inline std::string users_columns(AdapterType type) {
	return std::format("{}, name {} NOT NULL, email {}, age INTEGER, active {} DEFAULT {}", pk_column(type),
					   text_type(type), text_type(type), bool_type(type), bool_true_literal(type));
}

// ------------------------------------------------------------------------------
// Parameterized fixture base
// ------------------------------------------------------------------------------

// Connects to GetParam()'s adapter, registers it as the "default" connection, and
// (re)creates the tables returned by table_definitions(); drops them in TearDown.
// Derived fixtures must call AdapterTest::SetUp() and return if HasFatalFailure().
class AdapterTest : public ::testing::TestWithParam<AdapterType> {
  protected:
	using table_list = std::vector<std::pair<std::string, std::string>>; // name, column list

	virtual table_list table_definitions(AdapterType type) const = 0;

	void SetUp() override {
		try {
			adapter_ = create_connection(GetParam());
			config::instance().set_connections({ { "default", adapter_ } }, "default");
		} catch (const std::exception& e) {
			// An adapter compiled into this build must be reachable: fail rather than skip,
			// so a missing server can't pass as green
			FAIL() << "Cannot connect to " << adapter_name(GetParam()) << " database: " << e.what();
		}

		try {
			for (const auto& [name, columns] : table_definitions(GetParam())) {
				drop_table(name);
				adapter_->query(std::format("CREATE TABLE {} ({})", name, columns), {});
				tables_.push_back(name);
			}
		} catch (const std::exception& e) {
			FAIL() << "Cannot create test tables on " << adapter_name(GetParam()) << ": " << e.what();
		}
	}

	void TearDown() override {
		for (const auto& name : tables_ | std::views::reverse) {
			drop_table(name);
		}
		config::instance().set_connections({});
	}

	// Best effort: the table may not exist
	void drop_table(const std::string& name) const {
		if (!adapter_) {
			return;
		}
		try {
			adapter_->query("DROP TABLE IF EXISTS " + name, {});
		} catch (...) { // NOLINT(bugprone-empty-catch) -- best-effort drop, table may not exist
		}
	}

	AdapterType adapter_type() const { return GetParam(); }

	std::shared_ptr<connection> adapter_;
	std::vector<std::string> tables_;
};

} // namespace ar::test
