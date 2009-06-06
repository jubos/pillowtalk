#include <iostream>
#include <string>
#include <vector>
#include <sstream>

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

BOOST_AUTO_TEST_CASE( test_memory_leak )
{
  pt_node_t* root = pillowtalk_from_json("{\"test\":{hello:\"world\"}}");
  BOOST_REQUIRE(pillowtalk_map_get(root,"test"));
  pillowtalk_free_node(root);
}

BOOST_AUTO_TEST_CASE( test_bad_json )
{
  pt_node_t* root = pillowtalk_from_json("{}}");
  pillowtalk_free_node(root);
}
