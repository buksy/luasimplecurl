#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <lua5.2/lua.h>
#include <lua5.2/lauxlib.h>
#include <curl/curl.h>

#define SIMPLE_HTTP_METATABLE "simple.http.curl"



#define LUA_SET_CALLBACK_FUNCTION(L , index , fref, c) \
	 if (lua_isfunction(L,index)) \
	  {  \
	    int ref; \
	    lua_pushvalue(L, index); \
	    ref=luaL_ref(L, LUA_REGISTRYINDEX);  /* get reference to the lua object in registry */ \
	    if (c->fref != LUA_REFNIL) \
	      luaL_unref(L, LUA_REGISTRYINDEX, c->fref);\
	    c->fref = ref; \
	  }

#define DO_CURL_PERFORM(c) \
	if (c && c->curl) \
	  c->lastError = curl_easy_perform (c->curl);

typedef struct header
{
  char *name;
  char *value;
} sHeader;

typedef struct header_list
{
  sHeader **headers;
  int hcount;
} sHeaderList;


typedef struct simple_curl
{
  CURL *curl;
  lua_State *L;
  sHeaderList *hlist;
  short hdone;
  int writeRef;
  int readRef;
  int lastError;
} sCurl;


static void
free_header (sHeader * h)
{
  if (h)
    {
      if (h->name)
	free (h->name);
      if (h->value)
	free (h->value);
      free (h);
    }
}

static void
clean_up_headers (sHeaderList * hlist)
{
  if (hlist && (hlist->headers))
    {
      int i = 0;
      for (i = 0; i < hlist->hcount; i++)
	{
	  free_header (hlist->headers[i]);
	}
      free (hlist->headers);
      hlist->headers = NULL;
      hlist->hcount = 0;
    }
}

static void
build_header (sHeader * h, char *hline)
{

  char *saveptr = NULL;
  char *ptr = strtok_r (hline, "= ", &saveptr);
  while (ptr != NULL)
    {
      if (!h->name)
	h->name = ptr;
      else
	h->value = ptr;
      ptr = strtok_r (NULL, "= ", &saveptr);
    }

}

// Read callback function -- This will be used in PUT

// Write callback function this will be used in when data if comething back from the server 
size_t
write_callback_fn (void *ptr, size_t size, size_t nmemb, void *userdata)
{
  //printf ("Calling write \n\n");
  sCurl *c = (sCurl *) userdata;
  size_t ret = (size * nmemb);
  if (c->writeRef != LUA_REFNIL)
    {
      // Load the function in LUA 
      lua_rawgeti (c->L, LUA_REGISTRYINDEX, c->writeRef);
      lua_pushlstring (c->L, ptr, ret);
      lua_call (c->L, 1, 1);
      // Get the reaturn value from the stack and 
      if (lua_toboolean (c->L, -1) == 0)	// Callback to lua should return true or false 
	ret = 0;
    }
  return ret;
}

// Header callback function this will be used to capture the headers 
size_t
header_callback_fn (void *ptr, size_t size, size_t nmemb, void *userdata)
{
  size_t ret = (size * nmemb);
  const char *data = (const char *) ptr;
  sCurl *scurl = (sCurl *) userdata;
  if (scurl)
    {
      if ((ret > 0) && (data[0] != '\r') && (data[0] != '\n'))
	{
	  if (scurl->hlist && scurl->hdone)
	    {
	      clean_up_headers (scurl->hlist);
	      scurl->hlist->headers = NULL;
	      scurl->hdone = 0;
	    }
	  char *header = calloc (1, (ret + 1));
	  memccpy (header, data, (int) '\n', ret);
	  // Remove \r\n
	  header[strlen (header) - 2] = '\0';
	  //printf("1\n");  
	  scurl->hlist->headers =
	    (sHeader **) realloc (scurl->hlist->headers,
				  sizeof (sHeader *) * (scurl->hlist->hcount +
							1));
	  //printf("2\n");
	  sHeader *h = calloc (1, sizeof (sHeader));
	  build_header (h, header);
	  //printf("3\n");
	  scurl->hlist->headers[scurl->hlist->hcount] = h;
	  //printf("4\n");
	  scurl->hlist->hcount++;
	}
      else
	{
	  if (scurl->hlist)
	    {
	      scurl->hdone = 1;
	      //printf("headers done");
	      //TODO set the metatable for headers
	    }
	}
    }
  return ret;
}

/////////////////////////////////----- Library Functions ------///////////////////////////////////////
static int
newconnect (lua_State * L)
{

  // Open the curl Handler
  CURL *curl = curl_easy_init ();

  if (!curl)
    {
      luaL_error (L, "Simple HTTP could not make a new connection");
      return 0;
    }

  // Load the user def table 
  if (lua_istable (L, -1))
    {

      //printf ("1\n");
//    printf ("Top %d\n", lua_gettop(L));

      lua_pushstring (L, "url");
      lua_gettable (L, -2);
      if (lua_isstring (L, -1))
	{
	  curl_easy_setopt (curl, CURLOPT_URL, lua_tostring (L, -1));
	  lua_pop (L, 1);
	}

//    printf ("Top %d\n", lua_gettop(L)); 

//     lua_pushstring (L, "follow_redirect");   
//     lua_gettable (L, -2);
//     printf ("Top %d\n", lua_gettop(L)); 
//     if (!(lua_isnoneornil(L, -1)) && (lua_toboolean (L, -1))) 
//     {
//      curl_easy_setopt (curl, CURLOPT_FOLLOWLOCATION, 1L);
//      lua_pop (L, 1);
//     }

      lua_pushstring (L, "ssl_veryfy");
      lua_gettable (L, -2);
      if (!(lua_isnoneornil (L, -1)) && (!lua_toboolean (L, -1)))
	{
	  //printf ("4\n");
	  curl_easy_setopt (curl, CURLOPT_SSL_VERIFYPEER, 0L);
	  lua_pop (L, 1);
	}

//    printf ("Top %d\n", lua_gettop(L));

      lua_pop (L, 1);

    }
  else if (!lua_isnil (L, -2))
    {
      luaL_error (L, "Invalid parameter, definition table required");
    }

  lua_settop (L, 0);

  sCurl *c = (sCurl *) lua_newuserdata (L, sizeof (sCurl));
  c->L = c->curl = NULL;
  c->hdone = 0;
  c->writeRef = LUA_REFNIL;
  c->readRef = LUA_REFNIL;
  c->hlist = calloc (1, sizeof (sHeaderList));
  c->L = L;
  c->curl = curl;

  curl_easy_setopt (curl, CURLOPT_WRITEHEADER, (void *) c);
  curl_easy_setopt (curl, CURLOPT_HEADERFUNCTION, header_callback_fn);
  curl_easy_setopt (curl, CURLOPT_WRITEDATA, (void *) c);
  curl_easy_setopt (curl, CURLOPT_WRITEFUNCTION, write_callback_fn);

  // Now set the metatable
  luaL_getmetatable (L, SIMPLE_HTTP_METATABLE);
  lua_setmetatable (L, -2);

  return 1;
}

static int
url_encode (lua_State * L)
{

  return 1;
}

static int
url_decode (lua_State * L)
{

  return 1;
}

static int
table_to_JSON (lua_State * L)
{

  return 1;
}

static int
json_to_table (lua_State * L)
{

  return 1;
}


/////////////////////////////------- End of Library Functions -------//////////////////////////////////




////////////////////////////-------- Metatable Functions ----------////////////////////////////////////
static int
gc_curl (lua_State * L)
{

  sCurl *c = (sCurl *) luaL_checkudata (L, 1, SIMPLE_HTTP_METATABLE);
  if (c)
    {
      if (c->curl)
	curl_easy_cleanup (c->curl);

      if (c->hlist)
	{
	  // Seeme quite wiered that lua wants to clean up this memmory eventhough it done not create them 
//      clean_up_headers(c->hlist);
//      free(c->hlist);
//      c->hlist = NULL;
	}

      if (c->writeRef != LUA_REFNIL)
	{
	  luaL_unref (L, LUA_REGISTRYINDEX, c->writeRef);
	  c->writeRef = LUA_REFNIL;
	}

      if (c->readRef != LUA_REFNIL)
	{
	  luaL_unref (L, LUA_REGISTRYINDEX, c->readRef);
	  c->readRef = LUA_REFNIL;
	}
    }
  return 0;
}

static int
perform_get (lua_State * L)
{
  sCurl *c = (sCurl *) luaL_checkudata (L, 1, SIMPLE_HTTP_METATABLE);
  LUA_SET_CALLBACK_FUNCTION (L, 2, writeRef, c);
  DO_CURL_PERFORM (c);
  lua_settop (L, 0);
  lua_pushboolean (L, (c->lastError == CURLE_OK));
  return 1;
}

static int
perform_post (lua_State * L)
{

//   DO_CURL_PERFORM (c);
//   lua_settop(L, 0);
//   lua_pushboolean(L, (c->lastError == CURLE_OK));
  return 1;
}

static int
perform_put (lua_State * L)
{

//   DO_CURL_PERFORM (c);
//   lua_settop(L, 0);
//   lua_pushboolean(L, (c->lastError == CURLE_OK));
  return 1;
}

static int
perfrom_delete (lua_State * L)
{
  return 0;
}

static int
setCURL_options (lua_State * L)
{
  return 0;
}

static int
set_header (lua_State * L)
{
  return 0;
}

static int
get_header (lua_State * L)
{

  return 1;
}

static int
set_basic_auth (lua_State * L)
{
  sCurl *c = (sCurl *) luaL_checkudata (L, 1, SIMPLE_HTTP_METATABLE);
  const char *user = luaL_checkstring (L, 2);
  const char *pass = luaL_checkstring (L, 3);
  curl_easy_setopt (c->curl, CURLOPT_HTTPAUTH, CURLAUTH_BASIC);
  curl_easy_setopt (c->curl, CURLOPT_USERNAME, user);
  curl_easy_setopt (c->curl, CURLOPT_PASSWORD, pass);
  return 0;
}

static int
disconnect (lua_State * L)
{
  return gc_curl (L);
}

static int
get_last_error (lua_State * L)
{
  sCurl *c = (sCurl *) luaL_checkudata (L, 1, SIMPLE_HTTP_METATABLE);
  lua_settop (L, 0);
  lua_pushinteger (L, c->lastError);
  lua_pushstring (L, curl_easy_strerror (c->lastError));
  return 2;
}

////////////////////////////////////------ End of Metatable Functions sCurl -----////////////////////////////////

static const luaL_Reg c_funcs[] = {
  {"__gc", gc_curl},
  {"get", perform_get},
  {"post", perform_post},
  {"put", perform_put},
  {"delete", perfrom_delete},
  {"setCURLOption", setCURL_options},
  {"setRequestHeader", set_header},
  {"setBasicAuth", set_basic_auth},
  {"getResponseHeader", get_header},
  {"disconnect", disconnect},
  {"getLastError", get_last_error},
  {NULL, NULL}
};

static void
create_matatable (lua_State * L)
{
  luaL_newmetatable (L, SIMPLE_HTTP_METATABLE);
  lua_pushliteral (L, "__index");
  lua_pushvalue (L, -2);
  lua_rawset (L, -3);
  luaL_setfuncs (L, c_funcs, 0);
}

/**
All the functions supported by library 
**/
static const luaL_Reg functions[] = {
  {"newconnect", newconnect},
  {"URLEncode", url_encode},
  {"URLDecode", url_decode},
  {"tableToJSON", table_to_JSON},
  {"JSONToTable", json_to_table},

  {NULL, NULL}
};

/*
 * Exported functions.
 */

LUALIB_API int
luaopen_simplehttp (lua_State * L)
{
  // Initialize curl
  curl_global_init (CURL_GLOBAL_ALL);
  /* Create metatable */
  create_matatable (L);
  /* Create module */
  luaL_newlib (L, functions);
  // Define a standard option table, there is only pretty limited amount of curl opetions that we use day to day
  // url , follow_redirect, ssl_veryfy 

  return 1;
}
