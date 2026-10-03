// ============================================================================
// config.h
//
// SPDX-FileCopyrightText: 2020-2023 Indridi Bjornsson
// SPDX-License-Identifier: MIT
// See file LICENSE.MIT for detail
//

#pragma once

#include <activerecord/activerecord.h>
#include <activerecord/connection.h>
#include <activerecord/connection_factory.h>
#include <map>
#include <memory>

namespace ar {

/**
 * a singleton containing all configuration for active record
 */
class config {
  public:
	static config& instance() {
		static config instance_;
		return instance_;
	}

	config(config const&) = delete;
	void operator=(config const&) = delete;

	using connection_ptr = std::shared_ptr<connection>;
	using connection_map = std::map<std::string, connection_ptr>;

	/**
	 * Sets all the connections for this config.
	 *
	 * @param connections - a map of all the connections to set.
	 */
	void set_connections(const connection_map& connections) {
		connections_ = connections;
		default_connection_.clear();
	}

	/**
	 * Sets all the connections for this config and the default connection.
	 *
	 * @param connections - a map of all the connections to set.
	 * @param string default_connection - a name of the default connection to use.
	 */
	void set_connections(const connection_map& connections, std::string_view default_connection) {
		connections_ = connections;
		default_connection_ = default_connection;
	}

	/**
	 * Sets a named connection from URL.
	 */
	void set_connection_url(std::string_view name, std::string_view url) {
		connections_[std::string(name)] = make_connection(url);
		if (default_connection_.empty()) {
			default_connection_ = std::string(name);
		}
	}

	/**
	 * Sets multiple named connections from URL map.
	 */
	void set_connection_urls(const std::map<std::string, std::string>& urls, std::string_view default_connection = {}) {
		connection_map mapped;
		for (const auto& [name, url] : urls) {
			mapped[name] = make_connection(url);
		}
		connections_ = std::move(mapped);
		default_connection_ = std::string(default_connection);
	}

	/**
	 * Gets a connection with the name 'con_name'.
	 *
	 * @param con_name - An optional name of the connection to get.
	 * @return connection - a reference to the connection with the name 'con_name' or the
	 *					   default connection if no name is provided.
	 * @throws - exception if not found.
	 */
	connection_ptr get_connection(std::string_view con_name) {
		auto iter{ connections_.end() };
		if (!con_name.empty()) {
			iter = connections_.find(std::string(con_name));
		} else if (!default_connection_.empty()) {
			iter = connections_.find(default_connection_);
		}

		if (iter == connections_.end()) {
			throw ex::config("Unable to find config \"{}\"", con_name);
		}
		return iter->second;
	}

	/**
	 * Gets the default connection.
	 *
	 * @return connection - a reference to the default connection.
	 * @throws - exception if no default connection can be found.
	 */
	connection_ptr get_connection() { return get_connection(default_connection_); }

  private:
	config() = default;

	std::string default_connection_;
	connection_map connections_;
};

} // namespace ar
