#include <iostream>
#include <string>
#include <vector>

#define BOOST_TEST_MAIN
#include <boost/test/included/unit_test.hpp> 
#include "pillowtalk.h"

using namespace std;
using namespace boost::unit_test;

struct InitFixture {
  InitFixture() {
    pillowtalk_init();
    pt_response_t* res; 

    res = pillowtalk_delete("http://localhost:5984/pillowtalk_test");
    if (res->response_code == 500) {
      pillowtalk_free_response(res);
      cout << "Please ensure that couchdb is running on localhost" << endl;
      exit(-1);
    }
    pillowtalk_free_response(res);

    res = pillowtalk_put("http://localhost:5984/pillowtalk_test",NULL,0);
    pillowtalk_free_response(res);

    const char* basic = "{}";
    res = pillowtalk_put("http://localhost:5984/pillowtalk_test/basic",basic,strlen(basic));
    pillowtalk_free_response(res);

    const char* array = "{\"a\":[1,2,3]}";
    res = pillowtalk_put("http://localhost:5984/pillowtalk_test/array",array,strlen(array));
    pillowtalk_free_response(res);
  }


  ~InitFixture() {
    pillowtalk_cleanup();
  }
};

//BOOST_FIXTURE_TEST_SUITE(s, InitFixture);
BOOST_GLOBAL_FIXTURE(InitFixture);

BOOST_AUTO_TEST_CASE( test_basic )
{
  pt_response_t* res = pillowtalk_get("http://localhost:5984/pillowtalk_test/basic");
  BOOST_REQUIRE(res);
  BOOST_REQUIRE(res->root);
  BOOST_REQUIRE(res->root->type == PT_MAP);
  pt_node_t* id = pillowtalk_map_get(res->root,"_id");
  BOOST_REQUIRE(id);
  BOOST_REQUIRE(id->type == PT_STRING);
  const char* val = pillowtalk_string_get(id);
  BOOST_REQUIRE_EQUAL(val,"basic");

  pillowtalk_free_response(res);
}

BOOST_AUTO_TEST_CASE( test_array )
{
  pt_response_t* res = pillowtalk_get("http://localhost:5984/pillowtalk_test/array");
  BOOST_REQUIRE(res->root);
  BOOST_REQUIRE(res->root->type == PT_MAP);

  pt_node_t* array = pillowtalk_map_get(res->root,"a");
  BOOST_REQUIRE(array);
  BOOST_REQUIRE(array->type == PT_ARRAY);
  unsigned int len = pillowtalk_array_len(array);
  BOOST_REQUIRE_EQUAL(len,3);
  pillowtalk_free_response(res);
}

//BOOST_AUTO_TEST_SUITE_END()
