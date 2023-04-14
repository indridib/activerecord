// ============================================================================
// config.h
//
// SPDX-FileCopyrightText: 2020-2023 Indridi Bjornsson
// SPDX-License-Identifier: MIT
// See file LICENSE.MIT for detail
//

#pragma once

#include <activerecord.h>
#include <connection.h>

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

	config(config const&)			= delete;
	void operator=(config const&)	= delete;
	
	typedef std::shared_ptr<connection> connection_ptr;
	typedef std::map<std::string, connection_ptr> connection_map;


	/**
	* Sets all the connections for this config.
	*
	* @param connections - a map of all the connections to set.
	*/
	void set_connections(const connection_map& connections) noexcept {
		connections_ = connections;
	}

	/**
	* Sets all the connections for this config and the default connection.
	*
	* @param connections - a map of all the connections to set.
	* @param string default_connection - a name of the default connection to use.
	*/
	void set_connections(const connection_map& connections, const std::string& default_connection) noexcept {
		connections_ = connections;
		default_connection_ = default_connection;
	}

	/**
	* Gets a connection with the name 'con_name'.
	*
	* @param con_name - An optional name of the connection to get.
	* @return connection - a reference to the connection with the name 'con_name' or the 
	*					   default connection if no name is provided.
	* @throws - exception if not found.
	*/
	connection_ptr get_connection(const std::string& con_name) {
		auto iter = connections_.begin();
		if (!con_name.empty()) {
			iter = connections_.find(con_name);
		} else if (!default_connection_.empty()) {
			iter = connections_.find(default_connection_);
		}

		if (iter == connections_.end()) {
			throw ex::config("Unable to find config \"" + con_name + "\"" );
		}
		return iter->second;
	}

	/**
	* Gets the default connection.
	*
	* @return connection - a reference to the default connection.
	* @throws - exception if no default connection can be found.
	*/
	connection_ptr get_connection() {
		return get_connection(default_connection_);
	}

private:
	config() {}

	std::string default_connection_;
	connection_map connections_;
};

} // namespace ar
