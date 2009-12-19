#include <iostream>
#include <string>
#include <vector>

#define BOOST_TEST_MAIN
#include <boost/test/included/unit_test.hpp> 
#include "pillowtalk.h"

using namespace std;
using namespace boost::unit_test;

static string 
read_file(const string& filename)
{
  string test_files_dir = string(boost::unit_test::framework::master_test_suite().argv[1]);
  stringstream content;
  string line;
  ifstream myfile((test_files_dir + "/" + filename).c_str());
  if (myfile.is_open())
  {
    while (! myfile.eof() )
    {
      getline (myfile,line);
      content << line << endl;
    }
    myfile.close();
  } else {  
    cout << "Unable to open file" << endl;
    exit(-1);
  }
  return content.str();
}

BOOST_AUTO_TEST_CASE(test_null_json)
{
  char* json_str = pt_to_json(NULL,0);
  BOOST_REQUIRE_EQUAL(json_str,"null");
  free(json_str);
}

BOOST_AUTO_TEST_CASE(test_null_map_update)
{
  pt_node_t* json = pt_from_json("{}");
  int ret_code = pt_map_update(NULL,json,0);
  BOOST_REQUIRE_EQUAL(ret_code,1);
  pt_free_node(json);
}

BOOST_AUTO_TEST_CASE(test_basic_map_generation)
{
  pt_node_t* map = pt_map_new();
  pt_node_t* string = pt_string_new("world");
  pt_map_set(map,"hello",string);
  char* json_str = pt_to_json(map,0);

  BOOST_REQUIRE_EQUAL(json_str,"{\"hello\":\"world\"}");
  free(json_str);
  pt_free_node(map);
}

BOOST_AUTO_TEST_CASE(test_basic_array_generation)
{
  pt_node_t* array = pt_array_new();
  pt_node_t* integer = pt_integer_new(99);
  pt_node_t* dbl = pt_double_new(5.5);
  pt_node_t* string = pt_string_new("string");
  pt_node_t* map = pt_map_new();
  pt_node_t* world = pt_string_new("world");
  pt_map_set(map,"hello",world);

  pt_array_push_back(array,integer);
  pt_array_push_back(array,dbl);
  pt_array_push_back(array,string);
  pt_array_push_back(array,map);

  char* json_str = pt_to_json(array,0);
  BOOST_REQUIRE_EQUAL(json_str,"[99,5.5,\"string\",{\"hello\":\"world\"}]");

  free(json_str);
  pt_free_node(array);
}

BOOST_AUTO_TEST_CASE(test_clone)
{
  char* star_wars = strdup(read_file("/fixtures/star_wars.json").c_str());

  pt_node_t* star_wars_pt = pt_from_json(star_wars);
  pt_node_t* clone = pt_clone(star_wars_pt);

  char* star_wars_pt_str = pt_to_json(star_wars_pt,0);
  char* clone_str = pt_to_json(star_wars_pt,0);

  BOOST_REQUIRE_EQUAL(star_wars_pt_str,clone_str);

  free(star_wars);
  free(star_wars_pt_str);
  free(clone_str);

  pt_free_node(star_wars_pt);
  pt_free_node(clone);
}

BOOST_AUTO_TEST_CASE(update_map)
{
  char* star_wars = strdup(read_file("/fixtures/star_wars.json").c_str());
  char* star_wars_additions = strdup(read_file("/fixtures/star_wars_append.json").c_str());
  char* star_wars_merged = strdup(read_file("/fixtures/star_wars_merged.json").c_str());

  pt_node_t* star_wars_pt = pt_from_json(star_wars);
  pt_node_t* star_wars_additions_pt = pt_from_json(star_wars_additions);
  pt_node_t* star_wars_merged_pt = pt_from_json(star_wars_merged);

  free(star_wars);
  free(star_wars_additions);
  free(star_wars_merged);

  pt_map_update(star_wars_pt,star_wars_additions_pt, 1);
  pt_free_node(star_wars_additions_pt);

  char* computed_json = pt_to_json(star_wars_pt,0);
  char* merged_json = pt_to_json(star_wars_merged_pt,0);
  BOOST_REQUIRE_EQUAL(computed_json,merged_json);

  pt_free_node(star_wars_pt);
  pt_free_node(star_wars_merged_pt);
  free(computed_json);
  free(merged_json);
}
