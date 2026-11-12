// ============================================================================
// typed_model_showcase.cpp
//
// SPDX-FileCopyrightText: 2020-2026 Indridi Bjornsson
// SPDX-License-Identifier: MIT
// See file LICENSE for details
//
// Demonstrates typed_model<T> with mixed field types and relationships on SQLite in-memory DB.
// Change the connection setup in main() to use another adapter.
//

#include <activerecord/adapters/sqlite_adapter.h>
#include <activerecord/config.h>
#include <activerecord/datetime.h>
#include <activerecord/relations.h>
#include <activerecord/schema.h>
#include <activerecord/sql_builder.h>
#include <activerecord/typed_model.h>
#include <cstdint>
#include <iostream>
#include <optional>
#include <string>
#include <vector>

struct Account;

struct Owner : ar::typed_model<Owner> {
	static constexpr auto table_name{ "owners" };
	static constexpr auto primary_key{ "id" };

	std::int64_t id{};
	std::string name{};

	static constexpr auto columns() { return ar::columns(ar::col<&Owner::id>("id"), ar::col<&Owner::name>("name")); }

	ar::query<Account> accounts() const; // defined after Account: the body needs Account complete
};

struct Account : ar::typed_model<Account> {
	static constexpr auto table_name{ "accounts" };
	static constexpr auto primary_key{ "id" };

	std::int64_t id{};
	std::string username{};
	int age{};
	bool active{};
	double credit{};
	ar::datetime joined_at{};
	std::optional<std::string> nickname{};
	ar::datetime created_at{};
	ar::datetime updated_at{};
	int64_t version{};
	std::optional<std::int64_t> owner_id{};

	static constexpr auto columns() {
		return ar::columns(ar::col<&Account::id>("id"), ar::col<&Account::username>("username"),
						   ar::col<&Account::age>("age"), ar::col<&Account::active>("active"),
						   ar::col<&Account::credit>("credit"), ar::col<&Account::joined_at>("joined_at"),
						   ar::col<&Account::nickname>("nickname"), ar::col<&Account::created_at>("created_at"),
						   ar::col<&Account::updated_at>("updated_at"), ar::col<&Account::version>("version"),
						   ar::col<&Account::owner_id>("owner_id").references<Owner>());
	}

	std::optional<Owner> owner() const { return ar::belongs_to<&Account::owner_id>(*this); }
};

inline ar::query<Account> Owner::accounts() const {
	return ar::has_many<&Account::owner_id>(*this);
}

// Disable metadata tracking (created_at/updated_at/version) with typed_model<T, false>.
struct ImportAccount : ar::typed_model<ImportAccount, false> {
	static constexpr auto table_name{ "accounts" };
	static constexpr auto primary_key{ "id" };

	std::int64_t id{};
	std::string username{};

	static constexpr auto columns() {
		return ar::columns(ar::col<&ImportAccount::id>("id"), ar::col<&ImportAccount::username>("username"));
	}
};

int main() {
	try {
		// Easy to swap: replace sqlite_adapter with another adapter and connection setup.
		auto sqlite{ std::make_shared<ar::sqlite_adapter>(":memory:") };
		sqlite->foreign_keys(true); // references<>() creates foreign keys
		ar::config::instance().set_connections({ { "default", sqlite } }, "default");
		// The table comes from Account::columns(); optional members become nullable columns
		ar::create_tables<Owner, Account>();

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

		// Relationships: named by the foreign-key member
		Owner owner;
		owner.name = "Ragnar";
		owner.save();
		updated.owner_id = owner.id;
		updated.save();
		Account second{ ar::build<&Account::owner_id>(owner) };
		second.username = "bjorn";
		second.save();

		if (const std::optional<Owner> found{ updated.owner() }) {
			std::cout << "Owner of " << updated.username << ": " << found->name << "\n";
		}
		std::cout << owner.name << " has " << owner.accounts().count() << " accounts\n";

		const std::vector<Account> accounts{ ar::query<Account>().order("id").all() };
		const auto owners{ ar::preload<&Account::owner_id>(accounts) }; // one query for all owners
		for (const Account& account : accounts) {
			const auto found{ account.owner_id ? owners.find(*account.owner_id) : owners.end() };
			std::cout << account.username << " -> " << (found != owners.end() ? found->second.name : "<none>") << "\n";
		}

		const std::vector<Owner> all_owners{ ar::query<Owner>().all() };
		const auto accounts_by_owner{ ar::preload_children<&Account::owner_id>(all_owners) }; // one query
		for (const Owner& each : all_owners) {
			std::cout << each.name << ":";
			if (const auto found{ accounts_by_owner.find(each.id) }; found != accounts_by_owner.end()) {
				for (const Account& account : found->second) {
					std::cout << " " << account.username;
				}
			}
			std::cout << "\n";
		}

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
