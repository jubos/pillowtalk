#include "pillowtalk_impl.h"
#include <stdlib.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <curl/curl.h>
#include <curl/types.h>
#include <curl/easy.h>

#include <pthread.h>

extern pt_node_t* parse_json(const char* json, int json_len);

typedef struct pt_changes_feed_linked_list_t * pt_changes_feed_ll;
typedef struct pt_thread_obj_t * pt_thread_obj;
typedef struct pt_buffer_t * pt_buffer;

struct pt_thread_obj_t
{
  pthread_t        thread;
  pthread_mutex_t  cond_mutex; 
  pthread_mutex_t  list_mutex; 
  pthread_cond_t   cond; 
};

struct pt_changes_feed_linked_list_t {
  pt_node_t *line; 
  pt_changes_feed_ll next_node;
};

typedef enum {
  /** Indicates that there is a stale state (beginning) */
  pt_changes_feed_data_stale = 0,
  /** Indicates that the something is waiting for a signal */
  pt_changes_feed_data_waiting_for_signal,
  /** Indicates that the data thread is ready */
  pt_changes_feed_data_ready,
  /** Indicates that the data thread is done */
  pt_changes_feed_data_done
} pt_changes_feed_data_status;

struct pt_buffer_t {
  char   *buffer;
  size_t  size;
};

struct pt_changes_feed_t {
  int                         continuous; 
  int                         heartbeats;
  int                         stop_requested;
  char                       *extra_opts;
  pt_changes_feed_data_status status;
  pt_thread_obj               thread;
  pt_changes_feed_ll          active_list;
  pt_changes_callback_func    data_callback;  
  pt_buffer                   temp_buffer;
};

typedef struct {
  CURL            *curl_handle;
  pt_changes_feed cf_handle;
} pt_thread_arg;

pt_changes_feed_ll pt_list_alloc();
void pt_list_free(pt_changes_feed_ll ll, int retain_node);

pt_thread_obj pt_thread_alloc();
void pt_thread_free(pt_thread_obj thread);

void pt_append_to_list(pt_changes_feed handle, pt_node_t* node);
pt_node_t *pt_pop_from_list(pt_changes_feed handle);

pt_buffer pt_buffer_alloc();
void pt_buffer_free(pt_buffer buf);

int pt_process_changes_feed_buffer(pt_changes_feed handle, int flush);

/** deal with thread signals */
void pt_signal_data_is_ready(pt_changes_feed handle);
void pt_signal_data_is_done(pt_changes_feed handle);
int pt_block_until_data(pt_changes_feed handle);

static size_t recv_changes_callback(void *ptr, size_t size, size_t nmemb, void *data);
static void *pt_readout_thread(void *ptr); 

size_t safe_strlen(const char* astr);
void *safe_realloc(void *ptr, size_t size);

//___________________________________________________________________________
/** Allocate a thread */
pt_thread_obj pt_thread_alloc()
{
  pt_thread_obj ret_thr;
  ret_thr = calloc(1, sizeof(*ret_thr));
  pthread_mutex_init(&ret_thr->cond_mutex, NULL);
  pthread_mutex_init(&ret_thr->list_mutex, NULL);
  pthread_cond_init(&ret_thr->cond, NULL);
  return ret_thr;
}

//___________________________________________________________________________
/** Free a thread */
void pt_thread_free(pt_thread_obj thread)
{
  if (!thread) return;
  pthread_mutex_destroy(&thread->cond_mutex);
  pthread_mutex_destroy(&thread->list_mutex);
  pthread_cond_destroy(&thread->cond);
  free(thread);
}

//___________________________________________________________________________
/** Allocate a list */
pt_changes_feed_ll pt_list_alloc()
{
  pt_changes_feed_ll ret_obj = 
    (pt_changes_feed_ll) calloc(1, sizeof(struct pt_changes_feed_linked_list_t));

  return ret_obj;
}

//___________________________________________________________________________
/** Recursively free a list, if retain_node == 0, the corresponding nodex are
 * also freed.
 */
void pt_list_free(pt_changes_feed_ll handle, int retain_node)
{
  if (!handle) return;
  pt_list_free(handle->next_node, retain_node);
  if (!retain_node) pt_free_node(handle->line); 
  free(handle);
}

//___________________________________________________________________________
/** Allocate a buffer struct. 
 */
pt_buffer pt_buffer_alloc()
{
  pt_buffer ret_buf;
  ret_buf = calloc(1, sizeof(*ret_buf));
  return ret_buf;
}

//___________________________________________________________________________
/** Free buffer 
 */
void pt_buffer_free(pt_buffer buf)
{
  free(buf->buffer);
  free(buf);
}

#define CHECK_AND_PERFORM_FUNCTION(check, func, var)      \
  if (check) func(var);

#define CHECK_LOCK_MUTEX(check, var)                      \
  CHECK_AND_PERFORM_FUNCTION(check, pthread_mutex_lock, var)

#define CHECK_UNLOCK_MUTEX(check, var)                    \
  CHECK_AND_PERFORM_FUNCTION(check, pthread_mutex_unlock, var)
//___________________________________________________________________________
/** Perform thread safe list appending, this takes over ownership of the node
 * from the caller.
 */
void pt_append_to_list(pt_changes_feed handle, pt_node_t* node)
{
  pt_changes_feed_ll new_list_entry = pt_list_alloc();
  new_list_entry->line = node;
  pt_changes_feed_ll test = NULL;

  // Acquire the lock
  CHECK_LOCK_MUTEX(handle->thread, &handle->thread->list_mutex);
  // Setup the linked list
  if (!handle->active_list) handle->active_list = new_list_entry; 
  else {
    test = handle->active_list;
    while (test->next_node) test = test->next_node; 
    test->next_node = new_list_entry;
  }
  // Release lock
  CHECK_UNLOCK_MUTEX(handle->thread, &handle->thread->list_mutex);
}

//___________________________________________________________________________
/** Perform thread safe popping from list, this pt_node is now owned by
 * function caller and so *MUST* be freed via pt_free_node.  Will return NULL
 * if nothing is available. 
 */
pt_node_t *pt_pop_from_list(pt_changes_feed handle)
{
  pt_node_t *ret_node = NULL;
  pt_changes_feed_ll ll = NULL; 

  // Acquire the lock
  CHECK_LOCK_MUTEX(handle->thread, &handle->thread->list_mutex);

  // This automatically clears the list if this is the last node.
  if (handle->active_list) {
    ll = handle->active_list;
    handle->active_list = ll->next_node;
  }

  // Release lock
  CHECK_UNLOCK_MUTEX(handle->thread, &handle->thread->list_mutex);
  
  // If there was no node popped, return NULL;
  if (!ll) return ret_node;

  // Set the node to be returned.
  ret_node = ll->line;

  // Set the next_node to be NULL, to avoid recursive deletion.
  ll->next_node = NULL;

  // Free the list component object, do *NOT* free the node we are about to
  // return.
  pt_list_free(ll, 1);
  return ret_node;
}

//___________________________________________________________________________
/** Allocate a new changes feed object */
pt_changes_feed pt_changes_feed_alloc()
{
  pt_changes_feed ret_obj = 
    (pt_changes_feed) calloc(1, sizeof(struct pt_changes_feed_t));

  // Perform default configuration
  pt_changes_feed_config(ret_obj, pt_changes_feed_continuous, 0); 
  pt_changes_feed_config(ret_obj, pt_changes_feed_req_heartbeats, 0); 
  ret_obj->temp_buffer = pt_buffer_alloc();

  return ret_obj;
}

//___________________________________________________________________________
/** Free a changes feed object */
void pt_changes_feed_free(pt_changes_feed handle)
{
  pt_thread_free(handle->thread); 
  free(handle->extra_opts);
  pt_list_free(handle->active_list, 0);
  pt_buffer_free(handle->temp_buffer); 
  free(handle);
}

//___________________________________________________________________________
/** A safe strlen, returns 0 if astr is NULL. 
 */
size_t safe_strlen(const char* astr)
{
  return (astr == NULL) ? 0 : strlen(astr);
}

//___________________________________________________________________________
/** A safe realloc. 
 */
void *safe_realloc(void *ptr, size_t size) 
{
  if(ptr) return (void*) realloc(ptr, size);
  else return (void*) malloc(size);
}

//___________________________________________________________________________
/** A safe copy of a string, copies into the passed in new  char **.
 */
void safe_copy_string(char** new_str, const char* orig_str)
{
  free(*new_str); 
  *new_str = malloc(safe_strlen(orig_str));
  strcpy(*new_str, orig_str);
}

//___________________________________________________________________________
/** build a URL, returned char* needs to be free'd */
char* pt_changes_feed_build_url(const char* server_name,
  const char* db,
  const pt_changes_feed handle)
{
  char *ret_str;
  size_t size_of_string = safe_strlen(server_name) + 
                          safe_strlen(db) + 
                          safe_strlen(handle->extra_opts) +
                          safe_strlen("//_changes?heartbeat=&feed=continuous") + 2048;
  ret_str = calloc(1, size_of_string);

  // Add the changes feed base URL 
  sprintf(ret_str, "%s/%s/_changes", server_name, db);
  if (!handle->continuous && !handle->heartbeats &&
      !safe_strlen(handle->extra_opts)) return ret_str; 
 
  // If we get here, it means that we have added options.  There is *no*
  // checking at this point!
  strcat(ret_str, "?");

  // Hearbeat
  if (handle->heartbeats) {
    sprintf(ret_str + strlen(ret_str),"heartbeat=%i&",handle->heartbeats);
  }

  // Continuous
  if (handle->continuous) strcat(ret_str, "feed=continuous&");

  // Adding extra options passed by the user
  if (safe_strlen(handle->extra_opts)) strcat(ret_str, handle->extra_opts);
  
  // If we have an ampersand on the end, remove it
  char* end_of_string = &ret_str[safe_strlen(ret_str)-1];
  if (*end_of_string == '&') *end_of_string = '\0'; 
  return ret_str; 
}

//___________________________________________________________________________
/** dealing with configuration of changes feed handle. */
int pt_changes_feed_config(pt_changes_feed handle, pt_changes_feed_option opt, ... )
{
  int ret_val = 1;
  va_list vlist;
  va_start(vlist, opt);

  // Switch on the options
  switch(opt) {
    case pt_changes_feed_continuous:
      handle->continuous = (va_arg(vlist, int));
      break;
    case pt_changes_feed_req_heartbeats:
      handle->heartbeats = (va_arg(vlist, int));
      break;
    case pt_changes_feed_callback_function:
      handle->data_callback = va_arg(vlist, pt_changes_callback_func); 
      break;
    case pt_changes_feed_generic_opts:
      safe_copy_string(&handle->extra_opts, va_arg(vlist, const char*));
      break;
    default:
      ret_val = 0;
  } 

  va_end(vlist);
  return ret_val;

} 

//___________________________________________________________________________
/** Perform the curl on this handle and *THEN* cleanup.  This means that this
 * function take control of the passed in curl_handle.
 */
void pt_perform_curl(CURL *curl_handle)
{
  // perform the Get, not yet checking for errors
  curl_easy_perform(curl_handle);

  // cleanup curl stuff, because we own this
  curl_easy_cleanup(curl_handle);
}

//___________________________________________________________________________
void *pt_readout_thread(void *arg)
{
  // We now *own* this curl handle
  pt_thread_arg *thread_arg = (pt_thread_arg *)arg;
  pt_changes_feed handle = thread_arg->cf_handle;

  pt_perform_curl((CURL*) thread_arg->curl_handle);
 
  // We're done, process the remaining feed buffer
  if (pt_process_changes_feed_buffer(handle, 1) != 0) { 
    pt_signal_data_is_ready(handle);
  }

  pt_signal_data_is_done(handle);
  pthread_exit(0);
}

//___________________________________________________________________________
/** Perform a loop over the list in the handle, and call the callback function.
 */
void pt_pop_from_list_and_callback(pt_changes_feed handle)                
{
  pt_node_t* anode;
  while (!handle->stop_requested && 
          (anode = pt_pop_from_list(handle))) {   
    if (handle->data_callback) {
      handle->stop_requested = (handle->data_callback(anode) < 0); 
    }
    pt_free_node(anode);                                     
  }
}

//___________________________________________________________________________
/** Does the meat of the changes feed work. */
void pt_changes_feed_run(pt_changes_feed handle, 
  const char* server_name,
  const char* database) 
{
  CURL *curl_handle;
  char *full_server_path = 
    pt_changes_feed_build_url(server_name, database, handle);

  /* init the curl session */
  curl_handle = curl_easy_init();

  /* specify URL to get */
  curl_easy_setopt(curl_handle, CURLOPT_URL, full_server_path);
  free(full_server_path);

  curl_easy_setopt(curl_handle, CURLOPT_CONNECTTIMEOUT, 10);

  // Want to avoid CURL SIGNALS
  curl_easy_setopt(curl_handle, CURLOPT_NOSIGNAL, 1);
  curl_easy_setopt(curl_handle, CURLOPT_NOPROGRESS, 1);
  curl_easy_setopt(curl_handle, CURLOPT_CUSTOMREQUEST, "GET");
  curl_easy_setopt(curl_handle, CURLOPT_WRITEFUNCTION, recv_changes_callback);

  /* we pass our handle struct to the callback function */
  curl_easy_setopt(curl_handle, CURLOPT_WRITEDATA, (void *)handle);

  // Make sure to reset the stop_requested variable
  handle->stop_requested = 0;

  // Remove list
  pt_list_free(handle->active_list, 0);
  handle->active_list = NULL;

  // Check if it's continuous readback
  if (handle->continuous) {

    /* We now need to deal with starting the thread */ 
     
    // Alloc a thread
    handle->thread = pt_thread_alloc();
    // pass information to the handle 
    pt_thread_obj thread_obj = handle->thread;  
    pt_thread_arg arg = { curl_handle, handle };
    
    // Set the status of the handle to stale.
    handle->status = pt_changes_feed_data_stale;

    // Start the thread
    pthread_create(&thread_obj->thread, NULL, &pt_readout_thread, &arg); 
    
    // Perform loop waiting for data
    while(1) {
      if(!pt_block_until_data(handle)) break;
      // Now we can pop as many as possible
      pt_pop_from_list_and_callback(handle);
    } 

    // The readout thread is done, re-join.
    pthread_join(thread_obj->thread, NULL);
    
    // Free the thread
    pt_thread_free(thread_obj);
    
    // Set to NULL
    handle->thread = NULL;

  } else { // Not continuous
    // We don't need to create any threads, simply perform the curl command. 
    pt_perform_curl(curl_handle);

    // Flush the buffer, this will make everything available in the list.
    pt_process_changes_feed_buffer(handle, 1);

  }
  
  // Now loop over the nodes that are still in the queue and pass them on to
  // the callback function
  pt_pop_from_list_and_callback(handle);

}

//___________________________________________________________________________
/** Receive the changes feed and append to buffer.  Will append the incoming
 * data to the available buffer in the pt_changes_feed struct. 
 */
size_t recv_changes_callback(void *ptr, size_t size, size_t nmemb, void *data)
{
  size_t realsize = size * nmemb;
  pt_changes_feed handle = (pt_changes_feed) data;
  pt_buffer buffer = handle->temp_buffer;

  // Remove the appended '\0' if it's there
  if (buffer->buffer && 
      buffer->size > 1 && 
      buffer->buffer[buffer->size-1] == '\0') {
    buffer->size -= 1;
  }

  buffer->buffer = safe_realloc(buffer->buffer, buffer->size + realsize + 1);
  if (buffer->buffer) {
    memcpy(buffer->buffer + buffer->size, ptr, realsize);
    buffer->size += realsize + 1;
    buffer->buffer[buffer->size - 1] = '\0';
  }

  // Try to process the buffer, will return the number of processed nodes
  if (pt_process_changes_feed_buffer(handle, 0) != 0) {
    // signal that data is ready in the queue 
    pt_signal_data_is_ready(handle);
  }

  // Check to see if we should stop
  if (handle->stop_requested) return CURL_READFUNC_ABORT;
  return realsize;
}

//___________________________________________________________________________
/** Handle the buffer, 
 *  Returns the number of nodes that were processed, 0 if no nodes were
 *  processed. 
 */
int pt_process_changes_feed_buffer(pt_changes_feed handle, int flush)
{
  int ret_val = 0;
  pt_buffer buffer = handle->temp_buffer;

  // If we're not doing a continuous feed, simply return
  // If we are not flushing and not continuous, just return.
  // We will handle the non-continuous feeds on the last call.
  if (!flush && !handle->continuous) return ret_val;
  if (flush && !handle->continuous) {
    // process the entire chunk that came out as json
    pt_node_t *anode = pt_from_json(buffer->buffer); 
    pt_append_to_list(handle, anode);
    // Reset the buffer size
    buffer->size = 0; 
    if (anode) ret_val = 1;
    return ret_val;
  } 
  // Now simply try to grab as much as possible
  size_t available_data_size = buffer->size;
  char *current_ptr = buffer->buffer;
  while (available_data_size > 0) {
    char *next_end_line = index(current_ptr, '\n');
    if (!next_end_line) break;
    // we have a ptr to the next end line
    if (next_end_line == current_ptr) {
      // pass blank lines on as pt_null node objects 
      pt_append_to_list(handle, pt_null_new());
      current_ptr++;
      available_data_size--;
      ret_val++;
      continue;
    }
    // otherwise process the line
    *next_end_line = '\0';
    pt_node_t *anode = pt_from_json(current_ptr);
    pt_append_to_list(handle, anode);
    if (anode) ret_val++;
    available_data_size -= (next_end_line - current_ptr + 1); 
    current_ptr = next_end_line + 1;
  } 
  
  // Now we need to clean everything up
  if (available_data_size == 1) {
    // everything is flushed, we can simply adjust the size 
    buffer->size = 0; 
  } else {
    memmove(buffer->buffer, current_ptr, available_data_size);
    buffer->size = available_data_size;
  } 
  return ret_val;
}


#define CHECK_COND_FLAG_BEGIN(handle)                              \
  pt_thread_obj thread_obj = handle->thread;                       \
  pthread_mutex_lock(&thread_obj->cond_mutex);                     \

#define COND_WAIT(handle, astat)                                   \
  while (handle->status == astat) {                                \
    pthread_cond_wait(&thread_obj->cond, &thread_obj->cond_mutex); \
  } 

// Perform a signal if that status is correct
#define COND_SIGNAL(handle, astat, new_stat)                       \
  if (handle->status == astat) {                                   \
    pthread_cond_signal(&thread_obj->cond);                        \
  }                                                                \
  handle->status = new_stat;

#define CHECK_COND_FLAG_END \
  pthread_mutex_unlock(&thread_obj->cond_mutex);

//______________________________________________________________________________
/** signal that data is ready */
void pt_signal_data_is_ready(pt_changes_feed handle) 
{
  //return;
  CHECK_COND_FLAG_BEGIN(handle)
  // Set the status to allow the condition to continue
  COND_SIGNAL(handle, 
    pt_changes_feed_data_waiting_for_signal, 
    pt_changes_feed_data_ready) 
  CHECK_COND_FLAG_END
}

//______________________________________________________________________________
/** Block thread until data is ready returns 1 if there is still data. */
int pt_block_until_data(pt_changes_feed handle)
{
  //return 1;
  int ret_val = 1;
  CHECK_COND_FLAG_BEGIN(handle)
  // Set the status to allow the condition to continue
  if (handle->status == pt_changes_feed_data_stale) 
    handle->status = pt_changes_feed_data_waiting_for_signal;
  COND_WAIT(handle, pt_changes_feed_data_waiting_for_signal)
  if (handle->status == pt_changes_feed_data_done) ret_val = 0;
  handle->status = pt_changes_feed_data_stale;
  CHECK_COND_FLAG_END
  return ret_val;
}

//______________________________________________________________________________
/** Indicate data is done */
void pt_signal_data_is_done(pt_changes_feed handle)
{
  //return;
  CHECK_COND_FLAG_BEGIN(handle)
  COND_SIGNAL(handle, pt_changes_feed_data_waiting_for_signal, pt_changes_feed_data_done) 
  CHECK_COND_FLAG_END
}
