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
  pt_node_t* array = pt_array_new();
  pt_array_push_back(array,pt_string_new("1"));
  pt_array_push_back(array,pt_string_new("2"));
  pt_array_push_back(array,pt_string_new("3"));

  pt_iterator_t* iter = pt_iterator(array);
  int index = 0;
  while(pt_node_t* elem = pt_iterator_next(iter,NULL)) {
    switch(index) {
      case 0:
        BOOST_REQUIRE_EQUAL(pt_string_get(elem),"1");
        break;
      case 1:
        BOOST_REQUIRE_EQUAL(pt_string_get(elem),"2");
        break;
      case 2:
        BOOST_REQUIRE_EQUAL(pt_string_get(elem),"3");
        break;
    }
    index++;
  }
  free(iter);
  pt_free_node(array);
}

BOOST_AUTO_TEST_CASE( test_blank_iterator)
{
  pt_node_t* array = pt_array_new();

  pt_iterator_t* iter = pt_iterator(array);
  pt_node_t* elem = pt_iterator_next(iter,NULL);
  BOOST_REQUIRE(!elem);
  free(iter);
  pt_free_node(array);
}

BOOST_AUTO_TEST_CASE( test_map_iterator)
{
  pt_node_t* map = pt_map_new();
  pt_map_set(map,"1",pt_integer_new(1));
  pt_map_set(map,"2",pt_integer_new(2));
  pt_map_set(map,"3",pt_integer_new(3));

  pt_iterator_t* iter = pt_iterator(map);
  pt_node_t* elem;
  const char* key = NULL;
  int index = 0;
  while(pt_node_t* elem = pt_iterator_next(iter,&key)) {
    switch(index) {
      case 0:
        BOOST_REQUIRE_EQUAL(pt_integer_get(elem),1);
        BOOST_REQUIRE_EQUAL(key,"1");
        break;
      case 1:
        BOOST_REQUIRE_EQUAL(pt_integer_get(elem),2);
        BOOST_REQUIRE_EQUAL(key,"2");
        break;
      case 2:
        BOOST_REQUIRE_EQUAL(pt_integer_get(elem),3);
        BOOST_REQUIRE_EQUAL(key,"3");
        break;
    }
    index++;
  }
  elem = pt_iterator_next(iter,NULL);
  BOOST_REQUIRE(!elem);
  free(iter);
  pt_free_node(map);
}

