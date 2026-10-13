// ============================================================================
// typed_model_showcase.cpp
//
// SPDX-FileCopyrightText: 2020-2026 Indridi Bjornsson
// SPDX-License-Identifier: MIT
// See file LICENSE for details
//
// Demonstrates typed_model<T> with mixed field types on SQLite in-memory DB.
// Change the connection setup in main() to use another adapter.
//

#include <activerecord/adapters/sqlite_adapter.h>
#include <activerecord/config.h>
#include <activerecord/datetime.h>
#include <activerecord/sql_builder.h>
#include <activerecord/typed_model.h>
#include <iostream>
#include <optional>

struct Account : ar::typed_model<Account> {
	static constexpr auto table_name{ "accounts" };
	static constexpr auto primary_key{ "id" };

	long id{};
	std::string username{};
	int age{};
	bool active{};
	double credit{};
	ar::datetime joined_at{};
	std::optional<std::string> nickname{};
	ar::datetime created_at{};
	ar::datetime updated_at{};
	int64_t version{};

	static constexpr auto columns() {
		return ar::columns(ar::col<&Account::id>("id"), ar::col<&Account::username>("username"),
						   ar::col<&Account::age>("age"), ar::col<&Account::active>("active"),
						   ar::col<&Account::credit>("credit"), ar::col<&Account::joined_at>("joined_at"),
						   ar::col<&Account::nickname>("nickname"), ar::col<&Account::created_at>("created_at"),
						   ar::col<&Account::updated_at>("updated_at"), ar::col<&Account::version>("version"));
	}
};

// Disable metadata tracking (created_at/updated_at/version) with typed_model<T, false>.
struct ImportAccount : ar::typed_model<ImportAccount, false> {
	static constexpr auto table_name{ "accounts" };
	static constexpr auto primary_key{ "id" };

	long id{};
	std::string username{};

	static constexpr auto columns() {
		return ar::columns(ar::col<&ImportAccount::id>("id"), ar::col<&ImportAccount::username>("username"));
	}
};

int main() {
	try {
		// Easy to swap: replace sqlite_adapter with another adapter and connection setup.
		ar::config::instance().set_connections({ { "default", std::make_shared<ar::sqlite_adapter>(":memory:") } },
											   "default");
		auto conn = ar::config::instance().get_connection();

		conn->query("CREATE TABLE accounts ("
					"id INTEGER PRIMARY KEY AUTOINCREMENT, "
					"username TEXT NOT NULL, "
					"age INTEGER NOT NULL, "
					"active INTEGER NOT NULL, "
					"credit REAL NOT NULL, "
					"joined_at INTEGER NOT NULL, "
					"nickname TEXT NULL, "
					"created_at INTEGER, "
					"updated_at INTEGER, "
					"version INTEGER"
					")",
					{});

		Account acc;
		acc.username = "sigrun";
		acc.age = 34;
		acc.active = true;
		acc.credit = 1250.75;
		acc.joined_at = ar::datetime{ 2024, 6, 15, 9, 30, 0 };
		acc.nickname = std::string{ "sig" };
		acc.save();

		std::cout << "Inserted account id: " << acc.id << "\n";
		std::cout << "Version after insert: " << acc.version << "\n";

		auto loaded = ar::query<Account>().find(acc.id);
		std::cout << "Loaded: " << loaded.username << ", age " << loaded.age << ", credit " << loaded.credit << "\n";
		std::cout << "Joined at: " << loaded.joined_at.to_string() << "\n";
		std::cout << "Nickname: " << (loaded.nickname.has_value() ? *loaded.nickname : std::string{ "<null>" }) << "\n";

		loaded.credit = 1500.25;
		loaded.nickname = std::nullopt; // Optional -> SQL NULL
		loaded.save();

		auto updated = ar::query<Account>().find(acc.id);
		std::cout << "Updated credit: " << updated.credit << "\n";
		std::cout << "Updated nickname: "
				  << (updated.nickname.has_value() ? *updated.nickname : std::string{ "<null>" }) << "\n";
		std::cout << "Version after update: " << updated.version << "\n";
		std::cout << "updated_at: " << updated.updated_at.to_string() << "\n";

		std::cout << "typed_model showcase completed.\n";
		return 0;
	} catch (const ar::ex::activerecord& e) {
		std::cerr << "ActiveRecord error: " << e.what() << "\n";
		return 1;
	} catch (const std::exception& e) {
		std::cerr << "Error: " << e.what() << "\n";
		return 1;
	}
}
