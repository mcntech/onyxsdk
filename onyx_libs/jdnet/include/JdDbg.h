/*
 *  JdDbgLog.h
 *
 *  Copyright 2010 MCN Technologies Inc.. All rights reserved.
 *
 */
#ifndef __JD_DBG_H__
#define __JD_DBG_H__

#include <string>
#include <vector>

#if (defined(DEBUG) || defined(FORCE_DBG_MSG))
#define JdDbg(level, _x_) if((level) <= mJdDbg.mDbgLvl) { mJdDbg.Out _x_; }
#else
#define JdDbg(level, _x_)
#endif
#include <stdio.h>
#include <stdarg.h>


#ifdef NO_DEBUG_LOG
#define JDBG_LOG(DbgLevel, x)
#else
#define JDBG_LOG(DbgLevel, x) do { if(DbgLevel <= modDbgLevel) { \
									fprintf(stderr,"%s:%s:%d:", __FILE__, __FUNCTION__, __LINE__); \
									CJdDbg::dbg_printf x;                                          \
									fprintf(stderr,"\n");                                          \
								}                                                                  \
							} while(0);
#endif

class CJdDbg
{
public:
	enum DBG_LVL_T
	{
		LVL_ERR,
		LVL_WARN,
		LVL_SETUP,
		LVL_MSG,
		LVL_TRACE,
		LVL_STRM
	};

	CJdDbg(const char* szPrefix, int dbgLvl)
	{
		mPrefix = szPrefix;
		mDbgLvl = dbgLvl;
	}

	static void dbg_printf (const char *format, ...)
	{
		char buffer[1024 + 1];
		va_list arg;
		va_start (arg, format);
		vsprintf (buffer, format, arg);
#ifdef WIN32
		//OutputDebugStringA(buffer);
		fprintf(stderr, ":%s", buffer);
#else
		fprintf(stderr, ":%s", buffer);
#endif
	}

	void Out(char *szError,...)
	{
		char	szBuff[256];
		va_list vl;

		va_start(vl, szError);
		sprintf(szBuff, mPrefix.c_str());
		vsprintf(szBuff + strlen(szBuff), szError, vl);
		strcat(szBuff, "\r\n");
#ifdef WIN32
		OutputDebugStringA(szBuff);
#else
#endif
		va_end(vl);
	}
	int         mDbgLvl;
	std::string mPrefix;
};
#endif // __JD_DBG_H__