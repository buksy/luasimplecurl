/**
 * Copyright [2013] [Gihan Munasinghe ayeshka@gmail.com ]
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 *
 */

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

typedef struct curl_slist reqHeaders;

typedef struct simple_curl
{
  CURL *curl;
  lua_State *L;
  sHeaderList *hlist;
  short hdone;
  int writeRef;
  int readRef;
  int lastError;
  reqHeaders *requestH;
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
	  if (hlist->headers[i])
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
	h->name = strdup (ptr);
      else
	h->value = strdup (ptr);
      ptr = strtok_r (NULL, "= ", &saveptr);
    }

}

// Read callback function -- This will be used in PUT
size_t
read_callback_fn (void *ptr, size_t size, size_t nmemb, void *userdata)
{
  //printf ("Calling write \n\n");
  sCurl *c = (sCurl *) userdata;
  size_t ret = 0;
  if (c->readRef != LUA_REFNIL)
    {
      ret = size * nmemb;
      // Load the function in LUA 
      lua_rawgeti (c->L, LUA_REGISTRYINDEX, c->readRef);
      lua_pushinteger (c->L, ret);
      lua_call (c->L, 1, 2);
      // Get the reaturn value from the stack 
      // Callback to lua should return size of data and the data it self 
      size_t out = 0;
      out = lua_tointeger (c->L, -2);
      if (out > 0)
	{

	  if (out <= ret)
	    {
	      memcpy (ptr, lua_tolstring (c->L, -1, &out), out);
	      ret = out;
	    }
	  else
	    {
	      luaL_error (c->L, "More data send from the read function");
	      ret = CURL_READFUNC_ABORT;
	    }

	}
      else if (out < 0)
	{
	  ret = CURL_READFUNC_ABORT;
	}
      else
	ret = 0;
    }
  return ret;
}

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
	  free (header);
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
  // stack now contains: -1 => table
  if (lua_istable (L, -1))
    {
      lua_pushnil (L);
      // stack now contains: -1 => nil; -2 => table
      while (lua_next (L, -2))
	{
	  // stack now contains: -1 => value; -2 => key; -3 => table
	  // copy the key so that lua_tostring does not modify the original
	  lua_pushvalue (L, -2);
	  // stack now contains: -1 => key; -2 => value; -3 => key; -4 => table
	  const char *key = lua_tostring (L, -1);
	  if (strcasecmp ("url", key) == 0)
	    {
	      curl_easy_setopt (curl, CURLOPT_URL, lua_tostring (L, -2));
	    }
	  else if (strcasecmp ("follow_redirect", key) == 0)
	    {
	      curl_easy_setopt (curl, CURLOPT_FOLLOWLOCATION,
				(lua_toboolean (L, -2)) ? 1L : 0L);
	    }
	  else if (strcasecmp ("ssl_verify", key) == 0)
	    {
	      curl_easy_setopt (curl, CURLOPT_SSL_VERIFYPEER,
				(lua_toboolean (L, -2)) ? 1L : 0L);
	    }
	  else if (strcasecmp ("ssl_cert", key) == 0)
	    {

	    }
	  else if (strcasecmp ("username", key) == 0)
	    {
	      curl_easy_setopt (curl, CURLOPT_HTTPAUTH, CURLAUTH_BASIC);
	      curl_easy_setopt (curl, CURLOPT_USERNAME, lua_tostring (L, -2));
	    }
	  else if (strcasecmp ("password", key) == 0)
	    {
	      curl_easy_setopt (curl, CURLOPT_PASSWORD, lua_tostring (L, -2));
	    }
	  else if (strcasecmp ("conn_timeout", key) == 0)
	    {
	      curl_easy_setopt (curl, CURLOPT_CONNECTTIMEOUT_MS,
				lua_tonumber (L, -2));
	    }
	  else if (strcasecmp ("read_timeout", key) == 0)
	    {
	      curl_easy_setopt (curl, CURLOPT_TIMEOUT_MS,
				lua_tonumber (L, -2));
	    }
	  // pop value + copy of key, leaving original key
	  lua_pop (L, 2);
	  // stack now contains: -1 => key; -2 => table
	}
      // stack now contains: -1 => table (when lua_next returns 0 it pops the key
      // but does not push anything.)
      // Pop table
      lua_pop (L, 1);
    }

  lua_settop (L, 0);

  sCurl *c = (sCurl *) lua_newuserdata (L, sizeof (sCurl));
  c->L = c->curl = NULL;
  c->hdone = 0;
  c->writeRef = LUA_REFNIL;
  c->readRef = LUA_REFNIL;
  c->hlist = calloc (1, sizeof (sHeaderList));
  c->L = L;
  c->requestH = NULL;
  c->curl = curl;

  //c->requestH = curl_slist_append (c->requestH, "Transfer-Encoding: chunked");

  curl_easy_setopt (curl, CURLOPT_WRITEHEADER, (void *) c);
  curl_easy_setopt (curl, CURLOPT_HEADERFUNCTION, header_callback_fn);
  curl_easy_setopt (curl, CURLOPT_WRITEDATA, (void *) c);
  curl_easy_setopt (curl, CURLOPT_WRITEFUNCTION, write_callback_fn);
  curl_easy_setopt (curl, CURLOPT_READDATA, (void *) c);
  curl_easy_setopt (curl, CURLOPT_READFUNCTION, read_callback_fn);

  // Now set the metatable
  luaL_getmetatable (L, SIMPLE_HTTP_METATABLE);
  lua_setmetatable (L, -2);

  return 1;
}

static int
url_encode (lua_State * L)
{

  if (!lua_isnil (L, 1))
    {
      const char *s = luaL_checkstring (L, 1);
      lua_pushstring (L, curl_escape (s, strlen (s)));
      return 1;
    
}
  else
    {
      luaL_error (L, "string parameter expected");
    }
  return 0;
}

static int
url_decode (lua_State * L)
{

  if (!lua_isnil (L, 1))
    {
      const char *s = luaL_checkstring (L, 1);
      lua_pushstring (L, curl_unescape (s, strlen (s)));
      return 1;
    
}
  else
    {
      luaL_error (L, "string parameter expected");
    }
  return 0;
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
	{
	  curl_easy_cleanup (c->curl);
	  c->curl = NULL;
	}

      if (c->hlist)
	{
	  clean_up_headers (c->hlist);
	  free (c->hlist);
	  c->hlist = NULL;
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

      if (c->requestH)
	{
	  curl_slist_free_all (c->requestH);
	  c->requestH = NULL;
	}
    }
  return 0;
}

static int
perform_get (lua_State * L)
{
  sCurl *c = (sCurl *) luaL_checkudata (L, 1, SIMPLE_HTTP_METATABLE);
  curl_easy_setopt (c->curl, CURLOPT_HTTPGET, 1L);
  LUA_SET_CALLBACK_FUNCTION (L, 2, writeRef, c);
  DO_CURL_PERFORM (c);
  lua_settop (L, 0);
  lua_pushboolean (L, (c->lastError == CURLE_OK));
  return 1;
}

static int
perform_post (lua_State * L)
{
// 1 ==> the Object, 2==> POST String/or a readfunction , 3 => the callback function
  sCurl *c = (sCurl *) luaL_checkudata (L, 1, SIMPLE_HTTP_METATABLE);
  if (lua_isstring (L, 2))
    {

      curl_easy_setopt (c->curl, CURLOPT_POSTFIELDS, lua_tostring (L, 2));

    }
  else if (lua_isfunction (L, 2))
    {
      // Set the chunk encoding header
      c->requestH =
	curl_slist_append (c->requestH, "Transfer-Encoding: chunked");
      curl_easy_setopt (c->curl, CURLOPT_HTTPHEADER, c->requestH);
      curl_easy_setopt (c->curl, CURLOPT_POST, 1L);
      LUA_SET_CALLBACK_FUNCTION (L, 2, readRef, c);

    }

  LUA_SET_CALLBACK_FUNCTION (L, 3, writeRef, c);
  DO_CURL_PERFORM (c);
  lua_settop (L, 0);
  lua_pushboolean (L, (c->lastError == CURLE_OK));
  return 1;
}

static int
perform_put (lua_State * L)
{
  sCurl *c = (sCurl *) luaL_checkudata (L, 1, SIMPLE_HTTP_METATABLE);

  if (lua_isfunction (L, 2))
    {
      // Set the chunk encoding header
      c->requestH =
	curl_slist_append (c->requestH, "Transfer-Encoding: chunked");
      curl_easy_setopt (c->curl, CURLOPT_HTTPHEADER, c->requestH);
      curl_easy_setopt (c->curl, CURLOPT_UPLOAD, 1L);
      LUA_SET_CALLBACK_FUNCTION (L, 2, readRef, c);

    }

  LUA_SET_CALLBACK_FUNCTION (L, 3, writeRef, c);
  DO_CURL_PERFORM (c);
  lua_settop (L, 0);
  lua_pushboolean (L, (c->lastError == CURLE_OK));
  return 1;
}

static int
setCURL_options (lua_State * L)
{
  return 0;
}

static int
set_header (lua_State * L)
{
  sCurl *c = (sCurl *) luaL_checkudata (L, 1, SIMPLE_HTTP_METATABLE);

  // Load the user def table 
  // stack now contains: -1 => table
  if (lua_istable (L, -1))
    {
      lua_pushnil (L);
      // stack now contains: -1 => nil; -2 => table
      while (lua_next (L, -2))
	{
	  // stack now contains: -1 => value; -2 => key; -3 => table
	  // copy the key so that lua_tostring does not modify the original
	  lua_pushvalue (L, -2);
	  // stack now contains: -1 => key; -2 => value; -3 => key; -4 => table
	  const char *key = lua_tostring (L, -1);
	  const char *val = lua_tostring (L, -2);

	  char *hstr = NULL;
	  asprintf (&hstr, "%s: %s", key, val);
	  if (hstr)
	    {
	      c->requestH = curl_slist_append (c->requestH, hstr);
	      free (hstr);
	    }
	  // pop value + copy of key, leaving original key
	  lua_pop (L, 2);
	}
    }

  if (c->requestH)
    {
      curl_easy_setopt (c->curl, CURLOPT_HTTPHEADER, c->requestH);
    }

  return 1;
}

static int
get_header (lua_State * L)
{
  sCurl *c = (sCurl *) luaL_checkudata (L, 1, SIMPLE_HTTP_METATABLE);
  lua_settop (L, 0);
  if (c->hlist)
    {
      int i = 0;
      lua_newtable (L);
      for (i = 0; i < c->hlist->hcount; i++)
	{
	  sHeader *h = c->hlist->headers[i];
	  if (h)
	    {
	      lua_pushstring (L, h->name);
	      lua_pushstring (L, h->value);
	      lua_settable (L, -3);
	    }
	}
    }
  else
    {
      lua_pushnil (L);
    }
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
  // {"setCURLOption", setCURL_options},
  {"setRequestHeaders", set_header},
  {"setBasicAuth", set_basic_auth},
  {"getResponseHeaders", get_header},
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
  // url , follow_redirect, ssl_verify 

  return 1;
}
