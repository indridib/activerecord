// ==============================================================================
// tests/package_consumer/main.cpp
//
// SPDX-License-Identifier: MIT
//
// Smoke test for the installed package: save and reload one model through SQLite.
// Exits 0 on success. Uses the library's public include style (<model.h>).
// ==============================================================================

#include <adapters/sqlite_adapter.h>
#include <config.h>
#include <exception>
#include <iostream>
#include <memory>
#include <model.h>
#include <sql_builder.h>

namespace {

class Item : public ar::model {
  public:
	static const char* st_table_name() { return "items"; }
	static const char* st_primary_key() { return "id"; }

	Item() : model(st_table_name(), st_primary_key()) {}
};

} // namespace

int main() {
	try {
		auto conn{ std::make_shared<ar::sqlite_adapter>(":memory:") };
		ar::config::instance().set_connections({ { "default", conn } }, "default");
		conn->query("CREATE TABLE items (id INTEGER PRIMARY KEY AUTOINCREMENT, name TEXT NOT NULL)", {});

		Item item;
		item.set("name", "packaged");
		if (!item.save()) {
			std::cerr << "save failed\n";
			return 1;
		}

		const auto loaded{ ar::query<Item>().where("name = ?", "packaged").first() };
		if (!loaded || loaded->get("name").as_string() != "packaged") {
			std::cerr << "round trip failed\n";
			return 1;
		}

		std::cout << "package consumer OK\n";
		return 0;
	} catch (const std::exception& e) {
		std::cerr << "error: " << e.what() << '\n';
		return 1;
	}
}
