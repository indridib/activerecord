// ============================================================================
// model.h
//
// SPDX-FileCopyrightText: 2020-2023 Indridi Bjornsson
// SPDX-License-Identifier: MIT
// See file LICENSE.MIT for detail
//

#pragma once

#include <activerecord.h>
#include <connection.h>
#include <config.h>
#include <attribute.h>
#include <sql_builder.h>

#include <map>
#include <set>

namespace ar {

/**
* Base class for all models.
*/
class model {
friend class sql_builder;

public:

	/**
	* Constructor for a model.
	*
	* @param table_name - name of the database table this model is connected to.
	* @param primary_key - name of the primary key of the table.
	* @param con_name - name of the connection to use.
	*/
	model(std::string_view table_name, std::string_view primary_key = "id", std::string_view con_name = {})
		: table_name_(table_name)
		, primary_key_(primary_key)
		, connection_(config::instance().get_connection(con_name))
	{}

	/**
	* Return the database table name.
	*
	* @return - The database table name.
	*/
	std::string_view table_name() const noexcept { return table_name_; }

	/**
	* Returns the primary key name.
	*
	* @return - The primary key for the model.
	*/
	std::string_view get_primary_key() const noexcept { return primary_key_; }



	// =========================================================================
	// Attribute handling
	//
	// NOTE: we are not implementing the following methods because they are basically
	// only used by get() and set()
	// - assign_attribute()
	// - read_attribute()


	/**
	* Returns a reference to the model's attributes.
	*
	* @return - a constant reference to the attributes.
	*/
	const attribute_map& attributes() const noexcept { return attributes_; }



	/**
	* Determines if an attribute exists for this model.
	*
	* @param name - attribute name.
	* @return - true if the attribute exist.
	*/
	bool isset(std::string_view name) const noexcept
	{
		std::string key(name);
		const auto& aliases = alias_attributes();
		return attributes_.find(key) != attributes_.end() || aliases.find(key) != aliases.end();
	}

	/**
	* Returns the actual attribute name if name is aliased.
	*
	* @param name - attribute name.
	* @return - real name of the attribute or empty if attribute can not be found.
	*/
	std::string get_real_attribute_name(std::string_view name) const noexcept
	{
		std::string key(name);
		if (attributes_.find(key) != attributes_.end())
			return key;
		const auto& aliases = alias_attributes();
		auto iter = aliases.find(key);
		if (iter != aliases.end())
			return iter->second;
		return {};
	}


	/**
	* Retrieves an attribute's value or a relationship object based on the name passed. If the attribute
	* accessed is 'id' then it will return the model's primary key no matter what the actual attribute name is
	* for the primary key.
	*
	* @param name - attribute name.
	* @return - the value of the attribute.
	* @throws ex::attribute_not_found in case the attribute does not exist.
	*/
	const attribute& get(const std::string_view name) const {
		std::string real_name{ name };
		const auto& aliases = alias_attributes();
		auto alias_iter = aliases.find(real_name);
		if (alias_iter != aliases.end())
			real_name = alias_iter->second;

		auto attrib_iter = attributes_.find(real_name);
		if ( (attrib_iter == attributes_.end()) && (real_name == "id")) {
			// try primary key name
			attrib_iter = attributes_.find(std::string(get_primary_key()));
		}

		if (attrib_iter == attributes_.end()) {
			throw ex::attribute_not_found("Attribute \"{}\" not found", name);
		}
		return attrib_iter->second;
	} 

	// Overloading [] operator to access attributes
	const attribute &operator[] (const std::string_view name) const { return get(name); }



	// -------------------------------------------
	// modifying attributes


	/**
	* Mass update the model with data from an attributes map.
	*
	* Unlike update_attributes() this method only updates the model's data
	* but DOES NOT save it to the database.
	*
	* @param attribute_map attributes - attributes to update.
	* @param bool allow_new_attributes - if true will allow creating new attributes,
	*									  if false will throw an exception if the attribute does not already exist.
	*/
	void set_attributes(const attribute_map& attributes, bool allow_new_attributes = true) {
		for(const auto& attrib : attributes)
			set(attrib.first, attrib.second, allow_new_attributes);
	}

	/**
	* Mass update the model with data from an attributes map (move semantics overload).
	*
	* Unlike update_attributes() this method only updates the model's data
	* but DOES NOT save it to the database.
	*
	* @param attribute_map attributes - attributes to update (moved).
	* @param bool allow_new_attributes - if true will allow creating new attributes,
	*									  if false will throw an exception if the attribute does not already exist.
	*/
	void set_attributes(attribute_map&& attributes, bool allow_new_attributes = true) {
		for(auto& attrib : attributes)
			set(attrib.first, std::move(attrib.second), allow_new_attributes);
	}


	/**
	* Mass update the model with an array of attribute data and saves to the database.
	*
	* @param attribute_map attributes - attributes to update.
	* @param bool allow_new_attributes - if true will allow creating new attributes,
	*									  if false will throw an exception if the attribute does not already exist.
	* @return bool - true if successfully updated and saved otherwise false.
	*/
	bool update_attributes(const attribute_map& attributes, bool allow_new_attributes = true)
	{
		set_attributes(attributes, allow_new_attributes);
		return save();
	}

	/**
	* Sets a single attribute.
	*
	* @param name - attribute name.
	* @param value - attributes value.
	* @param bool allow_new_attributes - if true will allow creating new attributes,
	*									  if false will throw an exception if the attribute does not already exist.
	*/
	void set(const std::string_view name, const attribute& value, bool allow_new_attributes = true)
	{
		std::string real_name{ name };
		const auto& aliases = alias_attributes();
		auto alias_iter = aliases.find(real_name);
		if (alias_iter != aliases.end())
			real_name = alias_iter->second;

		// Primary key is immutable for persisted records.
		if (!new_record_ && real_name == primary_key_) {
			throw ex::readonly("Primary key \"{}\" cannot be modified on persisted {}", primary_key_, table_name_);
		}

		auto attrib_iter = attributes_.find(real_name);
		if ( (attrib_iter == attributes_.end()) && (real_name == "id")) {
			// try primary key name
			attrib_iter = attributes_.find(std::string(get_primary_key()));
		}

		if ( attrib_iter != attributes_.end() ) {
			attrib_iter->second = value;
			flag_dirty(attrib_iter->first);
		} else {
			// This attribute does not exist.
			if (!allow_new_attributes) {
				throw ex::attribute_not_found("Attribute \"{}\" not found", name);
			}
			// If allowing new attributes we just insert the item
			attributes_[real_name] = value;
			flag_dirty(real_name);
		}
	}

	/**
	* Sets a single attribute (move semantics overload).
	*
	* @param name - attribute name.
	* @param value - attributes value (moved).
	* @param bool allow_new_attributes - if true will allow creating new attributes,
	*									  if false will throw an exception if the attribute does not already exist.
	*/
	void set(const std::string_view name, attribute&& value, bool allow_new_attributes = true)
	{
		std::string real_name{ name };
		const auto& aliases = alias_attributes();
		auto alias_iter = aliases.find(real_name);
		if (alias_iter != aliases.end())
			real_name = alias_iter->second;

		// Primary key is immutable for persisted records.
		if (!new_record_ && real_name == primary_key_) {
			throw ex::readonly("Primary key \"{}\" cannot be modified on persisted {}", primary_key_, table_name_);
		}

		auto attrib_iter = attributes_.find(real_name);
		if ( (attrib_iter == attributes_.end()) && (real_name == "id")) {
			// try primary key name
			attrib_iter = attributes_.find(std::string(get_primary_key()));
		}

		if ( attrib_iter != attributes_.end() ) {
			attrib_iter->second = std::move(value);
			flag_dirty(attrib_iter->first);
		} else {
			// This attribute does not exist.
			if (!allow_new_attributes) {
				throw ex::attribute_not_found("Attribute \"{}\" not found", name);
			}
			// If allowing new attributes we just insert the item
			attributes_[real_name] = std::move(value);
			flag_dirty(real_name);
		}
	}

	/**
	* Updates a single attribute and saves the record without going through the normal validation procedure.
	*
	* @param name - attribute name.
	* @param value - attributes value.
	* @return bool - true if successful otherwise false.
	*/
	bool update_attribute(const std::string_view name, const attribute& value)
	{
		set(name, value, false);  // Don't allow new attributes for update_attribute
		return update(false);
	}

	/**
	* Save the model to the database.
	*
	* This function will automatically determine if an INSERT or UPDATE needs to occur.
	* If a validation returns false, then the model will not be saved and this will return false.
	*
	* If saving an existing model only data that has changed will be saved.
	*
	* @param should_validate - set to true or false depending on if you want the validators to run or not.
	* @return - true if the model was saved to the database otherwise false.
	*/
	bool save(bool should_validate = true)
	{
		verify_not_readonly("save");
		bool success{ is_new_record() ? insert(should_validate) : update(should_validate) };
		if (success) {
			reset_dirty();
		}
		return success;
	}


	// -------------------------------------------
	// Dirty handling

	/**
	* Returns a set of attributes that have been modified since loading the model.
	*
	* @return - a set of attributes or empty if no attributes are dirty.
	*/
	const string_set& dirty_attributes() const noexcept { return dirty_; }

	/**
	* Flags an attribute as dirty.
	*
	* @param name - attribute name.
	*/
	void flag_dirty(std::string_view name) noexcept { dirty_.insert(std::string(name)); }

	/**
	* Check if a particular attribute has been modified since loading the model.
	*
	* @param name - attribute name.
	* @return - true if the attribute has been modified.
	*/
	bool attribute_is_dirty(std::string_view name) const noexcept { return dirty_.find(std::string(name)) != dirty_.end(); }

	/**
	* Returns true if the model has been modified.
	*
	* @return - true if he model has been modified
	*/
	bool is_dirty() const noexcept { return !dirty_.empty(); }

	/**
	* Resets the dirty array, making the model clean.
	*
	*/
	void reset_dirty() noexcept { dirty_.clear(); }






	// - get_validation_rules()
	// - get_values_for()
	// - is_delegated()

	/**
	* Determine if the model is in read-only mode.
	*
	* @return boolean - true if the model is readonly
	*/
	bool is_readonly() const noexcept { return readonly_; }

	/**
	* Flag the model as readonly or not
	*
	* @param boolean readonly - Set to true to put the model into readonly mode
	*/
	void readonly(bool readonly) noexcept { readonly_ = readonly; }

	/**
	* Determine if the model is a new record.
	*
	* @return boolean - true if this is a new record
	*/
	bool is_new_record() const noexcept { return new_record_; }

	/**
	* Throws an exception if this model is set to readonly.
	*
	* @throws ex::readonly
	* @param string method_name - the name of method that was invoked on model for 
	* exception message
	*/
	void verify_not_readonly(const std::string_view method_name) const
	{
		if (readonly_) {
			throw ex::readonly("\"{}\" is not allowed since model is read-only", method_name);
		}
	}

	void find(std::size_t inValue) {
		// TODO(issue-6): pending implementation.
		// Note: Use query<Model>().find(id) instead
		throw ex::activerecord("model::find() not implemented - use query<Model>().find(id)");
	}

	void find_by_pk(std::size_t inValue) {
		// TODO(issue-6): pending implementation.
		// Note: Use query<Model>().find(id) instead
		throw ex::activerecord("model::find_by_pk() not implemented - use query<Model>().find(id)");
	}

	// - connection()
	// - reestablish_connection()
	// - table()
	// - create()
	// - delete()
	// - values_for_pk()
	// - values_for()
	// - set_timestamps() * set timestamp for update_at and create_at
	// - set_relationship_from_eager_load()
	// - reload()
	// - __clone()


	/**
	* Run validations on model and returns whether or not model passed validation.
	*
	* @see is_invalid()
	* @return - true if the model is valid
	*/
	bool is_valid() const
	{
		return validate();
	}

	/**
	* Runs validations and returns true if invalid.
	*
	* @see is_valid()
	* @return - true if the model is invalid
	*/
	bool is_invalid() const
	{
		return !validate();
	}

	/**
	* Returns a JSON representation of this model.
	* 
	* @param only - if not empty names of attributes that will be included
	* @param exclude - if not empty names of attributes that will be excluded
	* @return - string JSON representation of the model
	*/
	std::string to_json_string(const string_vector& only = {}, const string_set& exclude = {}) const { 
		return ar::to_json_string(attributes_, only, exclude);
	}

	/**
	* Returns a XML representation of this model.
	* 
	* @param obj_name - name of the object, if empty table name will be used
	* @param only - if not empty names of attributes that will be included
	* @param exclude - if not empty names of attributes that will be excluded
	* @return - XML representation of the model
	*/
	std::string to_xml_string(std::string_view obj_name = {}, const string_vector& only = {}, const string_set& exclude = {}) const {
		if (obj_name.empty())
			return ar::to_xml_string(attributes_, table_name_, only, exclude);
		else
			return ar::to_xml_string(attributes_, std::string(obj_name), only, exclude);
	}

	// - to_xml()
	// - to_csv()
	// - to_array()
	// - transaction()

protected:

	// NOTE: hooks to add functionality
	//	before_create
	//	after_create
	//	before_update
	//	after_update
	//	before_destroy
	//	after_destroy
	//	before_validation
	//	after_validation


	/**
	* Issue an UPDATE sql statement for this model's dirty attributes.
	*
	* @see save
	* @param bool should_validate - set to true or false depending on if you want the validators to run or not
	* @return bool - true if the model was saved to the database otherwise false
	*/
	bool update(bool should_validate = true)
	{
		verify_not_readonly("update");

		if (!is_dirty()) {
			return true;
		}
		if (should_validate && !validate()) {
			return false;
		}
		if (primary_key_.empty()) {
			throw ex::activerecord("Unable to update, no primary key defined for {}", table_name_);
		}
		const attribute& pk_value = get(primary_key_);
		string_vector names;
		attribute_vector values;
		for(const auto& attrib : dirty_attributes()) {
			if (attrib == primary_key_) {
				continue;
			}
			names.emplace_back(attrib);
			values.emplace_back(get(attrib));
		}

		// Nothing to update after excluding primary key.
		if (names.empty()) {
			return true;
		}

		sql_builder builder(connection_, table_name_);
		builder.update(names, values);
		builder.add_where(std::string(primary_key_) + " = ?", {pk_value});
		auto stmt{ connection_->query(builder.to_string(), builder.get_bind_values()) };
		return true;
	}

	/**
	* Issue an INSERT sql statement for this model's attribute.
	*
	* @see save
	* @param bool inValidate - set to true or false depending on if you want the validators to run or not
	* @return bool - true if the model was saved to the database otherwise false
	*/
	bool insert(bool inValidate = true)
	{
		verify_not_readonly("insert");

		if (!is_dirty()) {
			return true;
		}
		if (inValidate && !validate()) {
			return false;
		}
		string_vector names;
		attribute_vector values;
		for(const auto& attrib : dirty_attributes()) {
			// Make sure that primary keys isn't set
			if (attrib != primary_key_) {
				names.emplace_back(attrib);
				values.emplace_back(get(attrib));
			}
		}

		sql_builder builder(connection_, table_name_);
		builder.insert(names, values);
		auto stmt{ connection_->query(builder.to_string(), builder.get_bind_values()) };

		// Get the auto-generated primary key and set it on the model (don't mark as dirty)
		std::size_t id = connection_->insert_id();
		if (id > 0) {
			attributes_[std::string(primary_key_)] = static_cast<int64_t>(id);  // Set directly without marking dirty
		}

		// Mark as persisted (no longer a new record)
		new_record_ = false;

		return true;
	}


	/**
	* Validates the model.
	*
	* @return bool - true if passed validators otherwise false
	*/
	bool validate() const {
		// FixMe: do the stuff
		return true;
	}


	static const std::map<std::string, std::string>& alias_attributes() noexcept
	{
		static const std::map<std::string, std::string> aliases{};
		return aliases;
	}

	// Per-instance metadata kept lightweight to reduce model footprint.
	config::connection_ptr connection_;	// a reference to the database connection to use 
	std::string_view table_name_;		// name of the database table (expected static lifetime)
	std::string_view primary_key_;		// name of primary key, default id

public: //###JWD
	attribute_map attributes_;	// values as column_name => value
	bool new_record_{ true };	// wether this is a fresh copy not from database. 
								// Note: we could also test if the primary key is valid
protected:
	string_set dirty_;			// attributes that are dirty
	bool readonly_{ false };	// no writing

};



} // namespace ar

