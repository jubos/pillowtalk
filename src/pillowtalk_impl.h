#include "pillowtalk.h"
#include "uthash.h"
#include "utlist.h"

/* Here we have subclasses of pt_node */

struct pt_map_t; 

typedef struct {
  pt_node_t parent;
  char* key;
  pt_node_t* value;
  struct pt_map_t* map;
  UT_hash_handle hh;
} pt_key_value_t;

typedef struct {
  pt_node_t parent;
  pt_key_value_t* key_values;
} pt_map_t;

typedef struct {
  pt_node_t parent;
  pt_node_t** array;
  unsigned int len;
} pt_array_t;

typedef struct {
  pt_node_t parent;
  int value;
} pt_null_value_t;

typedef struct {
  pt_node_t parent;
  int value;
} pt_bool_value_t;

typedef struct {
  pt_node_t parent;
  int value;
} pt_int_value_t;

typedef struct {
  pt_node_t parent;
  double value;
} pt_double_value_t;

typedef struct {
  pt_node_t parent;
  char* value;
} pt_str_value_t;

/* This is useful for a stack of containers so we can know where we are */
typedef struct pt_container_ctx_t {
  pt_node_t* container;
  struct pt_container_ctx_t *next;//, *prev;
}pt_container_ctx_t;

/* Implementation Structure of pt_response_t */
typedef struct {
  pt_response_t core;
  pt_node_t* current_node;
  char* current_key;
  pt_container_ctx_t* stack;
} pt_response_impl_t;

