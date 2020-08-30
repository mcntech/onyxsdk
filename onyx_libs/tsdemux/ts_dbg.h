#include <stdio.h>
#include <stdarg.h>

//==========================================================
#define DBGLVL_ERROR    0
#define DBGLVL_WARN	    1
#define DBGLVL_SETUP    2
#define DBGLVL_STAT     3
#define DBGLVL_TRACE    4
#define DBGLVL_FNTRACE  5
#define DBGLVL_FRAME    6
#define DBGLVL_PACKET   7
#define DBGLVL_WAITLOOP 8

static int gDbgLevel = DBGLVL_TRACE;
static void dbg_printf (const char *format, ...)
{
	char buffer[1024 + 1];
	va_list arg;
	va_start (arg, format);
	vsprintf (buffer, format, arg);
	fprintf(stderr, ":%s", buffer);
}

#ifdef NO_DEBUG_LOG
#define DbgOut(DbgLevel, x)
#else
#define DbgOut(DbgLevel, x) do { if(DbgLevel <= gDbgLevel) { \
									fprintf(stderr,"%s:%s:%d:", __FILE__, __FUNCTION__, __LINE__); \
									dbg_printf x;                                                  \
									fprintf(stderr,"\n");                                          \
								}                                                                  \
							} while(0);
#endif