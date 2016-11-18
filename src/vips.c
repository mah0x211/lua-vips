/*
 *  Copyright (C) 2016 Masatoshi Teruya
 *
 *  Permission is hereby granted, free of charge, to any person obtaining a
 *  copy of this software and associated documentation files (the "Software"),
 *  to deal in the Software without restriction, including without limitation
 *  the rights to use, copy, modify, merge, publish, distribute, sublicense,
 *  and/or sell copies of the Software, and to permit persons to whom the
 *  Software is furnished to do so, subject to the following conditions:
 *
 *  The above copyright notice and this permission notice shall be included in
 *  all copies or substantial portions of the Software.
 *
 *  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 *  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 *  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 *  THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 *  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 *  FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 *  DEALINGS IN THE SOFTWARE.
 *
 *
 *  vips.c
 *  lua-vips
 *
 *  Created by Masatoshi Teruya on 16/11/18.
 */

#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <lua.h>
#include <lauxlib.h>
#include "lauxhlib.h"
#include "vips/vips.h"


#define MODULE_MT           "vips"
#define MODULE_IMAGE_MT     "vips.image"

#define DEFAULT_QUALITY     85


typedef struct {
    VipsImage *img;
    uint8_t q;
} lvips_t;


static int save_lua( lua_State *L )
{
    lvips_t *v = luaL_checkudata( L, 1, MODULE_IMAGE_MT );
    const char *pathname = lauxh_checkstring( L, 2 );

    if( vips_jpegsave( v->img, pathname,
                       // quality factor
                       "Q", v->q,
                       // filename of ICC profile to attach
                       "profile", "none",
                       // compute optimal Huffman coding tables
                       "optimize_coding", TRUE,
                       // progressive jpeg
                       "interlace", TRUE,
                       // remove all metadata from image
                       "strip", TRUE,
                    //    // disable chroma subsampling
                    //    "no-subsample", TRUE,
                       NULL ) == 0 ){
        lua_pushboolean( L, 1 );
        return 1;
    }

    // got error
    lua_pushboolean( L, 0 );
    lua_pushstring( L, vips_error_buffer() );
    vips_error_clear();

    return 2;
}


static int quality_lua( lua_State *L )
{
    lvips_t *v = luaL_checkudata( L, 1, MODULE_IMAGE_MT );
    lua_Integer q = lauxh_checkinteger( L, 2 );

    lauxh_argcheck( L, q >= 0 && q <= 100, 2, "0 to 100 expected, got %ld", q );

    // update quality
    v->q = q;

    // return self
    lua_settop( L, 1 );

    return 1;
}


static int tostring_lua( lua_State *L )
{
    lua_pushfstring( L, MODULE_MT ": %p", lua_touserdata( L, 1 ) );
    return 1;
}


static int gc_lua( lua_State *L )
{
    lvips_t *v = luaL_checkudata( L, 1, MODULE_IMAGE_MT );

    if( v->img ){
        VIPS_UNREF( v->img );
    }

    return 0;
}


static int newfromfile_lua( lua_State *L )
{
    const char *pathname = lauxh_checkstring( L, 1 );
    lvips_t *v = lua_newuserdata( L, sizeof( lvips_t ) );

    // mem error
    if( !v ){
        lua_pushnil( L );
        lua_pushstring( L, strerror( errno ) );
        return 2;
    }

    v->img = vips_image_new_from_file( pathname, "access",
                                       VIPS_ACCESS_SEQUENTIAL, NULL );
    if( v->img ){
        lauxh_setmetatable( L, MODULE_IMAGE_MT );
        v->q = DEFAULT_QUALITY;
        return 1;
    }

    // got error
    lua_pushnil( L );
    lua_pushstring( L, vips_error_buffer() );
    vips_error_clear();

    return 2;
}


static int gc_module( lua_State *L )
{
    vips_shutdown();
    return 0;
}


static int init_module( lua_State *L )
{
    // init vips global
    if( VIPS_INIT( MODULE_MT ) ){
        return luaL_error( L, "failed to VIPS_INIT()" );
    }

    // create metatable
    luaL_newmetatable( L, MODULE_MT );
    lauxh_pushfn2tbl( L, "__gc", gc_module );
    // remove metatable from stack
    lua_pop( L, 1 );

    // create module data
    if( !lua_newuserdata( L, sizeof(0) ) ){
        return luaL_error( L, "failed to create lua_newuserdata" );
    }
    lauxh_setmetatable( L, MODULE_MT );
    lauxh_ref( L );

    return 0;
}


LUALIB_API int luaopen_vips( lua_State *L )
{
    struct luaL_Reg mmethods[] = {
        { "__gc", gc_lua },
        { "__tostring", tostring_lua },
        { NULL, NULL }
    };
    struct luaL_Reg methods[] = {
        { "quality", quality_lua },
        { "save", save_lua },
        { NULL, NULL }
    };
    struct luaL_Reg *ptr = mmethods;

    // init vips global
    if( init_module( L ) == -1 ){
        return 0;
    }

    // create metatable
    luaL_newmetatable( L, MODULE_IMAGE_MT );
    // metamethods
    while( ptr->name ){
        lauxh_pushfn2tbl( L, ptr->name, ptr->func );
        ptr++;
    }
    // methods
    ptr = methods;
    lua_pushstring( L, "__index" );
    lua_newtable( L );
    while( ptr->name ){
        lauxh_pushfn2tbl( L, ptr->name, ptr->func );
        ptr++;
    }
    lua_rawset( L, -3 );
    // remove metatable from stack
    lua_pop( L, 1 );

    // register allocator
    lua_createtable( L, 0, 1 );
    lauxh_pushfn2tbl( L, "newFromFile", newfromfile_lua );

    return 1;
}


