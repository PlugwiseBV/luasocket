#ifndef SERIAL_H
#define SERIAL_H
/*=========================================================================*\
* Serial object
* LuaSocket toolkit
*
* This module is just an example of how to extend LuaSocket with a new 
* domain.
\*=========================================================================*/
#include "lua.h"

#include "buffer.h"
#include "timeout.h"
#include "socket.h"
#include "unix.h"

/*
 * Reuses userdata definition from unix.h, since it is useful for all
 * stream-like objects.
 *  
 * If we stored the serial path for use in error messages or userdata
 * printing, we might need our own userdata definition.
 */

typedef t_unix t_serial;
typedef p_unix p_serial;

int serial_open(lua_State *L);

#endif /* SERIAL_H */
