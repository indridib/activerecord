// ============================================================================
// connection.h
//
// SPDX-FileCopyrightText: 2020-2026 Indridi Bjornsson
// SPDX-License-Identifier: MIT
// See file LICENSE.MIT for detail
//

#pragma once

#include <activerecord/activerecord.h>
#include <activerecord/attribute.h>
#include <algorithm>
#include <cctype>
#include <charconv>
#include <map>
#include <memory>
#include <optional>
#include <string_view>

namespace ar {

/**
 * a general db statement class
 */
class statement {
  public:
	statement() = default;
	virtual ~statement() = default;
	virtual void execute(const std::string& cmd, const attribute_vector& values) = 0;
	virtual bool fetch() { return false; }
	virtual std::size_t affected_rows() const noexcept { return 0; }

	virtual void get_attributes(attribute_map&) {}
};

/**
 * a base class for database connection adapters.
 */
class connection {

  public:
	struct connection_url_info {
		std::string protocol;
		std::optional<std::string> host;
		std::optional<int> port;
		std::optional<std::string> database;
		std::optional<std::string> user;
		std::optional<std::string> password;
		std::optional<std::string> socket;
		std::optional<std::string> file;
		std::optional<std::string> charset;
		std::map<std::string, std::string> query;
	};

	/**
	 * The quote character used for quoting column and field names.
	 */
	static const char QUOTE_CHARACTER = '`';

	connection() = default;

	virtual ~connection() = default;

	/**
	 * Tells you if this adapter supports sequences or not.
	 *
	 * @return bool
	 */
	virtual bool supports_sequences() const { return false; }

	/**
	 * Use this for any adapters that can take connection info in the form below
	 * to set the adapters connection info.
	 *
	 * <code>
	 * protocol://username:password@host[:port]/dbname
	 * protocol://urlencoded%20username:urlencoded%20password@host[:port]/dbname?decode=true
	 * protocol://username:password@unix(/some/file/path)/dbname
	 * </code>
	 *
	 * Sqlite has a special syntax, as it does not need a database name or user authentication:
	 *
	 * <code>
	 * sqlite://file.db
	 * sqlite://../relative/path/to/file.db
	 * sqlite://unix(/absolute/path/to/file.db)
	 * sqlite://windows(c%2A/absolute/path/to/file.db)
	 * </code>
	 *
	 * @param string connection_url A connection URL
	 * @return object the parsed URL as an object.
	 */
	static connection_url_info parse_connection_url(std::string_view connection_url) {
		if (connection_url.empty()) {
			throw ex::config("Connection URL cannot be empty");
		}

		const auto sep_pos{ connection_url.find("://") };
		if (sep_pos == std::string_view::npos || sep_pos == 0) {
			throw ex::config("Invalid connection URL \"{}\": missing protocol", connection_url);
		}

		connection_url_info info{};
		info.protocol = std::string(connection_url.substr(0, sep_pos));

		std::string_view rest = connection_url.substr(sep_pos + 3);
		const auto query_pos{ rest.find('?') };
		std::string_view authority_and_path = (query_pos == std::string_view::npos) ? rest : rest.substr(0, query_pos);
		std::string_view query_str =
			(query_pos == std::string_view::npos) ? std::string_view{} : rest.substr(query_pos + 1);

		info.query = parse_query_params(query_str);
		const bool should_decode{ query_true(info.query, "decode") };
		if (auto iter = info.query.find("charset"); iter != info.query.end()) {
			info.charset = iter->second;
		}

		if (info.protocol == "sqlite") {
			parse_sqlite_connection(authority_and_path, info);
			return info;
		}

		parse_standard_connection(authority_and_path, should_decode, info);
		return info;
	}

	// - columns()

	/**
	 * Retrieve the insert id of the last model saved.
	 *
	 * @param string sequence Optional name of a sequence to use
	 * @return size_t
	 */
	virtual std::size_t insert_id() const = 0;

	/**
	 * Execute a raw SQL query on the database.
	 *
	 * @param sql - Raw SQL string to execute.
	 * @param attribute_vector - Optional array of bind values.
	 * @return a ptr to a statement that can be used to get the result set.
	 */
	virtual std::unique_ptr<statement> query(const std::string& sql, const attribute_vector& values = {}) = 0;

	// - query
	// - query_and_fetch_one
	// - query_and_fetch
	// - tables

	/**
	 * Begin a database transaction.
	 */
	virtual void begin_transaction() = 0;

	/**
	 * Commit the current transaction.
	 */
	virtual void commit() = 0;

	/**
	 * Rollback the current transaction.
	 */
	virtual void rollback() = 0;

	/**
	 * Quote a name like table names and field names.
	 * Properly escapes any embedded quote characters to prevent SQL injection.
	 *
	 * @param str - string to quote.
	 * @return string
	 */
	static std::string quote_name(std::string_view str) {
		// Handle already-quoted identifiers
		if (!str.empty() && str.front() == QUOTE_CHARACTER && str.back() == QUOTE_CHARACTER) {
			return std::string(str);
		}

		// Escape any quote characters within the identifier
		// SQL standard: double the quote character to escape it
		std::string escaped;
		escaped.reserve(str.size() + 2);
		escaped += QUOTE_CHARACTER;

		for (char c : str) {
			if (c == QUOTE_CHARACTER) {
				escaped += QUOTE_CHARACTER; // Double the quote to escape
			}
			escaped += c;
		}

		escaped += QUOTE_CHARACTER;
		return escaped;
	}

	/**
	 * Escapes quotes in a string.
	 *
	 * @param str - the string to be quoted.
	 * @return - string with any quotes in it properly escaped.
	 */
	virtual std::string escape(std::string_view str) const { return quote_name(str); }

	/**
	 * SQL fragment for inserting a row with all-default values.
	 */
	virtual std::string default_values_insert_clause() const { return "DEFAULT VALUES"; }

	// - date_to_string
	// - datetime_to_string
	// - string_to_datetime
	//

	/**
	 * Adds a limit clause to the SQL query.
	 *
	 * @param sql - a SQL statement to execute
	 * @param offset - row offset to start at.
	 * @param limit - maximum number of rows to return.
	 * @return on return sql will contain a SQL query that will limit results to specified parameters
	 */
	// ####JWD virtual void limit(std::string& sql, std::size_t inOffset, std::size_t inLimit) const = 0;
	virtual void limit(std::string& sql, long offset, long limit) const = 0;

	// - query_column_info
	// - query_for_tables
	// - set_encoding
	// - native_database_types

	/**
	 * Specifies whether or not adapter can use LIMIT/ORDER clauses with DELETE & UPDATE operations
	 *
	 * @internal
	 * @returns boolean (false by default)
	 */
	virtual bool accepts_limit_and_order_for_update_and_delete() const { return false; }

  private:
	static bool query_true(const std::map<std::string, std::string>& query, std::string_view key) {
		const auto iter{ query.find(std::string(key)) };
		if (iter == query.end()) {
			return false;
		}
		std::string value{ iter->second };
		std::transform(value.begin(), value.end(), value.begin(),
					   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
		return value == "1" || value == "true" || value == "yes" || value == "on";
	}

	static int parse_port(std::string_view str) {
		if (str.empty()) {
			throw ex::config("Invalid connection URL: empty port");
		}
		int port{};
		const auto* first = str.data();
		const auto* last = str.data() + str.size();
		auto [ptr, ec] = std::from_chars(first, last, port);
		if (ec != std::errc{} || ptr != last || port < 0 || port > 65535) {
			throw ex::config("Invalid connection URL: invalid port \"{}\"", str);
		}
		return port;
	}

	static std::string percent_decode(std::string_view value, bool plus_as_space = false) {
		std::string decoded;
		decoded.reserve(value.size());

		auto hex_val = [](char c) -> int {
			if (c >= '0' && c <= '9') {
				return c - '0';
			}
			if (c >= 'a' && c <= 'f') {
				return 10 + (c - 'a');
			}
			if (c >= 'A' && c <= 'F') {
				return 10 + (c - 'A');
			}
			return -1;
		};

		for (std::size_t i = 0; i < value.size(); ++i) {
			const char c = value[i];
			if (c == '%' && i + 2 < value.size()) {
				const int hi{ hex_val(value[i + 1]) };
				const int lo{ hex_val(value[i + 2]) };
				if (hi < 0 || lo < 0) {
					throw ex::config("Invalid connection URL: invalid percent encoding");
				}
				decoded += static_cast<char>((hi << 4) | lo);
				i += 2;
				continue;
			}
			if (plus_as_space && c == '+') {
				decoded += ' ';
				continue;
			}
			decoded += c;
		}
		return decoded;
	}

	static std::map<std::string, std::string> parse_query_params(std::string_view query) {
		std::map<std::string, std::string> out;
		while (!query.empty()) {
			const auto amp_pos{ query.find('&') };
			const std::string_view token = (amp_pos == std::string_view::npos) ? query : query.substr(0, amp_pos);
			query = (amp_pos == std::string_view::npos) ? std::string_view{} : query.substr(amp_pos + 1);
			if (token.empty()) {
				continue;
			}

			const auto eq_pos{ token.find('=') };
			const std::string_view key_sv = (eq_pos == std::string_view::npos) ? token : token.substr(0, eq_pos);
			const std::string_view val_sv =
				(eq_pos == std::string_view::npos) ? std::string_view{} : token.substr(eq_pos + 1);
			const std::string key{ percent_decode(key_sv, true) };
			const std::string value{ percent_decode(val_sv, true) };
			if (!key.empty()) {
				out[key] = value;
			}
		}
		return out;
	}

	static void parse_sqlite_connection(std::string_view value, connection_url_info& info) {
		if (value.empty()) {
			throw ex::config("Invalid sqlite connection URL: missing file path");
		}

		if (value.starts_with("unix(")) {
			if (value.back() != ')') {
				throw ex::config("Invalid sqlite connection URL: malformed unix(...) path");
			}
			info.file = std::string(value.substr(5, value.size() - 6));
			return;
		}

		if (value.starts_with("windows(")) {
			if (value.back() != ')') {
				throw ex::config("Invalid sqlite connection URL: malformed windows(...) path");
			}
			std::string path{ percent_decode(value.substr(8, value.size() - 9)) };
			std::replace(path.begin(), path.end(), '*', ':');
			info.file = std::move(path);
			return;
		}

		info.file = std::string(value);
	}

	static void parse_standard_connection(std::string_view value, bool should_decode, connection_url_info& info) {
		if (value.empty()) {
			throw ex::config("Invalid connection URL: missing host");
		}

		std::size_t slash_pos{ std::string_view::npos };
		const auto at_pos{ value.rfind('@') };
		const std::size_t host_start{ (at_pos == std::string_view::npos) ? 0 : (at_pos + 1) };
		const std::string_view host_segment = value.substr(host_start);

		if (host_segment.starts_with("unix(")) {
			const auto close_pos{ host_segment.find(')') };
			if (close_pos == std::string_view::npos) {
				throw ex::config("Invalid connection URL: malformed unix(...) host");
			}

			const std::size_t unix_end{ host_start + close_pos + 1 };
			if (unix_end < value.size()) {
				if (value[unix_end] != '/') {
					throw ex::config("Invalid connection URL: malformed unix(...) host");
				}
				slash_pos = unix_end;
			}
		} else {
			slash_pos = value.find('/', host_start);
		}

		const std::string_view authority = (slash_pos == std::string_view::npos) ? value : value.substr(0, slash_pos);
		const std::string_view db_path =
			(slash_pos == std::string_view::npos) ? std::string_view{} : value.substr(slash_pos + 1);
		if (authority.empty()) {
			throw ex::config("Invalid connection URL: missing host");
		}

		std::string_view userinfo;
		std::string_view hostinfo = authority;
		const auto authority_at_pos{ authority.rfind('@') };
		if (authority_at_pos != std::string_view::npos) {
			userinfo = authority.substr(0, authority_at_pos);
			hostinfo = authority.substr(authority_at_pos + 1);
		}

		if (!userinfo.empty()) {
			const auto colon{ userinfo.find(':') };
			const std::string_view user_sv = (colon == std::string_view::npos) ? userinfo : userinfo.substr(0, colon);
			const std::string_view pass_sv =
				(colon == std::string_view::npos) ? std::string_view{} : userinfo.substr(colon + 1);
			info.user = should_decode ? percent_decode(user_sv) : std::string(user_sv);
			if (!pass_sv.empty()) {
				info.password = should_decode ? percent_decode(pass_sv) : std::string(pass_sv);
			}
		}

		if (hostinfo.starts_with("unix(")) {
			if (hostinfo.back() != ')') {
				throw ex::config("Invalid connection URL: malformed unix(...) host");
			}
			const std::string socket{ std::string(hostinfo.substr(5, hostinfo.size() - 6)) };
			if (socket.empty()) {
				throw ex::config("Invalid connection URL: empty unix socket path");
			}
			info.socket = socket;
			info.host = socket;
		} else if (!hostinfo.empty() && hostinfo.front() == '[') {
			const auto close{ hostinfo.find(']') };
			if (close == std::string_view::npos) {
				throw ex::config("Invalid connection URL: malformed IPv6 host");
			}
			info.host = std::string(hostinfo.substr(1, close - 1));
			if (close + 1 < hostinfo.size()) {
				if (hostinfo[close + 1] != ':') {
					throw ex::config("Invalid connection URL: malformed host/port");
				}
				info.port = parse_port(hostinfo.substr(close + 2));
			}
		} else {
			const auto colon{ hostinfo.rfind(':') };
			if (colon != std::string_view::npos && hostinfo.find(':') == colon) {
				info.host = std::string(hostinfo.substr(0, colon));
				info.port = parse_port(hostinfo.substr(colon + 1));
			} else {
				info.host = std::string(hostinfo);
			}
		}

		if (!info.host.has_value() || info.host->empty()) {
			throw ex::config("Invalid connection URL: missing host");
		}

		if (!db_path.empty()) {
			info.database = should_decode ? percent_decode(db_path) : std::string(db_path);
		}
	}
};

} // namespace ar
