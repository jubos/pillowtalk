/*
 * This file will just use the pillowtalk functions to build up some JSON
 * for a star wars document of the following JSON:
 *
 *
 *   {
 *     "_id": "star_wars",
 *     "movies" : {
 *       "Star Wars Episode IV": {
 *         "characters": ["Luke Skywalker","Han Solo","Obi Wan Kenobi"]
 *       }
 *     }
 *   }
 *
 *   Then it should save that document in the pillowtalk_basics database at
 *   localhost, and then retrieve and add a year key and add "Princess Leia" to
 *   the characters list and save back.
 *
 */
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

#include "pillowtalk.h"

int main(int argc,char** argv) 
{
  pt_init();
  pt_node_t* root = pt_map_new();
  pt_map_set(root,"_id",pt_string_new("star_wars"));

  pt_node_t* movies = pt_map_new();
  pt_map_set(root,"movies",movies);

  // build movie subdocument
  pt_node_t* ep4 = pt_map_new();

  // build characters array
  pt_node_t* ep4_chars = pt_array_new();
  pt_array_push_back(ep4_chars,pt_string_new("Luke Skywalker"));
  pt_array_push_back(ep4_chars,pt_string_new("Han Solo"));
  pt_array_push_back(ep4_chars,pt_string_new("Obi Wan Kenobi"));

  pt_map_set(ep4,"characters", ep4_chars);
  
  pt_map_set(movies,"Star Wars Episode IV",ep4);

  pt_response_t* response = NULL;
  response = pt_delete("http://localhost:5984/pillowtalk_basics");
  pt_free_response(response);
  response = pt_put("http://localhost:5984/pillowtalk_basics",NULL);
  pt_free_response(response);
  response = pt_put("http://localhost:5984/pillowtalk_basics/star_wars",root);
  assert(response->response_code == 201);
  pt_free_response(response);

  pt_free_node(root);

  response = pt_get("http://localhost:5984/pillowtalk_basics/star_wars");
  assert(response->response_code == 200);

  pt_node_t* doc = response->root;
  const char* id = pt_string_get(pt_map_get(doc,"_id"));
  assert(!strcmp(id,"star_wars"));

  pt_node_t* ep4_node = pt_map_get(pt_map_get(doc,"movies"),"Star Wars Episode IV");
  pt_node_t* characters_node = pt_map_get(ep4_node,"characters");
  int array_len = pt_array_len(characters_node);
  assert(array_len == 3);

  pt_map_set(ep4_node,"year",pt_string_new("1977"));
  pt_array_push_back(characters_node,pt_string_new("Princess Leia"));
  pt_response_t* put_response = pt_put("http://localhost:5984/pillowtalk_basics/star_wars", doc);

  pt_free_response(response);
  pt_free_response(put_response);

  pt_cleanup();
}
