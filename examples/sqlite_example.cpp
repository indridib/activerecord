// ============================================================================
// sqlite_example.cpp
//
// Example demonstrating SQLite adapter usage with ActiveRecord
// ============================================================================

#include <config.h>
#include <adapters/sqlite_adapter.h>
#include <model.h>
#include <sql_builder.h>

#include <iostream>

// Define a User model
class User : public ar::model {
public:
    static constexpr std::string_view st_table_name() { return "users"; }
    static constexpr std::string_view st_primary_key() { return "id"; }

    User() : model(st_table_name(), st_primary_key()) {}
};

int main()
{
    try {
        // 1. Setup database connection (using in-memory database for example)
        ar::config::instance().set_connections({
            { "default", std::make_shared<ar::sqlite_adapter>(":memory:") }
        }, "default");

        auto conn = ar::config::instance().get_connection();

        // 2. Create table
        conn->query("CREATE TABLE users ("
                   "id INTEGER PRIMARY KEY AUTOINCREMENT, "
                   "name TEXT NOT NULL, "
                   "email TEXT, "
                   "age INTEGER"
                   ")", {});

        std::cout << "Database and table created successfully!\n\n";

        // 3. Insert some users
        std::cout << "Creating users...\n";

        User alice;
        alice.set("name", ar::attribute("Alice Smith"));
        alice.set("email", ar::attribute("alice@example.com"));
        alice.set("age", ar::attribute(28));
        alice.save();
        std::cout << "Created: " << alice.to_json_string() << "\n";

        User bob;
        bob.set("name", ar::attribute("Bob Johnson"));
        bob.set("email", ar::attribute("bob@example.com"));
        bob.set("age", ar::attribute(35));
        bob.save();
        std::cout << "Created: " << bob.to_json_string() << "\n";

        User charlie;
        charlie.set("name", ar::attribute("Charlie Brown"));
        charlie.set("email", ar::attribute("charlie@example.com"));
        charlie.set("age", ar::attribute(42));
        charlie.save();
        std::cout << "Created: " << charlie.to_json_string() << "\n\n";

        // 4. Lazy iteration - rows fetched one at a time, not loaded into memory
        std::cout << "All users (lazy iteration):\n";
        auto all_query = ar::query<User>().order("age ASC");
        for (const auto& user : all_query) {
            std::cout << "  " << user["name"].as_string()
                     << " (age: " << user["age"].as_integer() << ")\n";
        }
        std::cout << "\n";

        // 5. Query with WHERE clause - also lazy
        std::cout << "Users older than 30:\n";
        auto age_query = ar::query<User>().where("age > ?", ar::attribute(30));
        for (const auto& user : age_query) {
            std::cout << "  " << user["name"].as_string()
                     << " (age: " << user["age"].as_integer() << ")\n";
        }
        std::cout << "\n";

        // 6. Early termination - only fetches rows until break
        std::cout << "First 2 users (early break, fetches only what's needed):\n";
        int count = 0;
        auto early_query = ar::query<User>().order("age ASC");
        for (const auto& user : early_query) {
            std::cout << "  " << user["name"].as_string() << "\n";
            if (++count >= 2) break;  // Stops fetching, doesn't load remaining rows
        }
        std::cout << "\n";

        // 7. Count users
        std::cout << "Total users: " << ar::query<User>().count() << "\n\n";

        // 8. Find with std::optional - safe handling of missing records
        std::cout << "Finding Alice...\n";
        if (auto found = ar::query<User>().where("name = ?", ar::attribute("Alice Smith")).first()) {
            std::cout << "Found: " << found->to_json_string() << "\n";

            // 9. Update user
            found->set("age", ar::attribute(29));
            found->save();
            std::cout << "Updated Alice's age to 29\n";
        } else {
            std::cout << "Alice not found\n";
        }
        std::cout << "\n";

        // 10. first_or_throw - when absence is exceptional
        try {
            User admin = ar::query<User>().where("name = ?", ar::attribute("NonExistent")).first_or_throw();
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
        auto user = ar::query<User>().where("name = ?", ar::attribute("Bob Johnson")).first_or_throw();
        std::cout << "  Is dirty: " << (user.is_dirty() ? "yes" : "no") << "\n";
        user.set("email", ar::attribute("bob.new@example.com"));
        std::cout << "  After set: " << (user.is_dirty() ? "yes" : "no") << "\n";
        user.save();
        std::cout << "  After save: " << (user.is_dirty() ? "yes" : "no") << "\n\n";

        // 13. XML serialization
        if (auto alice_loaded = ar::query<User>().where("name = ?", ar::attribute("Alice Smith")).first()) {
            std::cout << "Alice as XML:\n" << alice_loaded->to_xml_string("user") << "\n\n";
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
