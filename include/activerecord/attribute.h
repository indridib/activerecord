// ============================================================================
// attribute.h
//
// SPDX-FileCopyrightText: 2020-2023 Indridi Bjornsson
// SPDX-License-Identifier: MIT
// See file LICENSE.MIT for detail
//

#pragma once

#include <activerecord.h>

#include <map>
#include <boost/variant.hpp>
#include <ctime>



namespace ar {

// NOTE: PHP Active Record defines the following types: 
//	STRING, INTEGER, DECIMAL, DATETIME, DATE, TIME
enum attribute_type {
	  type_null		= 0
	, type_string	= 1
	, type_integer	= 2
	, type_decimal	= 3
	, type_bool		= 4
	, type_date		= 5
};
typedef void* attribute_ptr;
typedef boost::variant<attribute_ptr, std::string, int64_t, double, bool /*, std::time_t*/> attribute_variant;


/**
* a single attribute can be of a different type and can have different type of values
*/
class attribute : public attribute_variant {
public:
	attribute() : attribute_variant(nullptr) {}
	attribute(bool value) : attribute_variant((bool )value) {}
	attribute(int64_t value) : attribute_variant((int64_t )value) {}
	attribute(int value) : attribute_variant((int64_t )value) {}
	attribute(const std::string& value) : attribute_variant(value) {}
	attribute(const char * value) : attribute_variant(std::string(value)) {}
	attribute(double value) : attribute_variant((double )value) {}
	attribute_type type() const { return static_cast<attribute_type>(which()); }

	bool is_null() const { return type() == type_null; }

	std::string		as_string() const	{ return boost::get<std::string>(*this); }
	bool			as_bool() const		{ return boost::get<bool>(*this); }
	int64_t			as_integer() const	{ return boost::get<int64_t>(*this); }
	double			as_real() const		{ return boost::get<double>(*this); }
	std::time_t		as_date() const		{ return boost::get<std::time_t>(*this); }

	/**
	* Returns a numberic value of the attribute if possible
	* Value types that can be converted to numeric are: null, bool, integer, double, date
	* @throws std::invalid_argument if it is unable to convert the value
	*/
	int64_t as_numeric() const {
		switch(type()) {
			case type_null:		return 0;
			case type_string:	return std::stoll(boost::get<std::string>(*this));
			case type_integer:	return boost::get<int64_t>(*this);
			case type_decimal:	return static_cast<int64_t>(boost::get<double>(*this));
			case type_bool:		return (boost::get<bool>(*this) ? 1 : 0);
			case type_date:		return static_cast<int64_t>(boost::get<std::time_t>(*this)); // fixme: config should keep date/time format
		}
		throw std::invalid_argument("Unable to convert attribute to numeric");
	}



	/**
	* Returns a string version of the attribute.
	* @throws std::invalid_argument if it is unable to convert the value
	*/
	std::string to_string() const {
		switch(type()) {
			case type_null:		return "";
			case type_string:	return boost::get<std::string>(*this);
			case type_integer:	return std::to_string(boost::get<int64_t>(*this));
			case type_decimal:	return std::to_string(boost::get<double>(*this));
			case type_bool:		return std::to_string(boost::get<bool>(*this));
			case type_date:		return std::to_string(boost::get<std::time_t>(*this)); // fixme: config should keep date/time format
		}
		throw std::invalid_argument("Unable to convert attribute to string");
	}
};

typedef std::map<std::string, attribute> attribute_map;
typedef std::vector<attribute> attribute_vector;


/**
* Return all keys from attributes. If only is definied contains only those keys else we will select all keys
* excluding those definied in excluded.
*
* @param attributes - attributes to use.
* @param only - if not empty names of attributes that will be included.
* @param exclude - if not empty names of attributes that will be excluded.
* @return - vector of keys to use.
*/
string_vector get_attrib_keys(const attribute_map& attributes, const string_vector& only = {}, const string_set& exclude = {}) noexcept;

/**
* Return attributes as JSON string.
* 
* @param attributes - attributes to use.
* @param only - if not empty names of attributes that will be included.
* @param exclude - if not empty names of attributes that will be excluded.
* @return - string JSON representation of the attributes.
*/
std::string to_json_string(const attribute_map& attributes, const string_vector& only = {}, const string_set& excluded = {}) noexcept;

/**
* Return attributes as XML string.
* 
* @param attributes - attributes to use.
* @param only - if not empty names of attributes that will be included.
* @param exclude - if not empty names of attributes that will be excluded.
* @return - string XML representation of the attributes.
*/
std::string to_xml_string(const attribute_map& attributes, const std::string& obj_name, const string_vector& only = {}, const string_set& exclude = {}) noexcept;


} // namespace ar

namespace std {
	std::string to_string(const ar::attribute_map& attributes) noexcept;
	inline std::string to_string(const ar::attribute& attrib) noexcept { return attrib.to_string(); }
}

