#include "pillowtalk_impl.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <curl/curl.h>
#include <curl/types.h>
#include <curl/easy.h>

#include <yajl/yajl_gen.h>
#include <yajl/yajl_parse.h>
#include <assert.h>

#include "bsd_queue.h"

/* Structs */
struct memory_chunk {
  char *memory;
  char *offset;
  size_t size;
};

/* Prototypes */
static pt_response_t* http_operation(const char* method,const char* server_target, const char* data, unsigned data_len);
static void *myrealloc(void *ptr, size_t size);
static size_t recv_memory_callback(void *ptr, size_t size, size_t nmemb, void *data);
static size_t send_memory_callback(void *ptr, size_t size, size_t nmemb, void *data);
static int json_null(void* response);
static int json_boolean(void* response,int boolean);
static int json_map_key(void * response, const unsigned char* str, unsigned int length);
static int json_integer(void* response,long integer);
static int json_double(void* response,double dbl);
static int json_string(void* response, const unsigned char* str, unsigned int length);
static int json_start_map(void* response);
static int json_end_map(void* response);
static int json_start_array(void* response);
static int json_end_array(void* response);
static void generate_map_json(pt_map_t* map, yajl_gen g);
static void generate_array_json(pt_array_t* map , yajl_gen g);
static void generate_node_json(pt_node_t* node, yajl_gen g);
static void free_map_node(pt_map_t* map);
static void add_node_to_context_container(pt_response_impl_t* context, pt_node_t* value);
static void parse_json(struct memory_chunk* chunk,pt_response_t*);

/* Globals */
static yajl_callbacks callbacks = {
  json_null, // null
  json_boolean, // boolean
  json_integer, // integer
  json_double, // double
  NULL, // number_string
  json_string, // string
  json_start_map, // start map
  json_map_key, // MAP KEY
  json_end_map, // end map
  json_start_array, // start array
  json_end_array, // end array
};


/* Public Implementation */

void pillowtalk_init()
{
  curl_global_init(CURL_GLOBAL_ALL);
}

void pillowtalk_cleanup()
{
  /* we're done with libcurl, so clean it up */
  curl_global_cleanup();
}

void pillowtalk_free_response(pt_response_t* response)
{
  if (response) {
    if (response->root) {
      pillowtalk_free_node(response->root);
    }
    free(response);
  }
}

pt_response_t* pillowtalk_delete(const char* server_target)
{
  pt_response_t* res = http_operation("DELETE",server_target,NULL,0);
  return res;
}

pt_response_t* pillowtalk_put(const char* server_target, pt_node_t* doc)
{
  char* data = NULL;
  int data_len = 0;
  if (doc) {
    data = pillowtalk_to_json(doc,0);
    if (data)
      data_len = strlen(data);
  }
  pt_response_t* res = http_operation("PUT",server_target,data,data_len);
  if (data)
    free(data);
  return res;
}

pt_response_t* pillowtalk_put_raw(const char* server_target, const char* data, unsigned int data_len)
{
  pt_response_t* res = http_operation("PUT",server_target,data,data_len);
  return res;
}

pt_response_t* pillowtalk_get(const char* server_target)
{
  pt_response_t* res = http_operation("GET",server_target,NULL,0);
  return res;
}

pt_node_t* pillowtalk_map_get(pt_node_t* map,const char* key)
{
  if (map->type == PT_MAP) {
    pt_map_t* real_map = (pt_map_t*) map;
    pt_key_value_t* search_result = NULL;
    HASH_FIND(hh,real_map->key_values,key,strlen(key),search_result);
    if (search_result) {
      return search_result->value;
    } else {
      return NULL;
    }
  } else {
    return NULL;
  }
}

unsigned int pillowtalk_array_len(pt_node_t* array)
{
  if (array->type == PT_ARRAY) {
    return ((pt_array_t*) array)->len;
  } else {
    return 0;
  }
}

pt_node_t* pillowtalk_array_get(pt_node_t* array, unsigned int idx)
{
  if (array->type == PT_ARRAY) {
    pt_array_t* real_array = (pt_array_t*) array;
    int index = 0;
    pt_array_elem_t* cur = NULL;
    TAILQ_FOREACH(cur,&real_array->head, entries) {
      if (index == idx) {
        return cur->node;
      }
    }
  }
  return NULL;
}

/* Pass in the pointer to the elem and remove it if it exists */
void pillowtalk_array_remove(pt_node_t* array, pt_node_t* node)
{
  if (array->type == PT_ARRAY) {
    pt_array_t* real_array = (pt_array_t*) array;
    pt_array_elem_t* cur = NULL;
    pt_array_elem_t* tmp = NULL;
    TAILQ_FOREACH_SAFE(cur,&real_array->head, entries, tmp) {
      if (cur->node == node) {
        TAILQ_REMOVE(&real_array->head,cur,entries);
        pillowtalk_free_node(cur->node);
        free(cur);
        real_array->len--;
        break;
      }
    }
  }
}

void pillowtalk_array_push_front(pt_node_t* array, pt_node_t* node)
{
  if (array->type == PT_ARRAY) {
    pt_array_t* real_array = (pt_array_t*) array;
    pt_array_elem_t* elem = (pt_array_elem_t*) malloc(sizeof(pt_array_elem_t));
    elem->node = node;
    real_array->len++;
    TAILQ_INSERT_HEAD(&real_array->head,elem,entries);
  }
}

void pillowtalk_array_push_back(pt_node_t* array, pt_node_t* node)
{
  if (array->type == PT_ARRAY) {
    pt_array_t* real_array = (pt_array_t*) array;
    pt_array_elem_t* elem = (pt_array_elem_t*) malloc(sizeof(pt_array_elem_t));
    elem->node = node;
    real_array->len++;
    TAILQ_INSERT_TAIL(&real_array->head,elem,entries);
  }
}

int pillowtalk_is_null(pt_node_t* null)
{
  return null->type == PT_NULL;
}

int pillowtalk_boolean_get(pt_node_t* boolean)
{
  if (boolean->type == PT_BOOLEAN) {
    pt_bool_value_t* bool_node = (pt_bool_value_t*) boolean;
    return bool_node->value;
  } else {
    return 0;
  }
}

int pillowtalk_integer_get(pt_node_t* integer)
{
  if (integer->type == PT_INTEGER) {
    return ((pt_int_value_t*) integer)->value;
  } else if (integer->type == PT_DOUBLE) {
    return (int) ((pt_double_value_t*) integer)->value;
  } else {
    return 0;
  }
}

double pillowtalk_double_get(pt_node_t* dbl)
{
  if (dbl->type == PT_DOUBLE) {
    return ((pt_double_value_t*) dbl)->value;
  } else if (dbl->type == PT_INTEGER) {
    return (double) ((pt_int_value_t*) dbl)->value;
  } else {
    return 0;
  }
}

const char* pillowtalk_string_get(pt_node_t* string)
{
  if (string->type == PT_STRING) {
    return ((pt_str_value_t*) string)->value;
  } else {
    return NULL;
  }
}

/* Build a new pt_map_t* and initialize it */
pt_node_t* pillowtalk_map_new()
{
  pt_node_t* new_node = (pt_node_t*) calloc(1,sizeof(pt_map_t));
  new_node->type = PT_MAP;
  return new_node;
}

pt_node_t* pillowtalk_null_new()
{
  pt_node_t* new_node = (pt_node_t*) calloc(1,sizeof(pt_null_value_t));
  new_node->type = PT_NULL;
  return new_node;
}

pt_node_t* pillowtalk_bool_new(int boolean)
{
  pt_bool_value_t* new_node = (pt_bool_value_t*) calloc(1,sizeof(pt_bool_value_t));
  new_node->parent.type = PT_BOOLEAN;
  new_node->value = boolean;
  return (pt_node_t*) new_node;
}

pt_node_t* pillowtalk_integer_new(int integer)
{
  pt_int_value_t* new_node = (pt_int_value_t*) calloc(1,sizeof(pt_int_value_t));
  new_node->parent.type = PT_INTEGER;
  new_node->value = integer;
  return (pt_node_t*) new_node;
}

pt_node_t* pillowtalk_double_new(double dbl)
{
  pt_double_value_t* new_node = (pt_double_value_t*) calloc(1,sizeof(pt_double_value_t));
  new_node->parent.type = PT_DOUBLE;
  new_node->value = dbl;
  return (pt_node_t*) new_node;
}

void pillowtalk_map_set(pt_node_t* map, const char* key, pt_node_t* value)
{
  pt_map_t* real_map = (pt_map_t*) map;
  pt_key_value_t* search_result = NULL;
  HASH_FIND(hh,real_map->key_values,key,strlen(key),search_result);
  if (search_result) {
    // free the old value
    pillowtalk_free_node(search_result->value);
    search_result->value = value;
  } else {
    pt_key_value_t* new_node = (pt_key_value_t*) calloc(1,sizeof(pt_key_value_t));
    char* new_key = strdup(key);
    new_node->parent.type = PT_KEY_VALUE;
    new_node->key = new_key;
    new_node->value = value;
    HASH_ADD_KEYPTR(hh,real_map->key_values,new_node->key,strlen(new_node->key),new_node);
  }
}

void pillowtalk_map_unset(pt_node_t* map, const char* key)
{
  pt_map_t* real_map = (pt_map_t*) map;
  pt_key_value_t* search_result = NULL;
  HASH_FIND(hh,real_map->key_values,key,strlen(key),search_result);
  if (search_result) {
    HASH_DEL(real_map->key_values,search_result);
    pillowtalk_free_node(search_result->value);
    free(search_result->key);
    free(search_result);
  }
}

pt_node_t* pillowtalk_string_new(const char* str)
{
  pt_str_value_t* new_node = (pt_str_value_t*) calloc(1,sizeof(pt_str_value_t));
  new_node->parent.type = PT_STRING;
  new_node->value = strdup(str);
  return (pt_node_t*) new_node;
}

pt_node_t* pillowtalk_array_new()
{
  pt_node_t* new_node = (pt_node_t*) calloc(1,sizeof(pt_array_t));
  new_node->type = PT_ARRAY;
  TAILQ_INIT(&((pt_array_t*) new_node)->head);
  return new_node;
}

pt_node_t* pillowtalk_array_push(pt_node_t* ary, pt_node_t* node)
{
  pt_node_t* new_node = (pt_node_t*) calloc(1,sizeof(pt_array_t));
  new_node->type = PT_ARRAY;
  return new_node;
}

char* pillowtalk_to_json(pt_node_t* root, int beautify)
{
  yajl_gen_config conf = { beautify,"  "};
  yajl_gen g = yajl_gen_alloc(&conf, NULL);

  generate_node_json(root,g);
  if (root->type == PT_ARRAY) {
    generate_array_json((pt_array_t*) root,g);
  } else if (root->type == PT_MAP) {
    generate_map_json((pt_map_t*) root,g);
  }

  const unsigned char * gen_buf = NULL;
  char* json = NULL;
  unsigned int len = 0;

  yajl_gen_get_buf(g, &gen_buf, &len);

  json = (char*) malloc(len + 1);
  memcpy(json,gen_buf,len);
  json[len] = '\0';

  yajl_gen_free(g);
  return json;
}

/* Static Implementation */

/*
 * This method wraps basic curl functionality
 */
static pt_response_t* http_operation(const char* http_method, const char* server_target, const char* data, unsigned data_len)
{
  CURL *curl_handle;
  CURLcode ret;
  struct memory_chunk recv_chunk;
  recv_chunk.memory=NULL; /* we expect realloc(NULL, size) to work */
  recv_chunk.size = 0;    /* no data at this point */

  struct memory_chunk send_chunk = {0,0,0};

  /* init the curl session */
  curl_handle = curl_easy_init();

  /* specify URL to get */
  curl_easy_setopt(curl_handle, CURLOPT_URL, server_target);

  curl_easy_setopt(curl_handle, CURLOPT_CONNECTTIMEOUT, 10);

  // Want to avoid CURL SIGNALS
  curl_easy_setopt(curl_handle, CURLOPT_NOSIGNAL, 1);

  printf("%s : %s\n",http_method,server_target);

  if (!strcmp("PUT",http_method))
    curl_easy_setopt(curl_handle, CURLOPT_UPLOAD, 1);
  else
    curl_easy_setopt(curl_handle, CURLOPT_CUSTOMREQUEST, http_method);

  if (data && data_len > 0) {
    send_chunk.memory = (char*) malloc(data_len);
    memcpy(send_chunk.memory,data,data_len);
    send_chunk.offset = send_chunk.memory;
    send_chunk.size = data_len;
    curl_easy_setopt(curl_handle, CURLOPT_READFUNCTION, send_memory_callback);
    curl_easy_setopt(curl_handle, CURLOPT_READDATA, (void*) &send_chunk);
  }

  /* send all data to this function  */
  curl_easy_setopt(curl_handle, CURLOPT_WRITEFUNCTION, recv_memory_callback);

  /* we pass our 'chunk' struct to the callback function */
  curl_easy_setopt(curl_handle, CURLOPT_WRITEDATA, (void *)&recv_chunk);

  /* some servers don't like requests that are made without a user-agent
     field, so we provide one */
  curl_easy_setopt(curl_handle, CURLOPT_USERAGENT, "pillowtalk-agent/0.1");

  /* get it! */
  ret = curl_easy_perform(curl_handle);

  pt_response_t* res = calloc(1,sizeof(pt_response_impl_t));
  if ((!ret)) {
    ret = curl_easy_getinfo(curl_handle,CURLINFO_RESPONSE_CODE, &res->response_code);
    if (ret != CURLE_OK)
      res->response_code = 500;

    if (recv_chunk.size > 0) {
      // Parse the JSON chunk returned
      recv_chunk.memory[recv_chunk.size] = '\0';
      parse_json(&recv_chunk,res);
    }
  } else {
    res->response_code = 500;
  }

  if(recv_chunk.memory)
    free(recv_chunk.memory);

  if (send_chunk.memory)
    free(send_chunk.memory);

  /* cleanup curl stuff */
  curl_easy_cleanup(curl_handle);
  return res;
}

static void *myrealloc(void *ptr, size_t size)
{
  /* There might be a realloc() out there that doesn't like reallocing
     NULL pointers, so we take care of it here */
  if(ptr)
    return (void*) realloc(ptr, size);
  else
    return (void*) malloc(size);
}

static size_t recv_memory_callback(void *ptr, size_t size, size_t nmemb, void *data)
{
  size_t realsize = size * nmemb;
  struct memory_chunk *mem = (struct memory_chunk*)data;

  mem->memory = (char*) myrealloc(mem->memory, mem->size + realsize + 1);
  if (mem->memory) {
    memcpy(&(mem->memory[mem->size]), ptr, realsize);
    mem->size += realsize;
    mem->memory[mem->size] = 0;
  }
  return realsize;
}

static size_t send_memory_callback(void *ptr, size_t size, size_t nmemb, void *data)
{
  size_t realsize = size * nmemb;
  if(realsize < 1)
    return 0;

  struct memory_chunk* mem = (struct memory_chunk*) data;
  if (mem->size > 0) {
    size_t bytes_to_copy = (mem->size > realsize) ? realsize : mem->size;
    memcpy(ptr,mem->offset,bytes_to_copy);
    mem->offset += bytes_to_copy;
    mem->size -= bytes_to_copy;
    return bytes_to_copy;
  }
  return 0;
}

/* Yajl Callbacks */
static int json_null(void* response)
{
  pt_node_t* node = (pt_node_t*) malloc(sizeof(pt_node_t));
  node->type = PT_NULL;
  add_node_to_context_container((pt_response_impl_t*) response,node);
  return 1;
}

static int json_boolean(void* response,int boolean)
{
  pt_bool_value_t * node = (pt_bool_value_t*) calloc(1,sizeof(pt_bool_value_t));
  node->parent.type = PT_BOOLEAN;
  node->value = boolean;
  add_node_to_context_container((pt_response_impl_t*) response,(pt_node_t*)node);
  return 1;
}

static int json_map_key(void* response, const unsigned char* str, unsigned int length)
{
  pt_response_impl_t* res = (pt_response_impl_t*) response;
  assert(res->stack && res->stack->container->type == PT_MAP);
  pt_map_t* container = (pt_map_t*) res->stack->container;
  pt_key_value_t* new_node = (pt_key_value_t*) calloc(1,sizeof(pt_key_value_t));
  char* new_str = (char*) malloc(length + 1);
  memcpy(new_str,str,length);
  new_str[length] = 0x0;
  new_node->key = new_str;
  new_node->parent.type = PT_KEY_VALUE;
  HASH_ADD_KEYPTR(hh,container->key_values,new_node->key,length,new_node);
  res->current_node = (pt_node_t*) new_node;
  return 1;
}

static int json_integer(void* response,long integer)
{
  pt_int_value_t* node = (pt_int_value_t*) calloc(1,sizeof(pt_int_value_t));
  node->parent.type = PT_INTEGER;
  node->value = integer;

  add_node_to_context_container(response,(pt_node_t*) node);
  return 1;
}

static int json_double(void* response,double dbl)
{
  pt_double_value_t* node = (pt_double_value_t*) calloc(1,sizeof(pt_double_value_t));
  node->parent.type = PT_DOUBLE;
  node->value = dbl;

  add_node_to_context_container(response,(pt_node_t*) node);
  return 1;
}

static int json_string(void* response, const unsigned char* str, unsigned int length)
{
  char* new_str = (char*) malloc(length + 1);
  memcpy(new_str,str,length);
  new_str[length] = 0x0;
  pt_str_value_t* node = (pt_str_value_t*) calloc(1,sizeof(pt_str_value_t));
  node->parent.type = PT_STRING;
  node->value = new_str;

  add_node_to_context_container(response,(pt_node_t*) node);
  return 1;
}

/* If we aren't in a key value pair then we create a new node, otherwise we are
 * the value 
 */
static int json_start_map(void* response)
{
  pt_response_impl_t* res = (pt_response_impl_t*) response;
  pt_node_t* new_node = (pt_node_t*) calloc(1,sizeof(pt_map_t));
  new_node->type = PT_MAP;
  add_node_to_context_container(res,new_node);
  pt_container_ctx_t* new_ctx = (pt_container_ctx_t*) calloc(1,sizeof(pt_container_ctx_t));
  new_ctx->container = new_node;
  LL_PREPEND(res->stack,new_ctx);
  return 1;
}

static int json_end_map(void* response)
{
  pt_response_impl_t* res = (pt_response_impl_t*) response;
  assert(res->stack->container->type == PT_MAP);
  pt_container_ctx_t* old_head = res->stack;
  LL_DELETE(res->stack,old_head);
  free(old_head);
  return 1;
}

static int json_start_array(void* response)
{
  pt_response_impl_t* res = (pt_response_impl_t*) response;
  pt_array_t* new_node = (pt_array_t*) calloc(1,sizeof(pt_array_t));
  TAILQ_INIT(&new_node->head);
  new_node->parent.type = PT_ARRAY;
  add_node_to_context_container(res,(pt_node_t*) new_node);
  pt_container_ctx_t* new_ctx = (pt_container_ctx_t*) calloc(1,sizeof(pt_container_ctx_t));
  new_ctx->container = (pt_node_t*) new_node;
  LL_PREPEND(res->stack,new_ctx);
  res->current_node = (pt_node_t*) new_node;
  return 1;
}

static int json_end_array(void* response)
{
  pt_response_impl_t* res = (pt_response_impl_t*) response;
  assert(res->stack->container->type == PT_ARRAY);
  pt_container_ctx_t* old_head = res->stack;
  LL_DELETE(res->stack,old_head);
  free(old_head);
  res->current_node = res->stack->container;
  return 1;
}

/*
 * This function looks to see what the current node and adds the new value node to it.
 * If it is an array it appends the value to the array.
 * If it is a key value pair it adds it to the value field of that pair.
 */
static void add_node_to_context_container(pt_response_impl_t* res, pt_node_t* new_node)
{
  if (res->current_node) {
    if (res->current_node->type == PT_ARRAY) {
      pt_array_t* resolved = (pt_array_t*) res->current_node;
      pt_array_elem_t* elem = (pt_array_elem_t*) malloc(sizeof(pt_array_elem_t));
      elem->node = new_node;
      TAILQ_INSERT_TAIL(&resolved->head,elem,entries);
      resolved->len++;
    } else if (res->current_node->type == PT_KEY_VALUE) {
      pt_key_value_t* resolved = (pt_key_value_t*) res->current_node;
      resolved->value = new_node;
    }
  } else {
    ((pt_response_t*)res)->root = new_node;
    res->current_node = new_node;
  }
}

static void parse_json(struct memory_chunk* chunk,pt_response_t* res)
{
  yajl_status stat;
  yajl_handle hand;
  yajl_parser_config cfg = { 0, 1 };

  hand = yajl_alloc(&callbacks, &cfg, NULL, res);
  stat = yajl_parse(hand, (const unsigned char*) chunk->memory, chunk->size);

  yajl_free(hand);
}

static void free_map_node(pt_map_t* map)
{
  pt_key_value_t* cur = NULL;
  while(map->key_values) {
    cur = map->key_values;
    HASH_DEL(map->key_values, cur);
    pillowtalk_free_node(cur->value);
    free(cur->key);
    free(cur);
  }
}

static void free_array_node(pt_array_t* array)
{
  while (!TAILQ_EMPTY(&array->head)) {
    pt_array_elem_t* elem;
    elem = TAILQ_FIRST(&array->head);
    TAILQ_REMOVE(&array->head, elem, entries);
    pillowtalk_free_node(elem->node);
    free(elem);
  }
}

/* Recursive Free Function.  Watch the fireworks! */
void pillowtalk_free_node(pt_node_t* node)
{
  switch(node->type) {
    case PT_MAP:
      {
        free_map_node((pt_map_t*) node);
      }
      break;
    case PT_ARRAY:
      {
        free_array_node((pt_array_t*) node);
      }
      break;
    case PT_STRING:
      free(((pt_str_value_t*) node)->value);
      break;
    default:
      break;
    // the basic value types will get handled in the free(node) below
  }
  free(node);
}

static void generate_map_json(pt_map_t* map, yajl_gen g)
{
  pt_key_value_t* cur = NULL;

  yajl_gen_map_open(g);
  for(cur=map->key_values; cur != NULL; cur=cur->hh.next) {
    yajl_gen_string(g,(const unsigned char*) cur->key,strlen(cur->key));
    generate_node_json(cur->value,g);
  }
  yajl_gen_map_close(g);
}

static void generate_array_json(pt_array_t* array, yajl_gen g)
{
  pt_array_t* real_array = (pt_array_t*) array;
  pt_array_elem_t* cur = NULL;
  yajl_gen_array_open(g);
  TAILQ_FOREACH(cur,&real_array->head, entries) {
    generate_node_json(cur->node,g);
  }
  yajl_gen_array_close(g);
}

static void generate_node_json(pt_node_t* node, yajl_gen g)
{
  switch(node->type) {
    case PT_ARRAY:
      generate_array_json((pt_array_t*) node,g);
      break;

    case PT_MAP:
      generate_map_json((pt_map_t*) node,g);
      break;

    case PT_NULL:
      yajl_gen_null(g);
      break;

    case PT_BOOLEAN:
      yajl_gen_bool(g,((pt_bool_value_t*) node)->value);
      break;

    case PT_INTEGER:
      yajl_gen_integer(g,((pt_int_value_t*) node)->value);
      break;

    case PT_DOUBLE:
      yajl_gen_double(g,((pt_double_value_t*) node)->value);
      break;

    case PT_STRING:
      yajl_gen_string(g,(const unsigned char*) ((pt_str_value_t*) node)->value, strlen(((pt_str_value_t*) node)->value));
      break;

    case PT_KEY_VALUE:
      break;
  }
}
