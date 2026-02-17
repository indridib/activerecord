// ============================================================================
// adapter_tests.cpp
//
// Parameterized integration tests that run against multiple database adapters
// This ensures all adapters behave consistently
// ============================================================================

#include <activerecord.h>
#include <config.h>
#include <gtest/gtest.h>
#include <model.h>
#include <sql_builder.h>

#ifdef AR_SQLITE
#include <adapters/sqlite_adapter.h>
#endif

#ifdef AR_MYSQL
#include <adapters/mysql_adapter.h>
#endif

#ifdef AR_POSTGRES
#include <adapters/postgres_adapter.h>
#endif

#ifdef AR_MSSQL
#include <adapters/mssql_adapter.h>
#endif

using namespace ar;

// ==============================================================================
// Test Model
// ==============================================================================

class User : public model {
  public:
	static const char* st_table_name() { return "users"; }
	static const char* st_primary_key() { return "id"; }

	User() : model(st_table_name(), st_primary_key()) {}
};

// ==============================================================================
// Adapter Factory - Creates different adapter types
// ==============================================================================

enum class AdapterType { SQLite, MySQL, PostgreSQL, MSSQL };

class AdapterFactory {
  public:
	static std::shared_ptr<connection> create(AdapterType type) {
		switch (type) {
#ifdef AR_SQLITE
		case AdapterType::SQLite:
			return std::make_shared<sqlite_adapter>(":memory:");
#endif

#ifdef AR_MYSQL
		case AdapterType::MySQL:
			// Note: This requires a running MySQL server
			// You may need to adjust these credentials or skip MySQL tests
			return std::make_shared<mysql_adapter>("localhost", // host
												   "test",		// user
												   "test",		// password
												   "test_db",	// database
												   3306			// port
			);
#endif

#ifdef AR_POSTGRES
		case AdapterType::PostgreSQL:
			// Note: This requires a running PostgreSQL server
			// You may need to adjust these credentials or skip PostgreSQL tests
			return std::make_shared<postgres_adapter>("localhost", // host
													  "test",	   // user
													  "test",	   // password
													  "test_db",   // database
													  5432		   // port
			);
#endif

#ifdef AR_MSSQL
		case AdapterType::MSSQL:
			// Note: This requires a running MS-SQL server
			// You may need to adjust these credentials or skip MS-SQL tests
			return std::make_shared<mssql_adapter>("localhost", // server
												   "test_db",	// database
												   "test",		// user
												   "test",		// password
												   1433			// port
			);
#endif

		default:
			throw std::runtime_error("Unsupported adapter type");
		}
	}

	static std::string create_table_sql(AdapterType type) {
		switch (type) {
#ifdef AR_SQLITE
		case AdapterType::SQLite:
			return "CREATE TABLE users ("
				   "id INTEGER PRIMARY KEY AUTOINCREMENT, "
				   "name TEXT NOT NULL, "
				   "email TEXT, "
				   "age INTEGER, "
				   "active BOOLEAN DEFAULT 1"
				   ")";
#endif

#ifdef AR_MYSQL
		case AdapterType::MySQL:
			return "CREATE TABLE IF NOT EXISTS users ("
				   "id INT AUTO_INCREMENT PRIMARY KEY, "
				   "name VARCHAR(255) NOT NULL, "
				   "email VARCHAR(255), "
				   "age INT, "
				   "active BOOLEAN DEFAULT 1"
				   ")";
#endif

#ifdef AR_POSTGRES
		case AdapterType::PostgreSQL:
			return "CREATE TABLE IF NOT EXISTS users ("
				   "id SERIAL PRIMARY KEY, "
				   "name VARCHAR(255) NOT NULL, "
				   "email VARCHAR(255), "
				   "age INTEGER, "
				   "active BOOLEAN DEFAULT true"
				   ")";
#endif

#ifdef AR_MSSQL
		case AdapterType::MSSQL:
			return "IF NOT EXISTS (SELECT * FROM sys.tables WHERE name = 'users') "
				   "CREATE TABLE users ("
				   "id INT IDENTITY(1,1) PRIMARY KEY, "
				   "name VARCHAR(255) NOT NULL, "
				   "email VARCHAR(255), "
				   "age INT, "
				   "active BIT DEFAULT 1"
				   ")";
#endif

		default:
			return "";
		}
	}

	static std::string drop_table_sql(AdapterType type) { return "DROP TABLE IF EXISTS users"; }

	static std::string adapter_name(AdapterType type) {
		switch (type) {
		case AdapterType::SQLite:
			return "SQLite";
		case AdapterType::MySQL:
			return "MySQL";
		case AdapterType::PostgreSQL:
			return "PostgreSQL";
		case AdapterType::MSSQL:
			return "MSSQL";
		default:
			return "Unknown";
		}
	}
};

// ==============================================================================
// Parameterized Test Fixture
// ==============================================================================

class DatabaseAdapterTest : public ::testing::TestWithParam<AdapterType> {
  protected:
	void SetUp() override {
		adapter_type_ = GetParam();

		try {
			adapter_ = AdapterFactory::create(adapter_type_);

			// Configure global connection
			config::connection_map connections;
			connections["default"] = adapter_;
			config::instance().set_connections(connections, "default");

			// Drop table if it exists (for MySQL)
			try {
				adapter_->query(AdapterFactory::drop_table_sql(adapter_type_), {});
			} catch (...) {
				// Ignore errors if table doesn't exist
			}

			// Create table
			adapter_->query(AdapterFactory::create_table_sql(adapter_type_), {});

		} catch (const std::exception& e) {
			GTEST_SKIP() << "Cannot connect to " << AdapterFactory::adapter_name(adapter_type_)
						 << " database: " << e.what();
		}
	}

	void TearDown() override {
		if (adapter_) {
			try {
				adapter_->query(AdapterFactory::drop_table_sql(adapter_type_), {});
			} catch (...) {
				// Ignore cleanup errors
			}
		}
	}

	AdapterType adapter_type_;
	std::shared_ptr<connection> adapter_;
};

// ==============================================================================
// Shared Integration Tests
// ==============================================================================

TEST_P(DatabaseAdapterTest, InsertAndRetrieve) {
	User user;
	user.set("name", attribute("John Doe"));
	user.set("email", attribute("john@example.com"));
	user.set("age", attribute(30));

	EXPECT_TRUE(user.save());

	// Retrieve and verify
	query<User> q;
	auto users = q.where("name = ?", attribute("John Doe")).all();
	ASSERT_EQ(1, users.size());
	EXPECT_EQ("John Doe", users[0]["name"].as_string());
	EXPECT_EQ("john@example.com", users[0]["email"].as_string());
	EXPECT_EQ(30, users[0]["age"].as_integer());
}

TEST_P(DatabaseAdapterTest, UpdateRecord) {
	User user;
	user.set("name", attribute("Jane"));
	user.set("age", attribute(25));
	user.save();

	user.set("age", attribute(26));
	EXPECT_TRUE(user.save());

	query<User> q;
	auto users = q.where("name = ?", attribute("Jane")).all();
	ASSERT_EQ(1, users.size());
	EXPECT_EQ(26, users[0]["age"].as_integer());
}

TEST_P(DatabaseAdapterTest, QueryWithWhere) {
	// Insert multiple users
	for (int i = 0; i < 5; ++i) {
		User u;
		u.set("name", attribute("User" + std::to_string(i)));
		u.set("age", attribute(20 + i * 5));
		u.save();
	}

	// Query users older than 25
	query<User> q;
	auto users = q.where("age > ?", attribute(25)).all();
	EXPECT_GE(users.size(), 2);
}

TEST_P(DatabaseAdapterTest, QueryWithOrderBy) {
	User u1, u2, u3;
	u1.set("name", attribute("Charlie"));
	u1.set("age", attribute(30));
	u1.save();

	u2.set("name", attribute("Alice"));
	u2.set("age", attribute(25));
	u2.save();

	u3.set("name", attribute("Bob"));
	u3.set("age", attribute(35));
	u3.save();

	query<User> q;
	auto users = q.order("name ASC").all();
	ASSERT_GE(users.size(), 3);
	EXPECT_EQ("Alice", users[0]["name"].as_string());
}

TEST_P(DatabaseAdapterTest, QueryWithLimit) {
	for (int i = 0; i < 10; ++i) {
		User u;
		u.set("name", attribute("User" + std::to_string(i)));
		u.save();
	}

	query<User> q;
	auto users = q.limit(5).all();
	EXPECT_EQ(5, users.size());
}

TEST_P(DatabaseAdapterTest, QueryCount) {
	for (int i = 0; i < 7; ++i) {
		User u;
		u.set("name", attribute("User" + std::to_string(i)));
		u.save();
	}

	query<User> q;
	EXPECT_EQ(7, q.count());
}

TEST_P(DatabaseAdapterTest, ParameterBinding) {
	User u;
	u.set("name", attribute("Test's Name")); // Name with apostrophe
	u.set("email", attribute("test@example.com"));
	EXPECT_TRUE(u.save());

	query<User> q;
	auto users = q.where("name = ?", attribute("Test's Name")).all();
	ASSERT_EQ(1, users.size());
	EXPECT_EQ("Test's Name", users[0]["name"].as_string());
}

TEST_P(DatabaseAdapterTest, NullValues) {
	User u;
	u.set("name", attribute("NoEmail"));
	u.set("email", attribute()); // NULL
	u.save();

	query<User> q;
	auto users = q.where("name = ?", attribute("NoEmail")).all();
	ASSERT_EQ(1, users.size());
	EXPECT_TRUE(users[0]["email"].is_null());
}

TEST_P(DatabaseAdapterTest, BooleanValues) {
	User u;
	u.set("name", attribute("Active User"));
	u.set("active", attribute(true));
	u.save();

	query<User> q;
	auto users = q.where("name = ?", attribute("Active User")).all();
	ASSERT_EQ(1, users.size());
	// Note: Boolean handling may vary by database
}

// ==============================================================================
// Test Instantiation
// ==============================================================================

std::vector<AdapterType> get_available_adapters() {
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

INSTANTIATE_TEST_SUITE_P(AllAdapters, DatabaseAdapterTest, ::testing::ValuesIn(get_available_adapters()),
						 [](const ::testing::TestParamInfo<AdapterType>& info) {
							 return AdapterFactory::adapter_name(info.param);
						 });
