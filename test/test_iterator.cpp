#include <iostream>
#include <string>
#include <vector>

#define BOOST_TEST_MAIN
#include <boost/test/included/unit_test.hpp> 
#include "pillowtalk.h"

using namespace std;
using namespace boost::unit_test;

BOOST_AUTO_TEST_CASE( test_iterator )
{
  pt_node_t* array = pillowtalk_array_new();
  pillowtalk_array_push_back(array,pillowtalk_string_new("1"));
  pillowtalk_array_push_back(array,pillowtalk_string_new("2"));
  pillowtalk_array_push_back(array,pillowtalk_string_new("3"));

  pt_iterator_t* iter = pillowtalk_iterator(array);
  int index = 0;
  while(pt_node_t* elem = pillowtalk_iterator_next(iter,NULL)) {
    switch(index) {
      case 0:
        BOOST_REQUIRE_EQUAL(pillowtalk_string_get(elem),"1");
        break;
      case 1:
        BOOST_REQUIRE_EQUAL(pillowtalk_string_get(elem),"2");
        break;
      case 2:
        BOOST_REQUIRE_EQUAL(pillowtalk_string_get(elem),"3");
        break;
    }
    index++;
  }
  free(iter);
  pillowtalk_free_node(array);
}

BOOST_AUTO_TEST_CASE( test_blank_iterator)
{
  pt_node_t* array = pillowtalk_array_new();

  pt_iterator_t* iter = pillowtalk_iterator(array);
  pt_node_t* elem = pillowtalk_iterator_next(iter,NULL);
  BOOST_REQUIRE(!elem);
  free(iter);
  pillowtalk_free_node(array);
}

BOOST_AUTO_TEST_CASE( test_map_iterator)
{
  pt_node_t* map = pillowtalk_map_new();
  pillowtalk_map_set(map,"1",pillowtalk_integer_new(1));
  pillowtalk_map_set(map,"2",pillowtalk_integer_new(2));
  pillowtalk_map_set(map,"3",pillowtalk_integer_new(3));

  pt_iterator_t* iter = pillowtalk_iterator(map);
  pt_node_t* elem;
  const char* key = NULL;
  int index = 0;
  while(pt_node_t* elem = pillowtalk_iterator_next(iter,&key)) {
    switch(index) {
      case 0:
        BOOST_REQUIRE_EQUAL(pillowtalk_integer_get(elem),1);
        BOOST_REQUIRE_EQUAL(key,"1");
        break;
      case 1:
        BOOST_REQUIRE_EQUAL(pillowtalk_integer_get(elem),2);
        BOOST_REQUIRE_EQUAL(key,"2");
        break;
      case 2:
        BOOST_REQUIRE_EQUAL(pillowtalk_integer_get(elem),3);
        BOOST_REQUIRE_EQUAL(key,"3");
        break;
    }
    index++;
  }
  elem = pillowtalk_iterator_next(iter,NULL);
  BOOST_REQUIRE(!elem);
  free(iter);
  pillowtalk_free_node(map);
}

