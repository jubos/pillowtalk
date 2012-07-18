//
// Copyright (c) 2009, Curtis Spencer. All rights reserved.
//
#ifndef __PILLOWTALK__H_
#define __PILLOWTALK__H_

/* msft dll export gunk.  To build a DLL on windows, you
 * must define WIN32, PT_SHARED, and PT_BUILD.  To use a shared
 * DLL, you must define PT_SHARED and WIN32 */
#if defined(WIN32) && defined(PT_SHARED)
#  ifdef PT_BUILD
#    define PT_API __declspec(dllexport)
#  else
#    define PT_API __declspec(dllimport)
#  endif
#else
#  define PT_API
#endif 


#ifdef	__cplusplus
extern "C" {
#endif

typedef enum {PT_MAP,PT_ARRAY,PT_NULL, PT_BOOLEAN, PT_INTEGER, PT_DOUBLE, PT_STRING, PT_KEY_VALUE} pt_type_t;

typedef struct {
  pt_type_t type;
} pt_node_t;

typedef struct {
  pt_node_t* root;
  long response_code;
  char* raw_json;
  int raw_json_len;
} pt_response_t;

// Opaque type for iterator
typedef struct {
} pt_iterator_t;

PT_API void pt_init();
PT_API void pt_cleanup();

PT_API void pt_free_node(pt_node_t* node);
PT_API void pt_free_response(pt_response_t* res);

/***** HTTP Related Functions ******/

PT_API pt_response_t* pt_delete(const char* server_target);

PT_API pt_response_t* pt_put(const char* server_target, pt_node_t* document);
PT_API pt_response_t* pt_put_raw(const char* server_target, const char* data, unsigned int data_len);

/* 
 * Do an HTTP get request on the target and parse the resulting JSON into the
 * pt_response object
 */
PT_API pt_response_t* pt_get(const char* server_target);


/*
 * This will just do a get against the server target and not try to parse it at all.
 * It is useful for doing your own parsing with the resultant JSON 
 */
PT_API pt_response_t* pt_unparsed_get(const char* server_target);

/***** Node Related Functions ******/

/*
 * Once you have a node, you can call various functions on it, and most will
 * return NULL if you do it on the wrong type.  Check the type attribute of the
 * pt_node_t to ensure you are doing the correct operation.
 */
PT_API pt_node_t* pt_map_get(pt_node_t* map,const char* key);

PT_API unsigned int pt_array_len(pt_node_t* array);
PT_API pt_node_t* pt_array_get(pt_node_t* array, unsigned int idx);

PT_API int pt_is_null(pt_node_t* null);
PT_API int pt_boolean_get(pt_node_t* boolean);
PT_API int pt_integer_get(pt_node_t* integer);
PT_API double pt_double_get(pt_node_t* dbl);
PT_API const char* pt_string_get(pt_node_t* string);

/* 
 * The following functions are used to change a pt_node_t to do update
 * operations or get new json strings 
 */

PT_API void pt_map_set(pt_node_t* map, const char* key, pt_node_t* value);
PT_API void pt_map_unset(pt_node_t* map, const char* key);

PT_API pt_node_t* pt_null_new();
PT_API pt_node_t* pt_bool_new(int boolean);
PT_API pt_node_t* pt_integer_new(int integer);
PT_API pt_node_t* pt_double_new(double dbl);
PT_API pt_node_t* pt_string_new(const char* str);
PT_API pt_node_t* pt_map_new();
PT_API pt_node_t* pt_array_new();

PT_API void pt_array_push_back(pt_node_t* array, pt_node_t* elem);
PT_API void pt_array_push_front(pt_node_t* array, pt_node_t* elem);

/*
 * This will remove elem if it exists in the array and free it as well, so
 * don't use elem after this
 */
PT_API void pt_array_remove(pt_node_t* array, pt_node_t* elem);

/* 
 * Build an iterator from an array/map node.  If you pass in an unsupported
 * node it will return NULL
 */
PT_API pt_iterator_t* pt_iterator(pt_node_t* node);

/*
 * This returns the next node in the iterator back and NULL when complete.
 *
 * The key char** is also set to the key of a key value pair if you are
 * iterating through a map
 *
 */
PT_API pt_node_t* pt_iterator_next(pt_iterator_t* iter, const char** key);


/*
 * Convert a pt_node_t structure into a raw json string
 */
PT_API char* pt_to_json(pt_node_t* root, int beautify);

/* 
 * Take a raw json string and turn it into a pillowtalk structure
 */
PT_API pt_node_t* pt_from_json(const char* json);

/*
 * Merge additions into an existing pt_node
 *
 * For example if your root looks like this
 * {
 *    "name" : "Curtis",
 *    "favorite_food" : "Bread"
 * }
 *
 * and your additions look like this
 *
 * {
 *    "favorite_game" : "Street Fighter II"
 * }
 *
 * then the resulting json in root would be
 *
 * {
 *    "name" : "Curtis",
 *    "favorite_food" : "Bread",
 *    "favorite_game" : "Street Fighter II"
 * }
 *
 * @return a nonzero error code if something cannot properly be merged.  For
 * example, if a key in the root is an array and the additions has it as a hash
 * then it will give up there, but it won't rollback so be careful.
 */    
PT_API int pt_map_update(pt_node_t* root, pt_node_t* additions,int append);

/*
 * This method is useful if you want to clone a root you are working on to make
 * changes to it
 */
PT_API pt_node_t* pt_clone(pt_node_t* root);

/*
 * Print out a node, useful for debugging 
 */
PT_API void pt_printout(pt_node_t* root, const char* indent);

/*
 * The following is for implementation of the changes feed
 */

/** Handler to the changes feed. */
typedef struct pt_changes_feed_t * pt_changes_feed;
typedef enum {
    /** Keep the changes feed open, default NO*/
    pt_changes_feed_continuous = 0x1, 
    /** Request heartbeats from the server every N ms.  0 (default) means no heartbeats */
    pt_changes_feed_req_heartbeats = 0x2,
    /** set changes feed callback function */
    pt_changes_feed_callback_function = 1000,
    /** set changes feed generic options, 
      * string gets appends without checking 
      * to the url (const char*) */
    pt_changes_feed_generic_opts = 2000,
  } pt_changes_feed_option;

/** Function type for performing a call back on a change line from the server
 * (in continuous mode) or the entire JSON object (in non-continuous mode). 
 */
typedef int (*pt_changes_callback_func)(pt_node_t* line_change);

/** Allocate a changes feed handle. */
PT_API pt_changes_feed pt_changes_feed_alloc();

/** Set configuration options for a changes feed handle. */ 
PT_API int pt_changes_feed_config(pt_changes_feed handle, pt_changes_feed_option opt, ...);

/** Run the changes feed will start the changes feed,
  * this will block until the changes feed ends. */
PT_API void pt_changes_feed_run(pt_changes_feed handle,
  const char* server_name,
  const char* database); 

/** Destroy the changes feed handle. */
PT_API void pt_changes_feed_free(pt_changes_feed handle);


#ifdef	__cplusplus
}
#endif

#endif // _PILLOWTALK_H_
