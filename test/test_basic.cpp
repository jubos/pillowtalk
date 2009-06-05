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

    res = pillowtalk_put_raw("http://localhost:5984/pillowtalk_test",NULL,0);
    pillowtalk_free_response(res);

    const char* basic = "{}";
    res = pillowtalk_put_raw("http://localhost:5984/pillowtalk_test/basic",basic,strlen(basic));
    pillowtalk_free_response(res);

    const char* array = "{\"a\":[1,2,3]}";
    res = pillowtalk_put_raw("http://localhost:5984/pillowtalk_test/array",array,strlen(array));
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


BOOST_AUTO_TEST_CASE( test_updates_to_document )
{
  pt_node_t* new_doc = pillowtalk_map_new();
  pt_node_t* id = pillowtalk_string_new("mynewdoc");
  pillowtalk_map_set(new_doc,"_id",id);

  pt_node_t* name = pillowtalk_string_new("jubos");
  pillowtalk_map_set(new_doc,"name",name);
  pillowtalk_map_set(new_doc,"updates",pillowtalk_array_new());

  pt_response_t* res = pillowtalk_put("http://localhost:5984/pillowtalk_test/mynewdoc",new_doc);
  BOOST_REQUIRE(pillowtalk_map_get(res->root,"rev") != NULL);
  pillowtalk_free_response(res);
  pillowtalk_free_node(new_doc);
  for(int i=0; i < 5; i++) {
    res = pillowtalk_get("http://localhost:5984/pillowtalk_test/mynewdoc");
    BOOST_REQUIRE(res->response_code == 200);
    pillowtalk_array_push_back(pillowtalk_map_get(res->root,"updates"),pillowtalk_integer_new(i));
    pt_response_t* put_res = pillowtalk_put("http://localhost:5984/pillowtalk_test/mynewdoc",res->root);
    BOOST_REQUIRE(put_res->response_code >= 200 && put_res->response_code < 300);
    pillowtalk_free_response(res);
    pillowtalk_free_response(put_res);
  }
}

// Here we make a new set of json and make sure we get what we expect
BOOST_AUTO_TEST_CASE( test_mutable_json )
{
  pt_node_t* map = pillowtalk_map_new();
  BOOST_REQUIRE(map);
  BOOST_REQUIRE(map->type == PT_MAP);

  pt_node_t* null = pillowtalk_null_new();
  BOOST_REQUIRE(null);
  BOOST_REQUIRE(null->type == PT_NULL);

  pt_node_t* boolean = pillowtalk_bool_new(1);
  BOOST_REQUIRE(boolean);
  BOOST_REQUIRE(boolean->type == PT_BOOLEAN);

  pt_node_t* integer = pillowtalk_integer_new(100);
  BOOST_REQUIRE(integer);
  BOOST_REQUIRE(integer->type == PT_INTEGER);

  pt_node_t* dbl = pillowtalk_double_new(9.99);
  BOOST_REQUIRE(dbl);
  BOOST_REQUIRE(dbl->type == PT_DOUBLE);

  pt_node_t* world = pillowtalk_string_new("string");
  BOOST_REQUIRE(world);
  BOOST_REQUIRE(world->type == PT_STRING);

  pt_node_t* ary = pillowtalk_array_new();
  BOOST_REQUIRE(ary);
  BOOST_REQUIRE(ary->type == PT_ARRAY);

  pt_node_t* int1 = pillowtalk_integer_new(1);
  pt_node_t* int2 = pillowtalk_integer_new(2);
  pt_node_t* int3 = pillowtalk_integer_new(3);
  pillowtalk_array_push_back(ary,int1);
  pillowtalk_array_push_back(ary,int2);
  pillowtalk_array_push_back(ary,int3);

  BOOST_REQUIRE_EQUAL(pillowtalk_array_len(ary),3);

  pillowtalk_array_remove(ary,int2);

  BOOST_REQUIRE_EQUAL(pillowtalk_array_len(ary),2);


  // Now set these things into the map
  
  pillowtalk_map_set(map,"null",null);
  pillowtalk_map_set(map,"boolean",boolean);
  pillowtalk_map_set(map,"integer",integer);
  pillowtalk_map_set(map,"double",dbl);
  pillowtalk_map_set(map,"string",world);
  pillowtalk_map_set(map,"array",ary);

  pt_node_t* value = pillowtalk_map_get(map,"null");
  BOOST_REQUIRE(value->type == PT_NULL);

  value = pillowtalk_map_get(map,"boolean");
  BOOST_REQUIRE(pillowtalk_boolean_get(value) == 1);

  value = pillowtalk_map_get(map,"integer");
  BOOST_REQUIRE(pillowtalk_integer_get(value) == 100);

  value = pillowtalk_map_get(map,"double");
  BOOST_REQUIRE(pillowtalk_double_get(value) == 9.99);

  value = pillowtalk_map_get(map,"string");
  BOOST_REQUIRE(value);
  BOOST_REQUIRE_EQUAL("string",pillowtalk_string_get(value));

  value = pillowtalk_map_get(map,"array");
  BOOST_REQUIRE(value);


  pillowtalk_map_unset(map,"string");
  BOOST_REQUIRE(!pillowtalk_map_get(map,"string"));

  pillowtalk_free_node(map);
}

//BOOST_AUTO_TEST_SUITE_END()
