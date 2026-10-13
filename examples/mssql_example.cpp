// ============================================================================
// mssql_example.cpp
//
// SPDX-FileCopyrightText: 2020-2026 Indridi Bjornsson
// SPDX-License-Identifier: MIT
// See file LICENSE for details
//
// Example demonstrating MS-SQL Server adapter usage with ActiveRecord
//

#include <activerecord/adapters/mssql_adapter.h>
#include <activerecord/config.h>
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
		// 1. Set up database connection
		// Note: Update these credentials for your MS-SQL Server
		std::string server = "localhost";
		std::string database = "activerecord_test";
		std::string username = "sa";
		std::string password = ""; // Set your SQL Server password
		unsigned int port = 1433;

		// Option 1: Connect using server/database/user/password
		ar::config::instance().set_connections(
			{ { "default", std::make_shared<ar::mssql_adapter>(server, database, username, password, port) } },
			"default");

		// Option 2: Connect using connection string
		// ar::config::instance().set_connections({
		//     { "default", std::make_shared<ar::mssql_adapter>(
		//         "DRIVER={ODBC Driver 17 for SQL
		//         Server};SERVER=localhost;DATABASE=activerecord_test;UID=sa;PWD=yourpassword;"
		//     )}
		// }, "default");

		// Option 3: Connect using DSN
		// ar::config::instance().set_connections({
		//     { "default", std::make_shared<ar::mssql_adapter>("MyDSN", "sa", "yourpassword") }
		// }, "default");

		auto conn = ar::config::instance().get_connection();

		std::cout << "Connected to MS-SQL Server successfully!\n\n";

		// 2. Create table (if it doesn't exist)
		std::cout << "Creating table if not exists...\n";
		conn->query("IF NOT EXISTS (SELECT * FROM sys.tables WHERE name = 'users') "
					"CREATE TABLE users ("
					"id INT IDENTITY(1,1) PRIMARY KEY, "
					"name VARCHAR(255) NOT NULL, "
					"email VARCHAR(255), "
					"age INT"
					")",
					{});

		std::cout << "Table ready!\n\n";

		// 3. Insert some users
		std::cout << "Creating users...\n";

		User alice;
		alice.set("name", "Alice Smith");
		alice.set("email", "alice@example.com");
		alice.set("age", 28);
		alice.save();
		std::cout << "Created: " << alice.to_json_string() << "\n";

		User bob;
		bob.set("name", "Bob Johnson");
		bob.set("email", "bob@example.com");
		bob.set("age", 35);
		bob.save();
		std::cout << "Created: " << bob.to_json_string() << "\n";

		User charlie;
		charlie.set("name", "Charlie Brown");
		charlie.set("email", "charlie@example.com");
		charlie.set("age", 42);
		charlie.save();
		std::cout << "Created: " << charlie.to_json_string() << "\n\n";

		// 4. Lazy iteration - rows fetched one at a time, not loaded into memory
		std::cout << "All users (lazy iteration):\n";
		auto all_query = ar::query<User>().order("age ASC");
		for (const auto& user : all_query) {
			std::cout << "  " << user["name"].as_string() << " (age: " << user["age"].as_integer() << ")\n";
		}
		std::cout << "\n";

		// 5. Query with WHERE clause - also lazy
		std::cout << "Users older than 30:\n";
		auto age_query = ar::query<User>().where("age > ?", 30);
		for (const auto& user : age_query) {
			std::cout << "  " << user["name"].as_string() << " (age: " << user["age"].as_integer() << ")\n";
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

			// 9. Update user
			found->set("age", 29);
			found->save();
			std::cout << "Updated Alice's age to 29\n";
		} else {
			std::cout << "Alice not found\n";
		}
		std::cout << "\n";

		// 10. first_or_throw - when absence is exceptional
		try {
			User admin = ar::query<User>().where("name = ?", "NonExistent").first_or_throw();
		} catch (const ar::ex::item_not_found&) {
			std::cout << "first_or_throw correctly threw for missing record\n\n";
		}

		// 11. last() - returns the last record (reverses ORDER BY)
		if (auto oldest = ar::query<User>().order("age ASC").last()) {
			std::cout << "Oldest user: " << oldest->get("name").as_string()
					  << " (age: " << oldest->get("age").as_integer() << ")\n\n";
		}

		// 12. Dirty tracking
		std::cout << "Dirty tracking demo:\n";
		auto user = ar::query<User>().where("name = ?", "Bob Johnson").first_or_throw();
		std::cout << "  Is dirty: " << (user.is_dirty() ? "yes" : "no") << "\n";
		user.set("email", "bob.new@example.com");
		std::cout << "  After set: " << (user.is_dirty() ? "yes" : "no") << "\n";
		user.save();
		std::cout << "  After save: " << (user.is_dirty() ? "yes" : "no") << "\n\n";

		// 13. XML serialization
		if (auto alice_loaded = ar::query<User>().where("name = ?", "Alice Smith").first()) {
			std::cout << "Alice as XML:\n" << alice_loaded->to_xml_string("user") << "\n\n";
		}

		std::cout << "=== MS-SQL Example Complete! ===\n";

	} catch (const ar::ex::activerecord& e) {
		std::cerr << "ActiveRecord error: " << e.what() << "\n";
		return 1;
	} catch (const std::exception& e) {
		std::cerr << "Error: " << e.what() << "\n";
		return 1;
	}

	return 0;
}
