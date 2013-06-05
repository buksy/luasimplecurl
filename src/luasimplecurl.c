#include <stdio.h>
#include <stdlib.h>
#include <lua5.2/lua.h>
#include <lua5.2/lauxlib.h>
#include <curl/curl.h>

#define SIMPLE_CURL_METATABLE "70474f43-b594-4a62-bcc0-8239b2da7b0a"


#define CURL_ACTION_CHECK(out,L) \
	if (CURLE_OK != out) \
	  luaL_error (L, "CURL Error ( %d - %s)", \
	    out, curl_easy_strerror (out));\
	(out == CURLE_OK);


typedef struct simple_curl
{
  CURL *curl;
  lua_State *L;
} sCurl;

/////////////////////////////////----- Library Functions ------///////////////////////////////////////
static int
newconnect (lua_State * L)
{
  const char *url = luaL_checkstring (L, -1);
  if (!url)
    {
      luaL_error (L, "URL not defined");
      return 0;
    }

  lua_pop (L, 1);
  sCurl *c = (sCurl *) lua_newuserdata (L, sizeof (sCurl));
  c->L = c->curl = NULL;
  c->L = L;

  // Open the curl Handler
  c->curl = curl_easy_init ();

  if (!c->curl)
    {
      luaL_error (L, "Simple HTTP could not make a new connection");
      return 0;
    }
  // Set up the curl option for the URL to make life simple I am adding follow redirect as well
  int err = 0;
  if ((err = curl_easy_setopt (c->curl, CURLOPT_URL, url)) != CURLE_OK)
    {
      luaL_error (L, "Simple HTTP could not make a new connection ( %d - %s)",
		  err, curl_easy_strerror (err));
      return 0;
    }

  curl_easy_setopt (c->curl, CURLOPT_FOLLOWLOCATION, 1L);

  // Now set the metatable
  luaL_getmetatable (L, SIMPLE_CURL_METATABLE);
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

  sCurl *c = (sCurl *) luaL_checkudata (L, 1, SIMPLE_CURL_METATABLE);
  if (c && c->curl)
    {
      curl_easy_cleanup (c->curl);
    }
  return 0;
}

static int
perform_get (lua_State * L)
{
  sCurl *c = (sCurl *) luaL_checkudata (L, 1, SIMPLE_CURL_METATABLE);
  CURL_ACTION_CHECK (curl_easy_perform (c->curl), L);
  return 0;
}

static int
perform_post (lua_State * L)
{
  return 0;
}

static int
perform_put (lua_State * L)
{
  return 0;
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
set_basic_auth (lua_State * L)
{
  sCurl *c = (sCurl *) luaL_checkudata (L, 1, SIMPLE_CURL_METATABLE);
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

  return 0;
}

////////////////////////////////////------ End of Metatable Functions -----////////////////////////////////

static const luaL_Reg c_funcs[] = {
  {"__gc", gc_curl},
  {"get", perform_get},
  {"post", perform_post},
  {"put", perform_put},
  {"delete", perfrom_delete},
  {"setCURLOption", setCURL_options},
  {"setHeader", set_header},
  {"setBasicAuth", set_basic_auth},
  {"disconnect", disconnect},
  {NULL, NULL}
};

static void
create_matatable (lua_State * L)
{

  luaL_newmetatable (L, SIMPLE_CURL_METATABLE);
  //lua_pushvalue(L, -1);
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
  curl_global_init (CURL_GLOBAL_ALL);
  /* Create module */
  //luaL_newlib (L, functions);

  /* Create metatable */
  create_matatable (L);

  luaL_newlib (L, functions);
  //lua_pop (L, 1);

  return 1;
}
