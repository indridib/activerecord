// ==============================================================================
// connection_url_test.cpp
//
// Unit tests for connection::parse_connection_url
// ==============================================================================

#include <activerecord/connection.h>
#include <gtest/gtest.h>

using namespace ar;

TEST(ConnectionUrlTest, ParsesBasicHostPortDatabase) {
	auto info = connection::parse_connection_url("mysql://user:pass@localhost:3306/app_db");
	EXPECT_EQ("mysql", info.protocol);
	ASSERT_TRUE(info.host.has_value());
	EXPECT_EQ("localhost", *info.host);
	ASSERT_TRUE(info.port.has_value());
	EXPECT_EQ(3306, *info.port);
	ASSERT_TRUE(info.database.has_value());
	EXPECT_EQ("app_db", *info.database);
	ASSERT_TRUE(info.user.has_value());
	EXPECT_EQ("user", *info.user);
	ASSERT_TRUE(info.password.has_value());
	EXPECT_EQ("pass", *info.password);
}

TEST(ConnectionUrlTest, ParsesDecodeFlagForCredentials) {
	auto info = connection::parse_connection_url("postgres://urlencoded%20user:pa%24%24@db.local/my_db?decode=true");
	ASSERT_TRUE(info.user.has_value());
	ASSERT_TRUE(info.password.has_value());
	EXPECT_EQ("urlencoded user", *info.user);
	EXPECT_EQ("pa$$", *info.password);
}

TEST(ConnectionUrlTest, ParsesUnixSocketForm) {
	auto info = connection::parse_connection_url("mysql://root:secret@unix(/var/run/mysqld.sock)/my_db");
	EXPECT_EQ("mysql", info.protocol);
	ASSERT_TRUE(info.socket.has_value());
	EXPECT_EQ("/var/run/mysqld.sock", *info.socket);
	ASSERT_TRUE(info.host.has_value());
	EXPECT_EQ("/var/run/mysqld.sock", *info.host);
	ASSERT_TRUE(info.database.has_value());
	EXPECT_EQ("my_db", *info.database);
}

TEST(ConnectionUrlTest, ParsesSqliteRelativePath) {
	auto info = connection::parse_connection_url("sqlite://../relative/path/to/file.db");
	EXPECT_EQ("sqlite", info.protocol);
	ASSERT_TRUE(info.file.has_value());
	EXPECT_EQ("../relative/path/to/file.db", *info.file);
}

TEST(ConnectionUrlTest, ParsesSqliteWindowsWrapper) {
	auto info = connection::parse_connection_url("sqlite://windows(c%2A/absolute/path/to/file.db)");
	EXPECT_EQ("sqlite", info.protocol);
	ASSERT_TRUE(info.file.has_value());
	EXPECT_EQ("c:/absolute/path/to/file.db", *info.file);
}

TEST(ConnectionUrlTest, ParsesQueryAndCharset) {
	auto info = connection::parse_connection_url("postgres://u:p@dbhost/mydb?charset=utf8&sslmode=require");
	ASSERT_TRUE(info.charset.has_value());
	EXPECT_EQ("utf8", *info.charset);
	auto iter = info.query.find("sslmode");
	ASSERT_TRUE(iter != info.query.end());
	EXPECT_EQ("require", iter->second);
}

TEST(ConnectionUrlTest, ThrowsOnMissingProtocol) {
	EXPECT_THROW(connection::parse_connection_url("localhost/mydb"), ex::config);
}

TEST(ConnectionUrlTest, ThrowsOnMissingHostForNonSqlite) {
	EXPECT_THROW(connection::parse_connection_url("mysql:///mydb"), ex::config);
}

TEST(ConnectionUrlTest, ThrowsOnInvalidPort) {
	EXPECT_THROW(connection::parse_connection_url("mysql://localhost:notaport/mydb"), ex::config);
}
