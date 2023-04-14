// ============================================================================
// config.h
//
// SPDX-FileCopyrightText: 2020-2023 Indridi Bjornsson
// SPDX-License-Identifier: MIT
// See file LICENSE.MIT for detail
//

#include <adapters/SQLApi_adapter.h>

namespace ar {

	SQLApi_statement::
SQLApi_statement(SAConnection *pConnection) 
	: statement()
	, cmd(pConnection) 
{}


	void SQLApi_statement::
execute(const std::string& command, const attribute_vector& values) 
{
	if (!values.empty()) {
		// need to replace ? with proper index :1, :2 , etc and then bind the arguments
		// NOTE: we are assuming that the cmd doesn't contain any question marks that shouldn't be 
		// bound to variables
		std::string newCmd{};
		int index = 1;
		for (const auto& ch : command) {
			if (ch == '?') {
				newCmd += ":" + std::to_string(index);
				index++;
			} else {
				newCmd += ch;
			}
		}
		cmd.setCommandText(newCmd.c_str());

		index = 1;
		for (const auto& param : values) {
			switch(param.type()) {
				case type_null:
					cmd.Param(index).setAsNull();
					break;
				case type_string:
					cmd.Param(index).setAsString() = param.as_string().c_str();
					break;
				default:
					cmd.Param(index).setAsLong() = static_cast<long>(param.as_numeric());
					break;
			}
			index++;
		}
	} else {
		cmd.setCommandText(command.c_str());
	}

	cmd.Execute();
}

	bool SQLApi_statement::
fetch() {
	return cmd.FetchNext();
}

	attribute SQLApi_statement::
field_as_attribute(const SAField &field)
{
	if (field.isNull()) {
		return attribute();
	}
	switch(field.FieldType()) {
		case SA_dtBool:
			return attribute(field.asBool());

		case SA_dtShort:
		case SA_dtUShort:
		case SA_dtLong:
		case SA_dtULong:
		case SA_dtNumeric:
			return attribute((int64_t )field.asLong());

		case SA_dtDouble:
			return attribute(field.asDouble());

		case SA_dtDateTime:
		case SA_dtInterval:
			return attribute(static_cast<std::time_t>(field.asLong()));

		case SA_dtString:
		case SA_dtBytes:
		case SA_dtLongBinary:
		case SA_dtLongChar:
		case SA_dtBLob:
			return attribute((const char *)field.asString());

		// case SA_dtUnknown:
		default:
			return attribute();
	}
	return attribute(); // we like happy compilers, right :-)
}


	 void SQLApi_statement::
get_attributes(attribute_map &attributes) {
	
	for(int i = 0; i < cmd.FieldCount(); i++) {
		const SAField &field = cmd.Field(i + 1);
		attributes.emplace(field.Name(), field_as_attribute(field));
	}
}




	SQLApi_adapter::
SQLApi_adapter(const std::string& db, const std::string& user, const std::string& password)
	: connection()
{
	// FIXME: Currently we are hardcoding the database file as SQLite
	SAClient_t client{ SA_SQLite_Client };
	try {
		connection_.setOption(_TSA("SQLITE.LIBS")) = _TSA("STATIC");
		connection_.setOption(_TSA("BusyTimeout")) = _TSA("100000"); // 100 seconds
		connection_.Connect( db.c_str(), user.c_str(), password.c_str(), client );

	} catch(...) {
		// FixMe: throw something sensible
	}
}

	SQLApi_adapter::
~SQLApi_adapter()
{}


	std::unique_ptr<statement> SQLApi_adapter::
query(const std::string& sql, const attribute_vector& values)
{
	std::unique_ptr<statement> stmt(new SQLApi_statement(&connection_));
	stmt->execute(sql, values);
	return stmt;
}


	void SQLApi_adapter::
limit(std::string& sql, long offset, long limit) const
{
	// sqlite: LIMIT number_rows OFFSET offset_value;
	if (limit >= 0) {
		sql += " LIMIT " + std::to_string(limit);
		if (offset >= 0) {
			sql += " OFFSET " + std::to_string(offset);
		}
	}
}

} // namespace ar
