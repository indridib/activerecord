// ============================================================================
// transaction.h
//
// SPDX-FileCopyrightText: 2020-2026 Indridi Bjornsson
// SPDX-License-Identifier: MIT
// See file LICENSE for details
//
// RAII wrapper for database transactions
//

#pragma once

#include <activerecord/config.h>
#include <activerecord/connection.h>

namespace ar {

/**
 * RAII wrapper for database transactions.
 *
 * Automatically begins a transaction on construction and rolls back on destruction
 * unless explicitly committed. This ensures transactions are properly cleaned up
 * even when exceptions are thrown.
 *
 * Example usage:
 *   {
 *       transaction tx;
 *       model.save();
 *       tx.commit();  // Explicitly commit
 *   } // Auto-rollback if commit() wasn't called
 */
class transaction {
  public:
	/**
	 * Begin a transaction on the default connection.
	 * @throws ex::activerecord if no default connection is configured
	 */
	transaction() : connection_(config::instance().get_connection()) {
		if (!connection_) {
			throw ex::activerecord("No database connection configured");
		}
		connection_->begin_transaction();
	}

	/**
	 * Begin a transaction on a specific connection.
	 * @param conn The connection to use for the transaction
	 * @throws ex::activerecord if connection is null
	 */
	explicit transaction(config::connection_ptr conn) : connection_(std::move(conn)) {
		if (!connection_) {
			throw ex::activerecord("Connection cannot be null");
		}
		connection_->begin_transaction();
	}

	/**
	 * Destructor - automatically rolls back if not committed.
	 * Exceptions during rollback are suppressed to prevent double-throws.
	 */
	~transaction() noexcept {
		if (!committed_ && connection_) {
			try {
				connection_->rollback();
			} catch (...) { // NOLINT(bugprone-empty-catch) -- suppress exceptions in destructor
			}
		}
	}

	// Delete copy/move to prevent multiple rollbacks
	transaction(const transaction&) = delete;
	transaction& operator=(const transaction&) = delete;
	transaction(transaction&&) = delete;
	transaction& operator=(transaction&&) = delete;

	/**
	 * Commit the transaction.
	 * Prevents automatic rollback in destructor.
	 * Safe to call multiple times (subsequent calls are no-ops).
	 */
	void commit() {
		if (!committed_ && connection_) {
			connection_->commit();
			committed_ = true;
		}
	}

	/**
	 * Explicitly roll back the transaction.
	 * Safe to call multiple times (subsequent calls are no-ops).
	 */
	void rollback() {
		if (!committed_ && connection_) {
			connection_->rollback();
			committed_ = true; // Prevent double-rollback in destructor
		}
	}

  private:
	config::connection_ptr connection_;
	bool committed_{ false };
};

} // namespace ar
