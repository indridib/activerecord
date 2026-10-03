// ==============================================================================
// connection_factory_test.cpp
//
// Unit tests for make_connection(URL)
// ==============================================================================

#include <activerecord/config.h>
#include <activerecord/connection_factory.h>
#include <gtest/gtest.h>

using namespace ar;

TEST(ConnectionFactoryTest, UnsupportedProtocolThrows) {
	EXPECT_THROW(make_connection("unknown://localhost/db"), ex::config);
}

#ifndef AR_MYSQL
TEST(ConnectionFactoryTest, DisabledMySqlProtocolThrows) {
	EXPECT_THROW(make_connection("mysql://user:pass@localhost/app_db"), ex::config);
}
#endif

#ifdef AR_SQLITE
TEST(ConnectionFactoryTest, SqliteConnectionFromUrl) {
	auto conn = make_connection("sqlite://:memory:");
	ASSERT_TRUE(conn != nullptr);
}

TEST(ConnectionFactoryTest, ConfigSetConnectionUrl) {
	config::instance().set_connection_url("default", "sqlite://:memory:");
	auto conn = config::instance().get_connection("default");
	ASSERT_TRUE(conn != nullptr);
}

TEST(ConnectionFactoryTest, ConfigSetConnectionUrlsMap) {
	std::map<std::string, std::string> urls;
	urls["primary"] = "sqlite://:memory:";
	urls["secondary"] = "sqlite://:memory:";

	config::instance().set_connection_urls(urls, "primary");
	auto primary = config::instance().get_connection("primary");
	auto secondary = config::instance().get_connection("secondary");
	ASSERT_TRUE(primary != nullptr);
	ASSERT_TRUE(secondary != nullptr);
}
#endif
