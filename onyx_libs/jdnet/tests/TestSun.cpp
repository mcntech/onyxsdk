// TestRtpRcv.cpp : Defines the entry point for the console application.
//
#ifdef WIN32
#include <winsock2.h>
#include <fcntl.h>
#include <io.h>

#define close		closesocket
#define snprintf	_snprintf
#define write		_write
#else
#endif
#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>
#include "JdStun.h"

int main(int argc, char* argv[])
{
#ifdef WIN32
	int nRet = 0;
	WSADATA wsaData;

	nRet = WSAStartup (MAKEWORD (2, 2), &wsaData);
	if (nRet != 0) {
		return 0;
	}
#endif
#ifdef TEST_BIND
	CJdStunBinding StunBinding;
	StunBinding.Run("stunserver.org",BINDING_RUN_ONCE);
#endif
	CJdStunDetect  JdStunDetect;
#if 0
	JdStunDetect.m_usHostInternalPort = 59427;
	JdStunDetect.GetNatType("stunserver.org");
#else
	struct in_addr hostIntAddr;
	unsigned short ushostIntPort = 8090;
	struct in_addr hostExtAddr;
	unsigned short ushostExtPort = 0;
	hostIntAddr.s_addr = inet_addr("192.168.1.105");
	JdStunDetect.GetMappedAddr(
		"stun.xten.com",
		//"stunserver.org", 
		hostIntAddr, ushostIntPort, &hostExtAddr, &ushostExtPort);
#endif
	fprintf(stdout,"Press any key..");
	getc(stdin);
	return 0;
}

