// ============================================================================
// postgres_example.cpp
//
// Example demonstrating PostgreSQL adapter usage with ActiveRecord
// ============================================================================

#include <config.h>
#include <adapters/postgres_adapter.h>
#include <model.h>
#include <sql_builder.h>

#include <iostream>

// Define a User model
class User : public ar::model {
public:
    static const char* st_table_name() { return "users"; }
    static const char* st_primary_key() { return "id"; }

    User() : model(st_table_name(), st_primary_key()) {}
};

int main()
{
    try {
        // 1. Setup database connection
        // Note: Update these credentials for your PostgreSQL server
        std::string host = "localhost";
        std::string user = "postgres";
        std::string password = "";  // Set your PostgreSQL password
        std::string database = "activerecord_test";
        unsigned int port = 5432;

        ar::config::instance().set_connections({
            { "default", std::make_shared<ar::postgres_adapter>(host, user, password, database, port) }
        }, "default");

        auto conn = ar::config::instance().get_connection();

        std::cout << "Connected to PostgreSQL database successfully!\n\n";

        // 2. Create table (if it doesn't exist)
        std::cout << "Creating table if not exists...\n";
        conn->query("CREATE TABLE IF NOT EXISTS users ("
                   "id SERIAL PRIMARY KEY, "
                   "name VARCHAR(255) NOT NULL, "
                   "email VARCHAR(255), "
                   "age INTEGER"
                   ")", {});

        std::cout << "Table ready!\n\n";

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

        // 4. Query all users
        std::cout << "All users:\n";
        ar::query<User> all_query;
        for (const auto& user : all_query.order("age ASC").all()) {
            std::cout << "  " << user["name"].as_string()
                     << " (age: " << user["age"].as_integer() << ")\n";
        }
        std::cout << "\n";

        // 5. Query with WHERE clause
        std::cout << "Users older than 30:\n";
        ar::query<User> age_query;
        age_query.where("age > ?", ar::attribute(30));
        for (const auto& user : age_query.all()) {
            std::cout << "  " << user["name"].as_string() << "\n";
        }
        std::cout << "\n";

        // 6. Update a user
        std::cout << "Updating Alice's age...\n";
        alice.set("age", ar::attribute(29));
        alice.save();
        std::cout << "Updated: " << alice.to_json_string() << "\n\n";

        // 7. Count users
        ar::query<User> count_query;
        std::cout << "Total users: " << count_query.count() << "\n\n";

        // 8. Find specific user
        std::cout << "Finding user with id=2...\n";
        ar::query<User> find_query;
        User found = find_query.find(2);
        std::cout << "Found: " << found.to_json_string() << "\n\n";

        // 9. Demonstrate dirty tracking
        std::cout << "Dirty tracking demo:\n";
        found.set("email", ar::attribute("newemail@example.com"));
        std::cout << "  Is dirty: " << (found.is_dirty() ? "yes" : "no") << "\n";
        found.save();
        std::cout << "  After save, is dirty: " << (found.is_dirty() ? "yes" : "no") << "\n\n";

        // 10. XML serialization
        std::cout << "User as XML:\n";
        std::cout << found.to_xml_string("user") << "\n\n";

        std::cout << "=== PostgreSQL Example Complete! ===\n";

    } catch (const ar::ex::activerecord& e) {
        std::cerr << "ActiveRecord error: " << e.what() << "\n";
        return 1;
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }

    return 0;
}
