/*=========================================================================*\
* Serial stream
* LuaSocket toolkit
\*=========================================================================*/
#include <string.h>

#include "lua.h"
#include "lauxlib.h"

#include "auxiliar.h"
#include "socket.h"
#include "options.h"
#include "unix.h"
#include <sys/un.h>

#include "fcntl.h"      /* file control definitions */
#include "termios.h"    /* POSIX terminal control definitions */
#include "stdbool.h"

/*
Reuses userdata definition from unix.h, since it is useful for all
stream-like objects.

If we stored the serial path for use in error messages or userdata
printing, we might need our own userdata definition.

Group usage is semi-inherited from unix.c, but unnecessary since we
have only one object type.
*/

/*=========================================================================*\
* Internal function prototypes
\*=========================================================================*/
static int global_create(lua_State *L);
static int meth_options(lua_State *L);
static int meth_send(lua_State *L);
static int meth_receive(lua_State *L);
static int meth_close(lua_State *L);
static int meth_settimeout(lua_State *L);
static int meth_getfd(lua_State *L);
static int meth_setfd(lua_State *L);
static int meth_dirty(lua_State *L);
static int meth_getstats(lua_State *L);
static int meth_setstats(lua_State *L);

/* serial object methods */
static luaL_Reg serial_methods[] = {
    {"__gc",        meth_close},
    {"__tostring",  auxiliar_tostring},
    {"close",       meth_close},
    {"dirty",       meth_dirty},
    {"getfd",       meth_getfd},
    {"getstats",    meth_getstats},
    {"setstats",    meth_setstats},
    {"receive",     meth_receive},
    {"send",        meth_send},
    {"setfd",       meth_setfd},
    {"settimeout",  meth_settimeout},
    {"options",     meth_options},
    {NULL,          NULL}
};

/* our socket creation function */
static luaL_Reg func[] = {
    {"serial",      global_create},
    {NULL,          NULL}
};


/*-------------------------------------------------------------------------*\
* Initializes module
\*-------------------------------------------------------------------------*/
LUASOCKET_API int luaopen_socket_serial(lua_State *L) {
    /* create classes */
    auxiliar_newclass(L, "serial{client}", serial_methods);
    /* create class groups */
    auxiliar_add2group(L, "serial{client}", "serial{any}");
    /* make sure the function ends up in the package table */
    luaL_openlib(L, "socket", func, 0);
    /* return the function instead of the 'socket' table */
    lua_pushstring(L, "serial");
    lua_gettable(L, -2);
    return 1;
}

/*=========================================================================*\
* Lua methods
\*=========================================================================*/
/*-------------------------------------------------------------------------*\
* Just call buffered IO methods
\*-------------------------------------------------------------------------*/
static int meth_send(lua_State *L) {
    p_unix un = (p_unix) auxiliar_checkclass(L, "serial{client}", 1);
    return buffer_meth_send(L, &un->buf);
}

static int meth_receive(lua_State *L) {
    p_unix un = (p_unix) auxiliar_checkclass(L, "serial{client}", 1);
    return buffer_meth_receive(L, &un->buf);
}

static int meth_getstats(lua_State *L) {
    p_unix un = (p_unix) auxiliar_checkclass(L, "serial{client}", 1);
    return buffer_meth_getstats(L, &un->buf);
}

static int meth_setstats(lua_State *L) {
    p_unix un = (p_unix) auxiliar_checkclass(L, "serial{client}", 1);
    return buffer_meth_setstats(L, &un->buf);
}

/*-------------------------------------------------------------------------*\
* Select support methods
\*-------------------------------------------------------------------------*/
static int meth_getfd(lua_State *L) {
    p_unix un = (p_unix) auxiliar_checkgroup(L, "serial{any}", 1);
    lua_pushnumber(L, (int) un->sock);
    return 1;
}

/* this is very dangerous, but can be handy for those that are brave enough */
static int meth_setfd(lua_State *L) {
    p_unix un = (p_unix) auxiliar_checkgroup(L, "serial{any}", 1);
    un->sock = (t_socket) luaL_checknumber(L, 2);
    return 0;
}

static int meth_dirty(lua_State *L) {
    p_unix un = (p_unix) auxiliar_checkgroup(L, "serial{any}", 1);
    lua_pushboolean(L, !buffer_isempty(&un->buf));
    return 1;
}

/*-------------------------------------------------------------------------*\
* Closes socket used by object
\*-------------------------------------------------------------------------*/
static int meth_close(lua_State *L)
{
    p_unix un = (p_unix) auxiliar_checkgroup(L, "serial{any}", 1);
    socket_destroy(&un->sock);
    lua_pushnumber(L, 1);
    return 1;
}


/*-------------------------------------------------------------------------*\
* Just call tm methods
\*-------------------------------------------------------------------------*/
static int meth_settimeout(lua_State *L) {
    p_unix un = (p_unix) auxiliar_checkgroup(L, "serial{any}", 1);
    return timeout_meth_settimeout(L, &un->tm);
}

/*=========================================================================*\
* Library functions
\*=========================================================================*/

/*  Get a parameter from a lua table, convert to boolean and set to termios option structure. */
#define SET_FLAG_IN_TERMIOS_STRUCT(L, options, tab_ind, flag_type, key_str, constant)                                       \
do {                                                                                                                        \
    lua_pushstring((L), (key_str));                                                                                         \
    lua_gettable((L), (tab_ind));                                                                                           \
    if (!lua_isnoneornil((L), -1)) {                                                                                        \
        if (lua_isboolean((L), -1)) {                                                                                       \
            if (lua_toboolean((L), -1)) {                                                                                   \
                (options)->flag_type |= (constant);                                                                         \
            } else {                                                                                                        \
                (options)->flag_type &= ~(constant);                                                                        \
            };                                                                                                              \
        } else {                                                                                                            \
            luaL_error(L, "Option flags must be of type boolean; flag %s is of type %s", (key_str), luaL_typename(L, -1));  \
        };                                                                                                                  \
    };                                                                                                                      \
    lua_pop((L), 1);                                                                                                        \
} while (0)

/*  Get a parameter from a termios option struct and add it to a lua table. */
#define GET_FLAG_FROM_TERMIOS_STRUCT(L, options, tab_ind, flag_type, key_str, constant)                                     \
do {                                                                                                                        \
    lua_pushboolean((L), (options)->flag_type && (constant));                                                               \
    lua_setfield((L), (tab_ind), (key_str));                                                                                \
} while (0)

/*  Debug info printout helper macro/function.

/ *  Print a parameter from a termios option struct. * /
#define PRINT_FLAG_FROM_TERMIOS_STRUCT(L, options, flag_type, key_str, constant)                                            \
do {                                                                                                                        \
    printf("%s:\t%s\n", (key_str), ((options)->flag_type && (constant))?"true":"false");                                    \
} while (0)

static void print_termios(struct termios *options) {
    int csize;
    if          ((*options).c_cflag && CS5) {
        csize = 5;
    } else if   ((*options).c_cflag && CS6) {
        csize = 6;
    } else if   ((*options).c_cflag && CS7) {
        csize = 7;
    } else if   ((*options).c_cflag && CS8) {
        csize = 8;
    };
    printf("Printing struct termios %p\nospeed:\t%i\nispeed:\t%i\ncsize:\t%i\n",
            options, cfgetospeed(options), cfgetispeed(options), csize);
    / *  Start with the control flags * /
    PRINT_FLAG_FROM_TERMIOS_STRUCT(L, options, c_cflag, "clocal",   CLOCAL);
    PRINT_FLAG_FROM_TERMIOS_STRUCT(L, options, c_cflag, "cread",    CREAD);
    PRINT_FLAG_FROM_TERMIOS_STRUCT(L, options, c_cflag, "parenb",   PARENB);
    PRINT_FLAG_FROM_TERMIOS_STRUCT(L, options, c_cflag, "cstopb",   CSTOPB);
    PRINT_FLAG_FROM_TERMIOS_STRUCT(L, options, c_cflag, "parodd",   PARODD);
    PRINT_FLAG_FROM_TERMIOS_STRUCT(L, options, c_cflag, "hupcl",    HUPCL);
    / *  Now handle the local flags * /
    PRINT_FLAG_FROM_TERMIOS_STRUCT(L, options, c_lflag, "icanon",   ICANON);
    PRINT_FLAG_FROM_TERMIOS_STRUCT(L, options, c_lflag, "echo",     ECHO);
    PRINT_FLAG_FROM_TERMIOS_STRUCT(L, options, c_lflag, "echoe",    ECHOE);
    PRINT_FLAG_FROM_TERMIOS_STRUCT(L, options, c_lflag, "echok",    ECHOK);
    PRINT_FLAG_FROM_TERMIOS_STRUCT(L, options, c_lflag, "echonl",   ECHONL);
    PRINT_FLAG_FROM_TERMIOS_STRUCT(L, options, c_lflag, "isig",     ISIG);
    PRINT_FLAG_FROM_TERMIOS_STRUCT(L, options, c_lflag, "tostop",   TOSTOP);
    PRINT_FLAG_FROM_TERMIOS_STRUCT(L, options, c_lflag, "iexten",   IEXTEN);
    / *  Input flags are next * /
    PRINT_FLAG_FROM_TERMIOS_STRUCT(L, options, c_iflag, "ignbrk",   IGNBRK);
    PRINT_FLAG_FROM_TERMIOS_STRUCT(L, options, c_iflag, "brkint",   BRKINT);
    PRINT_FLAG_FROM_TERMIOS_STRUCT(L, options, c_iflag, "ignpar",   IGNPAR);
    PRINT_FLAG_FROM_TERMIOS_STRUCT(L, options, c_iflag, "parmrk",   PARMRK);
    PRINT_FLAG_FROM_TERMIOS_STRUCT(L, options, c_iflag, "inpck",    INPCK);
    PRINT_FLAG_FROM_TERMIOS_STRUCT(L, options, c_iflag, "istrip",   ISTRIP);
    PRINT_FLAG_FROM_TERMIOS_STRUCT(L, options, c_iflag, "inlcr",    INLCR);
    PRINT_FLAG_FROM_TERMIOS_STRUCT(L, options, c_iflag, "igncr",    IGNCR);
    PRINT_FLAG_FROM_TERMIOS_STRUCT(L, options, c_iflag, "icrnl",    ICRNL);
    PRINT_FLAG_FROM_TERMIOS_STRUCT(L, options, c_iflag, "ixon",     IXON);
    PRINT_FLAG_FROM_TERMIOS_STRUCT(L, options, c_iflag, "ixany",    IXANY);
    PRINT_FLAG_FROM_TERMIOS_STRUCT(L, options, c_iflag, "ixoff",    IXOFF);
    / *  Finally get the output flags * /
    PRINT_FLAG_FROM_TERMIOS_STRUCT(L, options, c_oflag, "opost",    OPOST);
    PRINT_FLAG_FROM_TERMIOS_STRUCT(L, options, c_oflag, "onlcr",    ONLCR);
    PRINT_FLAG_FROM_TERMIOS_STRUCT(L, options, c_oflag, "ocrnl",    OCRNL);
    PRINT_FLAG_FROM_TERMIOS_STRUCT(L, options, c_oflag, "onocr",    ONOCR);
    PRINT_FLAG_FROM_TERMIOS_STRUCT(L, options, c_oflag, "onlret",   ONLRET);
    PRINT_FLAG_FROM_TERMIOS_STRUCT(L, options, c_oflag, "ofill",    OFILL);
}*/


/*  Utility function; takes a lua table at the top of the stack and converts it to a termios option control structure. */
static void set_termios(lua_State *L, struct termios *options, int tab_ind) {
    /*  Check if we need to reset the flags */
    lua_getfield(L, tab_ind, "reset_flags_first");
    if (!lua_isnoneornil(L, -1)) {
        if (lua_isboolean(L, -1)) {
            if (lua_toboolean(L, -1)) {
                (*options).c_cflag = 0;
                (*options).c_lflag = 0;
                (*options).c_oflag = 0;
                (*options).c_iflag = 0;
            };
        } else {
            luaL_error(L, "The reset_flags_first option must be a boolean; is of type %s", luaL_typename(L, -1));
        };
    };
    lua_pop((L), 1);
    /* input speed (baud rate) */
    lua_getfield(L, tab_ind, "ispeed");
    if (!lua_isnoneornil(L, -1)) {
        if (lua_isnumber(L, -1)) {
            int ispeed = lua_tointeger(L, -1);
            switch (ispeed) {
                case 0:         cfsetispeed(options, B0);       break;
                case 50:        cfsetispeed(options, B50);      break;
                case 75:        cfsetispeed(options, B75);      break;
                case 110:       cfsetispeed(options, B110);     break;
                case 134:       cfsetispeed(options, B134);     break;
                case 150:       cfsetispeed(options, B150);     break;
                case 200:       cfsetispeed(options, B200);     break;
                case 300:       cfsetispeed(options, B300);     break;
                case 600:       cfsetispeed(options, B600);     break;
                case 1200:      cfsetispeed(options, B1200);    break;
                case 1800:      cfsetispeed(options, B1800);    break;
                case 2400:      cfsetispeed(options, B2400);    break;
                case 4800:      cfsetispeed(options, B4800);    break;
                case 9600:      cfsetispeed(options, B9600);    break;
                case 19200:     cfsetispeed(options, B19200);   break;
                case 38400:     cfsetispeed(options, B38400);   break;
                case 57600:     cfsetispeed(options, B57600);   break;
                case 115200:    cfsetispeed(options, B115200);  break;
                case 230400:    cfsetispeed(options, B230400);  break;
                default:
                    luaL_error(L, "Baud rate (for ispeed) must be one of 0, 50, 75, 110, 134, 150, 200, 300, 600, 1200, 1800, 2400, 4800, 9600, 19200, 38400, 57600, 115200 or 230400");
                    break;
            };
        } else {
            luaL_error(L, "Baud rates must be of type number; flag ispeed is of type %s", luaL_typename(L, -1));
        };
    };
    lua_pop((L), 1);
    /* output speed (baud rate) */
    lua_getfield(L, tab_ind, "ospeed");
    if (!lua_isnoneornil(L, -1)) {
        if (lua_isnumber(L, -1)) {
            int ospeed = lua_tointeger(L, -1);
            switch (ospeed) {
                case 0:         cfsetospeed(options, B0);       break;
                case 50:        cfsetospeed(options, B50);      break;
                case 75:        cfsetospeed(options, B75);      break;
                case 110:       cfsetospeed(options, B110);     break;
                case 134:       cfsetospeed(options, B134);     break;
                case 150:       cfsetospeed(options, B150);     break;
                case 200:       cfsetospeed(options, B200);     break;
                case 300:       cfsetospeed(options, B300);     break;
                case 600:       cfsetospeed(options, B600);     break;
                case 1200:      cfsetospeed(options, B1200);    break;
                case 1800:      cfsetospeed(options, B1800);    break;
                case 2400:      cfsetospeed(options, B2400);    break;
                case 4800:      cfsetospeed(options, B4800);    break;
                case 9600:      cfsetospeed(options, B9600);    break;
                case 19200:     cfsetospeed(options, B19200);   break;
                case 38400:     cfsetospeed(options, B38400);   break;
                case 57600:     cfsetospeed(options, B57600);   break;
                case 115200:    cfsetospeed(options, B115200);  break;
                case 230400:    cfsetospeed(options, B230400);  break;
                default:
                    luaL_error(L, "Baud rate (for ospeed) must be one of 0, 50, 75, 110, 134, 150, 200, 300, 600, 1200, 1800, 2400, 4800, 9600, 19200, 38400, 57600, 115200 or 230400");
                    break;
            };
        } else {
            luaL_error(L, "Baud rates must be of type number; flag ospeed is of type %s", luaL_typename(L, -1));
        };
    };
    lua_pop((L), 1);
    /*  Check for csize */
    lua_getfield(L, tab_ind, "csize");
    /*  printf("type of csize's value: %s\n", luaL_typename(L, -1));*/
    if (!lua_isnoneornil(L, -1)) {
        /*  printf("csize: %f\n", lua_tonumber(L, -1));*/
        if (lua_isnumber(L, -1)) {
            /*  printf("csize: %i\n", (int) lua_tointeger(L, -1));*/
            switch  (lua_tointeger(L, -1)) {
                case 5:
                    /*  printf("setting csize to 5\n");*/
                    (*options).c_cflag &= ~CSIZE;
                    (*options).c_cflag |= CS5;
                    break;
                case 6:
                    /*  printf("setting csize to 6\n");*/
                    (*options).c_cflag &= ~CSIZE;
                    (*options).c_cflag |= CS6;
                    break;
                case 7:
                    /*  printf("setting csize to 7\n");*/
                    (*options).c_cflag &= ~CSIZE;
                    (*options).c_cflag |= CS7;
                    break;
                case 8:
                    /*  printf("setting csize to 8\n");*/
                    (*options).c_cflag &= ~CSIZE;
                    (*options).c_cflag |= CS8;
                    /*  printf("csize set to 8? %s\n", ((*options).c_cflag && CS8)?"yes":"no");*/
                    break;
                default:
                    luaL_error(L, "The character size mask (csize) must be one of 5, 6, 7 or 8");
            };
        } else {
            luaL_error(L, "The character size mask (csize) must be of type number; is of type %s", luaL_typename(L, -1));
        };
    };
    lua_pop((L), 1);
    /*  Start with the control flags */
    SET_FLAG_IN_TERMIOS_STRUCT(L, options, tab_ind, c_cflag, "clocal",   CLOCAL);
    SET_FLAG_IN_TERMIOS_STRUCT(L, options, tab_ind, c_cflag, "cread",    CREAD);
    SET_FLAG_IN_TERMIOS_STRUCT(L, options, tab_ind, c_cflag, "parenb",   PARENB);
    SET_FLAG_IN_TERMIOS_STRUCT(L, options, tab_ind, c_cflag, "cstopb",   CSTOPB);
    SET_FLAG_IN_TERMIOS_STRUCT(L, options, tab_ind, c_cflag, "parodd",   PARODD);
    SET_FLAG_IN_TERMIOS_STRUCT(L, options, tab_ind, c_cflag, "hupcl",    HUPCL);
    /*  Now handle the local flags */
    SET_FLAG_IN_TERMIOS_STRUCT(L, options, tab_ind, c_lflag, "icanon",   ICANON);
    SET_FLAG_IN_TERMIOS_STRUCT(L, options, tab_ind, c_lflag, "echo",     ECHO);
    SET_FLAG_IN_TERMIOS_STRUCT(L, options, tab_ind, c_lflag, "echoe",    ECHOE);
    SET_FLAG_IN_TERMIOS_STRUCT(L, options, tab_ind, c_lflag, "echok",    ECHOK);
    SET_FLAG_IN_TERMIOS_STRUCT(L, options, tab_ind, c_lflag, "echonl",   ECHONL);
    SET_FLAG_IN_TERMIOS_STRUCT(L, options, tab_ind, c_lflag, "isig",     ISIG);
    SET_FLAG_IN_TERMIOS_STRUCT(L, options, tab_ind, c_lflag, "tostop",   TOSTOP);
    SET_FLAG_IN_TERMIOS_STRUCT(L, options, tab_ind, c_lflag, "iexten",   IEXTEN);
    /*  Input flags are next */
    SET_FLAG_IN_TERMIOS_STRUCT(L, options, tab_ind, c_iflag, "ignbrk",   IGNBRK);
    SET_FLAG_IN_TERMIOS_STRUCT(L, options, tab_ind, c_iflag, "brkint",   BRKINT);
    SET_FLAG_IN_TERMIOS_STRUCT(L, options, tab_ind, c_iflag, "ignpar",   IGNPAR);
    SET_FLAG_IN_TERMIOS_STRUCT(L, options, tab_ind, c_iflag, "parmrk",   PARMRK);
    SET_FLAG_IN_TERMIOS_STRUCT(L, options, tab_ind, c_iflag, "inpck",    INPCK);
    SET_FLAG_IN_TERMIOS_STRUCT(L, options, tab_ind, c_iflag, "istrip",   ISTRIP);
    SET_FLAG_IN_TERMIOS_STRUCT(L, options, tab_ind, c_iflag, "inlcr",    INLCR);
    SET_FLAG_IN_TERMIOS_STRUCT(L, options, tab_ind, c_iflag, "igncr",    IGNCR);
    SET_FLAG_IN_TERMIOS_STRUCT(L, options, tab_ind, c_iflag, "icrnl",    ICRNL);
    SET_FLAG_IN_TERMIOS_STRUCT(L, options, tab_ind, c_iflag, "ixon",     IXON);
    SET_FLAG_IN_TERMIOS_STRUCT(L, options, tab_ind, c_iflag, "ixany",    IXANY);
    SET_FLAG_IN_TERMIOS_STRUCT(L, options, tab_ind, c_iflag, "ixoff",    IXOFF);
    /*  Finally set the output flags */
    SET_FLAG_IN_TERMIOS_STRUCT(L, options, tab_ind, c_oflag, "opost",    OPOST);
    SET_FLAG_IN_TERMIOS_STRUCT(L, options, tab_ind, c_oflag, "onlcr",    ONLCR);
    SET_FLAG_IN_TERMIOS_STRUCT(L, options, tab_ind, c_oflag, "ocrnl",    OCRNL);
    SET_FLAG_IN_TERMIOS_STRUCT(L, options, tab_ind, c_oflag, "onocr",    ONOCR);
    SET_FLAG_IN_TERMIOS_STRUCT(L, options, tab_ind, c_oflag, "onlret",   ONLRET);
    SET_FLAG_IN_TERMIOS_STRUCT(L, options, tab_ind, c_oflag, "ofill",    OFILL);
}

#define LUA_TCSANOW     0
#define LUA_TCSADRAIN   1
#define LUA_TCSAFLUSH   2

static const char *const tcsetattr_speed_options[] = {
    "now",
    "drain",
    "flush",
    NULL
};

static int meth_options(lua_State *L) {
    p_unix un = (p_unix) auxiliar_checkgroup(L, "serial{any}", 1);
    struct termios options;
    tcgetattr(un->sock, &options);
    if (!lua_isnoneornil(L, 2)) {
        if (lua_istable(L, 2)) {
            int when = TCSANOW;
            if (!lua_isnoneornil(L, 3)) {
                switch (luaL_checkoption(L, 3, NULL, tcsetattr_speed_options)) {
                    case LUA_TCSANOW:       when = TCSANOW;         break;
                    case LUA_TCSADRAIN:     when = TCSADRAIN;       break;
                    case LUA_TCSAFLUSH:     when = TCSAFLUSH;       break;
                }
                lua_pop(L, 1);
            };
            set_termios(L, &options, 2);
            /* set options */
            tcsetattr(un->sock, when, &options);
        } else {
            return luaL_argerror(L, 2, "Please pass a table or nil");
        };
        lua_pop(L, 1);
    };
    lua_pop(L, 1);
    lua_createtable(L, 0, 37);
    switch (cfgetospeed(&options)) {
        case B0:        lua_pushnumber( L,  0);         break;
        case B50:       lua_pushnumber( L,  50);        break;
        case B75:       lua_pushnumber( L,  75);        break;
        case B110:      lua_pushnumber( L,  110);       break;
        case B134:      lua_pushnumber( L,  134);       break;
        case B150:      lua_pushnumber( L,  150);       break;
        case B200:      lua_pushnumber( L,  200);       break;
        case B300:      lua_pushnumber( L,  300);       break;
        case B600:      lua_pushnumber( L,  600);       break;
        case B1200:     lua_pushnumber( L,  1200);      break;
        case B1800:     lua_pushnumber( L,  1800);      break;
        case B2400:     lua_pushnumber( L,  2400);      break;
        case B4800:     lua_pushnumber( L,  4800);      break;
        case B9600:     lua_pushnumber( L,  9600);      break;
        case B19200:    lua_pushnumber( L,  19200);     break;
        case B38400:    lua_pushnumber( L,  38400);     break;
        case B57600:    lua_pushnumber( L,  57600);     break;
        case B115200:   lua_pushnumber( L,  115200);    break;
        case B230400:   lua_pushnumber( L,  230400);    break;
        default:        lua_pushliteral(L,  "unknown"); break;
    };
    lua_setfield(L, 1, "ospeed");
    switch (cfgetispeed(&options)) {
        case B0:        lua_pushnumber( L,  0);         break;
        case B50:       lua_pushnumber( L,  50);        break;
        case B75:       lua_pushnumber( L,  75);        break;
        case B110:      lua_pushnumber( L,  110);       break;
        case B134:      lua_pushnumber( L,  134);       break;
        case B150:      lua_pushnumber( L,  150);       break;
        case B200:      lua_pushnumber( L,  200);       break;
        case B300:      lua_pushnumber( L,  300);       break;
        case B600:      lua_pushnumber( L,  600);       break;
        case B1200:     lua_pushnumber( L,  1200);      break;
        case B1800:     lua_pushnumber( L,  1800);      break;
        case B2400:     lua_pushnumber( L,  2400);      break;
        case B4800:     lua_pushnumber( L,  4800);      break;
        case B9600:     lua_pushnumber( L,  9600);      break;
        case B19200:    lua_pushnumber( L,  19200);     break;
        case B38400:    lua_pushnumber( L,  38400);     break;
        case B57600:    lua_pushnumber( L,  57600);     break;
        case B115200:   lua_pushnumber( L,  115200);    break;
        case B230400:   lua_pushnumber( L,  230400);    break;
        default:        lua_pushliteral(L,  "unknown"); break;
    };
    lua_setfield(L, 1, "ispeed");
    if          (options.c_cflag && CS5) {
        lua_pushnumber(L, 5);
        lua_setfield(L, 1, "csize");
    } else if   (options.c_cflag && CS6) {
        lua_pushnumber(L, 6);
        lua_setfield(L, 1, "csize");
    } else if   (options.c_cflag && CS7) {
        lua_pushnumber(L, 7);
        lua_setfield(L, 1, "csize");
    } else if   (options.c_cflag && CS8) {
        lua_pushnumber(L, 8);
        lua_setfield(L, 1, "csize");
    };
    /*  Start with the control flags */
    GET_FLAG_FROM_TERMIOS_STRUCT(L, (&options), 1, c_cflag, "clocal",   CLOCAL);
    GET_FLAG_FROM_TERMIOS_STRUCT(L, (&options), 1, c_cflag, "cread",    CREAD);
    GET_FLAG_FROM_TERMIOS_STRUCT(L, (&options), 1, c_cflag, "parenb",   PARENB);
    GET_FLAG_FROM_TERMIOS_STRUCT(L, (&options), 1, c_cflag, "cstopb",   CSTOPB);
    GET_FLAG_FROM_TERMIOS_STRUCT(L, (&options), 1, c_cflag, "parodd",   PARODD);
    GET_FLAG_FROM_TERMIOS_STRUCT(L, (&options), 1, c_cflag, "hupcl",    HUPCL);
    /*  Now handle the local flags */
    GET_FLAG_FROM_TERMIOS_STRUCT(L, (&options), 1, c_lflag, "icanon",   ICANON);
    GET_FLAG_FROM_TERMIOS_STRUCT(L, (&options), 1, c_lflag, "echo",     ECHO);
    GET_FLAG_FROM_TERMIOS_STRUCT(L, (&options), 1, c_lflag, "echoe",    ECHOE);
    GET_FLAG_FROM_TERMIOS_STRUCT(L, (&options), 1, c_lflag, "echok",    ECHOK);
    GET_FLAG_FROM_TERMIOS_STRUCT(L, (&options), 1, c_lflag, "echonl",   ECHONL);
    GET_FLAG_FROM_TERMIOS_STRUCT(L, (&options), 1, c_lflag, "isig",     ISIG);
    GET_FLAG_FROM_TERMIOS_STRUCT(L, (&options), 1, c_lflag, "tostop",   TOSTOP);
    GET_FLAG_FROM_TERMIOS_STRUCT(L, (&options), 1, c_lflag, "iexten",   IEXTEN);
    /*  Input flags are next */
    GET_FLAG_FROM_TERMIOS_STRUCT(L, (&options), 1, c_iflag, "ignbrk",   IGNBRK);
    GET_FLAG_FROM_TERMIOS_STRUCT(L, (&options), 1, c_iflag, "brkint",   BRKINT);
    GET_FLAG_FROM_TERMIOS_STRUCT(L, (&options), 1, c_iflag, "ignpar",   IGNPAR);
    GET_FLAG_FROM_TERMIOS_STRUCT(L, (&options), 1, c_iflag, "parmrk",   PARMRK);
    GET_FLAG_FROM_TERMIOS_STRUCT(L, (&options), 1, c_iflag, "inpck",    INPCK);
    GET_FLAG_FROM_TERMIOS_STRUCT(L, (&options), 1, c_iflag, "istrip",   ISTRIP);
    GET_FLAG_FROM_TERMIOS_STRUCT(L, (&options), 1, c_iflag, "inlcr",    INLCR);
    GET_FLAG_FROM_TERMIOS_STRUCT(L, (&options), 1, c_iflag, "igncr",    IGNCR);
    GET_FLAG_FROM_TERMIOS_STRUCT(L, (&options), 1, c_iflag, "icrnl",    ICRNL);
    GET_FLAG_FROM_TERMIOS_STRUCT(L, (&options), 1, c_iflag, "ixon",     IXON);
    GET_FLAG_FROM_TERMIOS_STRUCT(L, (&options), 1, c_iflag, "ixany",    IXANY);
    GET_FLAG_FROM_TERMIOS_STRUCT(L, (&options), 1, c_iflag, "ixoff",    IXOFF);
    /*  Finally get the output flags */
    GET_FLAG_FROM_TERMIOS_STRUCT(L, (&options), 1, c_oflag, "opost",    OPOST);
    GET_FLAG_FROM_TERMIOS_STRUCT(L, (&options), 1, c_oflag, "onlcr",    ONLCR);
    GET_FLAG_FROM_TERMIOS_STRUCT(L, (&options), 1, c_oflag, "ocrnl",    OCRNL);
    GET_FLAG_FROM_TERMIOS_STRUCT(L, (&options), 1, c_oflag, "onocr",    ONOCR);
    GET_FLAG_FROM_TERMIOS_STRUCT(L, (&options), 1, c_oflag, "onlret",   ONLRET);
    GET_FLAG_FROM_TERMIOS_STRUCT(L, (&options), 1, c_oflag, "ofill",    OFILL);
    return 1;
}

/*-------------------------------------------------------------------------*\
* Creates a serial object
\*-------------------------------------------------------------------------*/
static int global_create(lua_State *L) {
    bool rd = false;
    bool wr = false;
    const char* path    = luaL_checkstring(L, 1);
    const char* mode    = luaL_checkstring(L, 2);
    int fcntl_ret = 0;
    struct termios options;
    p_unix un;
    t_socket sock;
    int i = 0;
    char m;
    do {
        m = mode[i++];
        switch (m) {
            case 'r':
                rd = true;
                break;
            case 'w':
                wr = true;
                break;
        }
    } while (m != '\0' && (!rd || !wr));
    luaL_argcheck(L, (rd || wr), 2, "Please specify at least read or write mode");
    luaL_argcheck(L, (lua_istable(L, 3) || lua_isnoneornil(L, 3)), 3, "Please pass a table or a nil");

    /* printf("read, write: %i, %i\n", rd, wr);*/

    /* allocate unix object */
    un = (p_unix) lua_newuserdata(L, sizeof(t_unix));

    /* open serial device */
    sock = open(path, ((rd && wr)?O_RDWR:((rd)?O_RDONLY:O_WRONLY)) | O_NOCTTY | O_NDELAY);

    /*printf("open %s on %d\n", path, sock);*/

    if (sock < 0)  {
        lua_pushnil(L);
        lua_pushstring(L, socket_strerror(errno));
        lua_pushnumber(L, errno);
        return 3;
    }
    /* set its type as client object */
    auxiliar_setclass(L, "serial{client}", -1);

    lua_replace(L, 1);      /*  Put our userdata pointer at the bottom of the stack, removing the path string. */
    lua_remove(L, 2);       /*  Remove the mode string, optionally putting the optional options table at the second and top stack position. */

    /* clear options on the file descriptor */
    fcntl_ret = fcntl(sock, F_SETFL, 0);
    if(fcntl_ret < 0) {
        if (!lua_isnoneornil(L, 2)) {
            lua_pop(L, 1);
        };
        lua_pop(L, 1);
        lua_pushnil(L);
        lua_pushstring(L, socket_strerror(fcntl_ret));
        lua_pushnumber(L, fcntl_ret);
        return 3;
    }

    /* we want non-blocking I/O */
    fcntl_ret = fcntl(sock, F_SETFL, FNDELAY);
    if(fcntl_ret < 0) {
        if (!lua_isnoneornil(L, 1)) {
            lua_pop(L, 1);
        };
        lua_pop(L, 1);
        lua_pushnil(L);
        lua_pushstring(L, socket_strerror(fcntl_ret));
        lua_pushnumber(L, fcntl_ret);
        return 3;
    }

    /*  We don't want to share the file descriptor with any children we fork. */
    fcntl(sock, F_SETFD, FD_CLOEXEC);

    /*  gather the options */
    if (lua_istable(L, 2)) {
        int success;
        tcgetattr(sock, &options);
        set_termios(L, &options, 2);
        /* set options */
        success = tcsetattr(sock, TCSANOW, &options);
        if (success < 0) {
            lua_pop(L, 2);
            lua_pushnil(L);
            lua_pushliteral(L, "tcsetattr could not perform any of the requested operations");
            return 2;
        };
    };

    if (!lua_isnoneornil(L, 1)) {
        lua_pop(L, 1);
    };

    /* initialize remaining structure fields */
    socket_setnonblocking(&sock);
    un->sock = sock;
    io_init(&un->io, (p_send) socket_write, (p_recv) socket_read,
            (p_error) socket_ioerror, &un->sock);
    timeout_init(&un->tm, -1, -1);
    buffer_init(&un->buf, &un->io, &un->tm);
    return 1;
}
