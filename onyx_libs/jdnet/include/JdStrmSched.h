#ifndef __JD_STRM_SCHED_H__
#define __JD_STRM_SCHED_H__

#ifdef WIN32
#include <winsock2.h>
#include <fcntl.h>
#include <io.h>
#include <ws2tcpip.h>
#define close		closesocket
#define snprintf	_snprintf
#define write		_write
#define socklen_t	int
#else
#include <unistd.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <strings.h>
#include <netdb.h>
#include <string.h>
#endif
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <errno.h>

#ifndef WIN32

#define BYTE unsigned char
#define WORD unsigned short
#define DWORD unsigned long
#define __int64 long long
#define UNALIGNED
#define ULONGLONG	long long
#endif

#ifdef WIN32
#define OS_SLEEP(ms)		Sleep(ms);
#else
#define OS_SLEEP(ms)		uslleep(ms * 1000);
#endif

#ifdef WIN32
#define GET_TICK()			timeGetTime()
#else
#define GET_TICK()			TODO
#endif

class CJdStrmSched
{
public:
	CJdStrmSched()
	{
		m_lSysTimeStart = 0;
		m_lPcrStart = 0;

		m_lPcr = 0;
		m_lBitrate = 4000000;
		m_lIntrvalBytes = 0;
		m_lMaxDelay = 1000;
	}
	
	void Delay(long lPcrMs, long lBytes)
	{
		if(m_lPcr == 0) {
			/* First time init or init after reset */
			if(lPcrMs) {
				m_lPcr = m_lPcrStart = lPcrMs; 
				m_lSysTimeStart = GET_TICK();
			}
			return;
		}

		if(lPcrMs == 0){
			/* Predict the current PCR based on bitrate */
			m_lIntrvalBytes += lBytes;
			lPcrMs = m_lPcr + (m_lIntrvalBytes * 1000 / (m_lBitrate / 8));
		} else {
			m_lIntrvalBytes = 0;
			m_lPcr = lPcrMs;
		}

		long delSys = GET_TICK() - m_lSysTimeStart;
		long delPcr = lPcrMs - m_lPcrStart;

		if(abs(delPcr - delSys) > m_lMaxDelay){
			/* Something wrong. Reset */
			m_lPcr = 0;
			m_lIntrvalBytes = 0;
			return;
		}
		while(delPcr > delSys) {
			OS_SLEEP(1);
			delSys = GET_TICK() - m_lSysTimeStart;
		}
	}
	long m_lSysTimeStart;
	long m_lPcrStart;
	long m_lPcr;
	long m_lMaxDelay;
	long m_lBitrate;
	long m_lIntrvalBytes;

};

#endif //__JD_STRM_SCHED_H__