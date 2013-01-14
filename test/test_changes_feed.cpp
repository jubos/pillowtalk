#include <iostream>
#include <string>
#include <vector>

#include "pillowtalk.h"
using namespace std;

int gNumberOfHeartbeats = 0;

int callback_non_cont(pt_node_t* node)
{
  string document = "http://127.0.0.1:5984/pt_test/";
  pt_printout(node, " ");
  cout << endl;
  return 0;
}

int callback(pt_node_t* node)
{
  if (pt_is_null(node)) {
    gNumberOfHeartbeats++;
    cout << "Beat" << endl;
    if (gNumberOfHeartbeats == 10) {
      cout << "Number of heartbeats seen: " << gNumberOfHeartbeats << endl;    
      cout << "Ending ..." << endl;
      return -1;
    }
    return 0;
  }
  string document = "http://127.0.0.1:5984/pt_test/";
  pt_printout(node, " ");
  const char* astr = pt_string_get(pt_map_get(node, "id"));
  if (astr) document += astr; 
  pt_response_t* temp = pt_get(document.c_str());
  pt_printout(temp->root, " ");
  cout << endl;
  pt_free_response(temp);
  return 0;
}

int main()
{
  pt_init();
  pt_changes_feed cf = pt_changes_feed_alloc();
  
  /*
  pt_changes_feed_config(cf, pt_changes_feed_continuous, 0);
  pt_changes_feed_config(cf, pt_changes_feed_callback_function, &callback_non_cont);
  pt_changes_feed_run(cf, "http://127.0.0.1:5984","pt_test");
  */

  pt_changes_feed_config(cf, pt_changes_feed_continuous, 1);
  pt_changes_feed_config(cf, pt_changes_feed_req_heartbeats, 1000);
  pt_changes_feed_config(cf, pt_changes_feed_callback_function, &callback);
  pt_changes_feed_run(cf, "http://127.0.0.1:5984","pt_test");

  pt_changes_feed_free(cf);
  pt_cleanup();
  return 0;
}


