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
  stringstream content;
  string line;
  ifstream myfile(filename.c_str());
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
  char* json_str = pillowtalk_to_json(NULL,0);
  BOOST_REQUIRE_EQUAL(json_str,"null");
  free(json_str);
}

BOOST_AUTO_TEST_CASE(test_null_map_update)
{
  pt_node_t* json = pillowtalk_from_json("{}");
  int ret_code = pillowtalk_map_update(NULL,json,0);
  BOOST_REQUIRE_EQUAL(ret_code,1);
  pillowtalk_free_node(json);
}

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

BOOST_AUTO_TEST_CASE(test_clone)
{
  string test_files_dir = string(boost::unit_test::framework::master_test_suite().argv[1]);
  char* star_wars = strdup(read_file(test_files_dir + "/fixtures/star_wars.json").c_str());

  pt_node_t* star_wars_pt = pillowtalk_from_json(star_wars);
  pt_node_t* clone = pillowtalk_clone(star_wars_pt);

  char* star_wars_pt_str = pillowtalk_to_json(star_wars_pt,0);
  char* clone_str = pillowtalk_to_json(star_wars_pt,0);

  BOOST_REQUIRE_EQUAL(star_wars_pt_str,clone_str);

  free(star_wars);
  free(star_wars_pt_str);
  free(clone_str);

  pillowtalk_free_node(star_wars_pt);
  pillowtalk_free_node(clone);
}

BOOST_AUTO_TEST_CASE(update_map)
{
  size_t size;
  printf("%d\n",boost::unit_test::framework::master_test_suite().argc);
  printf("%s\n",boost::unit_test::framework::master_test_suite().argv[0]);
  printf("%s\n",boost::unit_test::framework::master_test_suite().argv[1]);

  string test_files_dir = string(boost::unit_test::framework::master_test_suite().argv[1]);
  char* star_wars = strdup(read_file(test_files_dir + "/fixtures/star_wars.json").c_str());
  char* star_wars_additions = strdup(read_file(test_files_dir + "/fixtures/star_wars_append.json").c_str());
  char* star_wars_merged = strdup(read_file(test_files_dir + "/fixtures/star_wars_merged.json").c_str());

  pt_node_t* star_wars_pt = pillowtalk_from_json(star_wars);
  pt_node_t* star_wars_additions_pt = pillowtalk_from_json(star_wars_additions);
  pt_node_t* star_wars_merged_pt = pillowtalk_from_json(star_wars_merged);

  free(star_wars);
  free(star_wars_additions);
  free(star_wars_merged);

  pillowtalk_map_update(star_wars_pt,star_wars_additions_pt, 1);
  pillowtalk_free_node(star_wars_additions_pt);

  char* computed_json = pillowtalk_to_json(star_wars_pt,0);
  char* merged_json = pillowtalk_to_json(star_wars_merged_pt,0);
  BOOST_REQUIRE_EQUAL(computed_json,merged_json);

  pillowtalk_free_node(star_wars_pt);
  pillowtalk_free_node(star_wars_merged_pt);
  free(computed_json);
  free(merged_json);
}
