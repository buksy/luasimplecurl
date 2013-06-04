#include <stdio.h>
#include <stdlib.h>
#include <lua5.2/lua.h>
#include <lua5.2/lauxlib.h>


#define SIMPLE_CURL_METATABLE "70474f43-b594-4a62-bcc0-8239b2da7b0a" 

typedef struct simple_curl{
	CURL *curl;
	lua_State *L;
}sCurl;


static int newconnect(lua_State *L)
{
	const char *url = luaL_checkstring (L,-1);
	if (!url) 
	{
		luaL_error (L, "URL not defined");	
		return 0;
	}

	lua_pop (L, 1);
	sCurl* c = (sCurl*)lua_newuserdata(L, sizeof(sCurl));
	c->L=c->curl=NULL;
	c->L=L;

    // Open the curl Handler
	c->curl = curl_easy_init();

	if (!c->curl) {
		luaL_error (L, "Simple HTTP could not make a new connection")
		return 0;
	}
	// Set up the curl option for the URL to make life simple I am adding follow redirect as well
	int err = 0;
	if ( (err = curl_easy_setopt(c->curl, CURLOPT_URL, url)) != CURLE_OK) 
	{
		luaL_error (L, "Simple HTTP could not make a new connection ( %d - %s)", err , curl_easy_strerror(err) );
		return 0;
	}	

	curl_easy_setopt (c->curl, CURLOPT_FOLLOWLOCATION, 1L);

	// Now set the metatable
	luaL_getmetatable(L, SIMPLE_CURL_METATABLE);
	lua_setmetatable(L, -2); 
		
	return 1;
}


static int gc_curl (lua_State *L)
{

	sCurl* c = (sCurl*)luaL_checkudata(L, 1, SIMPLE_CURL_METATABLE);
	if (c && c->curl)
	{
		curl_easy_cleanup(c->curl);
	}
	return 0
}

static int perform_get (lua_State *L)
{

}

static int perform_post (lua_State *L)
{

}

static int perform_put (lua_State *L)
{

}

static int perfrom_delete (lua_State *L)
{

}

static int setCURL_options (lua_State *L)
{

}

static int set_header (lua_State *L)
{

}

static int set_basic_auth (lua_State *L)
{

}

static void create_matatable (luaL_State *L) {
	
	luaL_newmetatable(L, SIMPLE_CURL_METATABLE);
    lua_pushcfunction(L, gc_curl);
    lua_setfield(L, -2, "__gc");
	
	lua_pushcfunction(L, perform_get);
    lua_setfield(L, -2, "get");

	lua_pushcfunction(L, perform_post);
    lua_setfield(L, -2, "post");

	lua_pushcfunction(L, perform_put);
    lua_setfield(L, -2, "put");

	lua_pushcfunction(L, perfrom_delete);
    lua_setfield(L, -2, "delete");

	lua_pushcfunction(L, setCURL_options);
    lua_setfield(L, -2, "setCURLOption");

	lua_pushcfunction(L, set_header);
    lua_setfield(L, -2, "setHeader");

	lua_pushcfunction(L, set_basic_auth);
    lua_setfield(L, -2, "setBasicAuth");				
}

/**
All the functions supported by library 
**/
static const luaL_Reg functions[] = {
	{"newconnect", newconnect},
	{NULL, NULL}
};

/*
 * Exported functions.
 */ 
 
LUALIB_API int luaopen_simplehttp (lua_State *L) {
	curl_global_init(CURL_GLOBAL_ALL);
	/* Create module */
	luaL_newlib(L, functions);
		
	/* Create metatable */
	create_matatable (L);	

	lua_pop(L, 1);
	
	return 1;
}
