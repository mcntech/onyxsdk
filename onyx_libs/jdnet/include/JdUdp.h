#ifndef __UDP_H__
#define __UDP_H__

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef WIN32
#include <winsock2.h>
#include <fcntl.h>
#include <io.h>
#define close	closesocket
#define snprintf	_snprintf
#define write	_write
#else
#include <unistd.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <strings.h>
#include <netdb.h>
#endif
#define NOT_IMPLEMENTED fprintf(stderr,"%s:Not Implemented.\n",__FUNCTION__);

class CUdp
{
public:
	typedef enum _TransferMode
	{
		MODE_SERVER,
		MODE_CLIENT,
		MODE_BOTH,
	} TransferMode;

	CUdp(TransferMode Mode)
	{
		m_hSock = -1;
		mMode = Mode;
		memset(&m_SockAddr, 0x00, sizeof(m_SockAddr));
	}
	virtual ~CUdp()
	{

	}

	int CreateSession();

	virtual void CloseSession()
	{
		if(m_hSock != -1)
			close(m_hSock);
	}
	virtual int Start() {NOT_IMPLEMENTED; return -1;}
	virtual int Pause() {NOT_IMPLEMENTED; return -1;}
	virtual int Stop(){NOT_IMPLEMENTED; return -1;}
	virtual int Read(char *pData, int nMaxLen);
	virtual int Write(char *pData, int nMaxLen);

	int	           m_hSock;
	in_addr        m_SockAddr;
	unsigned short mRemoteRtpPort;
	unsigned short mLocalRtpPort;
	bool           mMulticast;
	TransferMode   mMode;
};

#endif //__RTP_H__