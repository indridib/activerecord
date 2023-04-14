// activerecord.cpp 
//
#include <config.h>
#include <adapters/SQLApi_adapter.h>
#include <model.h>
#include <sql_builder.h>

#include <iostream>


class Zone : public ar::model {
public:
	static const char * st_table_name() { return "mm_zones"; }
	static const char * st_primary_key() { return "zoneid"; }

	Zone() : model(st_table_name(), st_primary_key()) {}
};



int main()
{

	ar::config::instance().set_connections({ 
		{ "default", std::shared_ptr<ar::connection>(new ar::SQLApi_adapter("example.db")) }
	});

/*
	Zone zoneb;
	zoneb.set_attributes( {	{"zonename", "example.com"}
							, {"zonetype", 2}
							, {"zonename_lower", "example.com"}
							, {"viewid", 0}

							, {"enabled", true}
							, {"dynamic", false}
							, {"adintegrated", true}

							, {"timestamp", 0}
							, {"syncserverid", 0}
							, {"category", 0}
							, {"dnssecsigned", false}
							, {"rpz", false}
							, {"isprivate", false}
							});
	zoneb.save();

*/
	Zone zonea{ ar::query<Zone>().find(10) };
	std::cout << zonea["zoneid"] << "\t" << zonea["zonename"] << "\t" << zonea["zonetype"] << std::endl;
	zonea.set_attributes( {{"zonename", "indi.test2.menandmice.is."}, {"zonetype", 3}});
	zonea.save();


	ar::query<Zone> test;
	for(const auto& iter : test.order("zoneid").where("zonetype = ?", 1 ).all()) {
		// std::cout << iter["zonename"] << "\t" << iter["zonetype"] <<std::endl;
		std::cout << iter.to_json_string() <<std::endl;
	}

#if 0
	std::cout << "count: " << ar::query<Zone>().count() << std::endl;
/*
	for(const auto& iter : ar::query<Zone>().select("zoneid, zonename, zonetype").order("zonename").all()) {
		std::cout << iter["zoneid"] << "\t" << iter["zonename"] << "\t" << iter["zonetype"] <<std::endl;
	}
*/
#endif

}

/*


// std::cout << std::to_string( iter.attributes() ) << std::endl;

Zones zones;
zones.find(5);

ar::sql_builder builder(zones);
std::string query = builder.select("a,b,c").order("c").to_string();

*/
