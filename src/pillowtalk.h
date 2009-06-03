//
// Copyright (c) 2009, Curtis Spencer. All rights reserved.
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU Library General Public License. You should
// have received a copy of the LGPL license along with this program; if you
// did not, you can find it at http://www.gnu.org/
//
#ifndef __PILLOWTALK__H_
#define __PILLOWTALK__H_

#ifdef	__cplusplus
extern "C" {
#endif

typedef enum {PT_MAP,PT_ARRAY,PT_NULL, PT_BOOLEAN, PT_INTEGER, PT_DOUBLE, PT_STRING, PT_KEY_VALUE} pt_type_t;

typedef struct {
  pt_type_t type;
} pt_node_t;

typedef struct {
  pt_node_t* root;
  int error_code;
} pt_response_t;

void pillowtalk_init();
void pillowtalk_cleanup();

void pillowtalk_free_response(pt_response_t* res);

/***** HTTP Related Functions ******/

pt_response_t* pillowtalk_delete(const char* server_target);

pt_response_t* pillowtalk_put(const char* server_target, const char* data, unsigned int data_len);

/* 
 * Do an HTTP get request on the target and parse the resulting JSON into the pt_response object
 */
pt_response_t* pillowtalk_get(const char* server_target);

/***** Node Related Functions ******/

/*
 * Once you have a node, you can call various functions on it, and most will
 * return NULL if you do it on the wrong type.  Check the type attribute of the
 * pt_node_t to ensure you are doing the correct operation.
 */
pt_node_t* pillowtalk_map_get(pt_node_t* map,const char* key);

unsigned int pillowtalk_array_len(pt_node_t* array);
pt_node_t* pillowtalk_array_get(pt_node_t* array, unsigned int idx);

int pillowtalk_is_null(pt_node_t* null);
int pillowtalk_boolean_get(pt_node_t* boolean);
int pillowtalk_integer_get(pt_node_t* integer);
double pillowtalk_double_get(pt_node_t* dbl);
const char* pillowtalk_string_get(pt_node_t* string);


#ifdef	__cplusplus
}
#endif

#endif // _PILLOWTALK_H_
