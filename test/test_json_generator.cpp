#include <iostream>
#include <string>
#include <vector>

#define BOOST_TEST_MAIN
#include <boost/test/included/unit_test.hpp> 
#include "pillowtalk.h"

using namespace std;
using namespace boost::unit_test;

BOOST_AUTO_TEST_CASE(test_basic_map_generation)
{
  pt_node_t* map = pillowtalk_map_new();
  pt_node_t* string = pillowtalk_string_new("world");
  pillowtalk_map_set(map,"hello",string);
  char* json_str = pillowtalk_to_json(map,0);

  BOOST_REQUIRE_EQUAL(json_str,"{\"hello\":\"world\"}");
  free(json_str);
  pillowtalk_free_node(map);
}

BOOST_AUTO_TEST_CASE(test_basic_array_generation)
{
  pt_node_t* array = pillowtalk_array_new();
  pt_node_t* integer = pillowtalk_integer_new(99);
  pt_node_t* dbl = pillowtalk_double_new(5.5);
  pt_node_t* string = pillowtalk_string_new("string");
  pt_node_t* map = pillowtalk_map_new();
  pt_node_t* world = pillowtalk_string_new("world");
  pillowtalk_map_set(map,"hello",world);

  pillowtalk_array_push_back(array,integer);
  pillowtalk_array_push_back(array,dbl);
  pillowtalk_array_push_back(array,string);
  pillowtalk_array_push_back(array,map);

  char* json_str = pillowtalk_to_json(array,0);
  BOOST_REQUIRE_EQUAL(json_str,"[99,5.5,\"string\",{\"hello\":\"world\"}]");

  free(json_str);
  pillowtalk_free_node(array);
}
