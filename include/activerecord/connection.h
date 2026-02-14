// ============================================================================
// connection.h
//
// SPDX-FileCopyrightText: 2020-2023 Indridi Bjornsson
// SPDX-License-Identifier: MIT
// See file LICENSE.MIT for detail
//

#pragma once

#include <activerecord.h>
#include <attribute.h>

#include <memory>


namespace ar {


/**
* a general db statement class 
*/
class statement {
public:
	statement() {}
	virtual ~statement() {}
	virtual void execute(const std::string& cmd, const attribute_vector& values) = 0;
	virtual bool fetch() { return false; }

	virtual void get_attributes(attribute_map &attributes) {}

};



/**
* a base class for database connection adapters.
*/
class connection {

public:
	/**
	* The quote character used for quoting column and field names.
	*/
	static const char QUOTE_CHARACTER = '`';

	connection() {}

	virtual ~connection() {}

	/**
	* Tells you if this adapter supports sequences or not.
	*
	* @return bool
	*/
	virtual bool supports_sequences() const { return false;	}

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
	static void parse_connection_url(const std::string& connection_url) {
		// fixme:
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

	// - transaction
	// - commit
	// - rollback

	/**
	* Quote a name like table names and field names.
	*
	* @param str - string to quote.
	* @return string
	*/
	static std::string quote_name(const std::string& str)
	{
		if ( ((*str.begin()) == QUOTE_CHARACTER) || ((*str.rbegin()) == QUOTE_CHARACTER) )  {
			return str;
		}
		return QUOTE_CHARACTER + str + QUOTE_CHARACTER;
	}

	/**
	* Escapes quotes in a string.
	*
	* @param str - the string to be quoted.
	* @return - string with any quotes in it properly escaped.
	*/
	virtual std::string escape(const std::string& str) const
	{
		return quote_name(str);
	}


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
	//####JWD virtual void limit(std::string& sql, std::size_t inOffset, std::size_t inLimit) const = 0;
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

};

} // namespace ar
