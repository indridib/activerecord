// ============================================================================
// sqlite_example.cpp
//
// SPDX-FileCopyrightText: 2020-2026 Indridi Bjornsson
// SPDX-License-Identifier: MIT
// See file LICENSE for details
//
// Example demonstrating SQLite adapter usage with ActiveRecord
// Showcases: all data types, lazy iteration, std::optional, timestamps, version
//

#include <activerecord/adapters/sqlite_adapter.h>
#include <activerecord/config.h>
#include <activerecord/datetime.h>
#include <activerecord/model.h>
#include <activerecord/sql_builder.h>
#include <iostream>

// Define a User model
class User : public ar::model {
  public:
	static constexpr std::string_view st_table_name() { return "users"; }
	static constexpr std::string_view st_primary_key() { return "id"; }

	User() : model(st_table_name(), st_primary_key()) {}
};

int main() {
	try {
		// 1. Set up database connection (using in-memory database for example)
		ar::config::instance().set_connections({ { "default", std::make_shared<ar::sqlite_adapter>(":memory:") } },
											   "default");

		auto conn = ar::config::instance().get_connection();

		// 2. Create table with all supported types
		conn->query("CREATE TABLE users ("
					"id INTEGER PRIMARY KEY AUTOINCREMENT, "
					"name TEXT NOT NULL, "
					"email TEXT, "
					"age INTEGER, "
					"active BOOLEAN DEFAULT 1, "
					"salary REAL, "
					"bio TEXT, "
					"created_at DATETIME, "
					"updated_at DATETIME, "
					"version INTEGER"
					")",
					{});

		std::cout << "Database and table created successfully!\n\n";

		// 3. Insert users with all data types and record tracking
		std::cout << "Creating users...\n";

		User alice;
		alice.init_record_tracking(true); // Registers created_at, updated_at, version
		alice.set("name", "Alice Smith");
		alice.set("email", "alice@example.com");
		alice.set("age", 28);
		alice.set("active", true);
		alice.set("salary", 75000.50);
		alice.set("bio", "Software engineer from Reykjavik");
		alice.save();
		// created_at = now, updated_at = now, version = 1
		std::cout << "Created: " << alice.to_json_string() << "\n";

		User bob;
		bob.init_record_tracking(true);
		bob.set("name", "Bob Johnson");
		bob.set("email", "bob@example.com");
		bob.set("age", 35);
		bob.set("active", true);
		bob.set("salary", 92000.00);
		// bio left as NULL to demonstrate null handling
		bob.save();
		std::cout << "Created: " << bob.to_json_string() << "\n";

		User charlie;
		charlie.init_record_tracking(true);
		charlie.set("name", "Charlie Brown");
		charlie.set("email", "charlie@example.com");
		charlie.set("age", 42);
		charlie.set("active", false);
		charlie.set("salary", 110000.75);
		charlie.set("bio", "Retired database administrator");
		charlie.save();
		std::cout << "Created: " << charlie.to_json_string() << "\n\n";

		// 4. Lazy iteration - rows fetched one at a time, not loaded into memory
		std::cout << "All users (lazy iteration):\n";
		auto all_query = ar::query<User>().order("age ASC");
		for (const auto& user : all_query) {
			std::cout << "  " << user["name"].as_string() << " (age: " << user["age"].as_integer()
					  << ", salary: " << user["salary"].as_real() << ")\n";
		}
		std::cout << "\n";

		// 5. Query with WHERE clause - filtering on boolean
		std::cout << "Active users:\n";
		auto active_query = ar::query<User>().where("active = ?", true);
		for (const auto& user : active_query) {
			std::cout << "  " << user["name"].as_string() << "\n";
		}
		std::cout << "\n";

		// 6. Early termination - only fetches rows until break
		std::cout << "First 2 users (early break, fetches only what's needed):\n";
		int count = 0;
		auto early_query = ar::query<User>().order("age ASC");
		for (const auto& user : early_query) {
			std::cout << "  " << user["name"].as_string() << "\n";
			if (++count >= 2)
				break;
		}
		std::cout << "\n";

		// 7. Count users
		std::cout << "Total users: " << ar::query<User>().count() << "\n\n";

		// 8. Find with std::optional - safe handling of missing records
		std::cout << "Finding Alice...\n";
		if (auto found = ar::query<User>().where("name = ?", "Alice Smith").first()) {
			std::cout << "Found: " << found->to_json_string() << "\n";

			// 9. Update triggers auto-refresh of updated_at and version increment
			found->set("salary", 80000.00);
			found->save();
			std::cout << "Updated Alice's salary\n";
		}
		std::cout << "\n";

		// 10. Timestamps and version tracking demo
		std::cout << "Timestamp and version tracking:\n";
		auto loaded = ar::query<User>().where("name = ?", "Alice Smith").first_or_throw();
		std::cout << "  version:    " << loaded.get("version").as_integer() << "\n";
		std::cout << "  created_at: " << loaded.get("created_at").as_date().to_string() << "\n";
		std::cout << "  updated_at: " << loaded.get("updated_at").as_date().to_string() << "\n";

		// Another update increments version again
		loaded.set("age", 29);
		loaded.save();

		auto reloaded = ar::query<User>().where("name = ?", "Alice Smith").first_or_throw();
		std::cout << "  After 2nd update:\n";
		std::cout << "  version:    " << reloaded.get("version").as_integer() << "\n";
		std::cout << "  updated_at: " << reloaded.get("updated_at").as_date().to_string() << "\n\n";

		// 11. NULL handling
		std::cout << "NULL handling:\n";
		auto bob_loaded = ar::query<User>().where("name = ?", "Bob Johnson").first_or_throw();
		std::cout << "  Bob's bio is null: " << (bob_loaded.get("bio").is_null() ? "yes" : "no") << "\n\n";

		// 12. first_or_throw - when absence is exceptional
		try {
			ar::query<User>().where("name = ?", "NonExistent").first_or_throw();
		} catch (const ar::ex::item_not_found&) {
			std::cout << "first_or_throw correctly threw for missing record\n\n";
		}

		// 13. last() - returns the last record (reverses ORDER BY)
		if (auto oldest = ar::query<User>().order("age ASC").last()) {
			std::cout << "Oldest user: " << oldest->get("name").as_string()
					  << " (age: " << oldest->get("age").as_integer() << ")\n\n";
		}

		// 14. Dirty tracking
		std::cout << "Dirty tracking demo:\n";
		auto user = ar::query<User>().where("name = ?", "Bob Johnson").first_or_throw();
		std::cout << "  Is dirty: " << (user.is_dirty() ? "yes" : "no") << "\n";
		user.set("email", "bob.new@example.com");
		std::cout << "  After set: " << (user.is_dirty() ? "yes" : "no") << "\n";
		user.save();
		std::cout << "  After save: " << (user.is_dirty() ? "yes" : "no") << "\n\n";

		// 15. XML serialization (shows all types)
		if (auto alice_final = ar::query<User>().where("name = ?", "Alice Smith").first()) {
			std::cout << "Alice as XML:\n" << alice_final->to_xml_string("user") << "\n\n";
		}

		std::cout << "=== SQLite Example Complete! ===\n";

	} catch (const ar::ex::activerecord& e) {
		std::cerr << "ActiveRecord error: " << e.what() << "\n";
		return 1;
	} catch (const std::exception& e) {
		std::cerr << "Error: " << e.what() << "\n";
		return 1;
	}

	return 0;
}
