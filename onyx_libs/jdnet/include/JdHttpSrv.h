#ifndef __JD_HTTP_SRV_H__
#define __JD_HTTP_SRV_H__

#include <assert.h>
#include <string.h>
#ifdef WIN32
//#include <winsock2.h>
#include <fcntl.h>
#include <io.h>

#define read	_read
#else
#include <signal.h>
#include <errno.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <sys/stat.h>
#include <pthread.h>
#endif
#include <stdlib.h>
#include <stdio.h>


#include "JdNetUtil.h"

#define MAX_HTTP_SRV_SESSIONS	4
#define MAX_HTTP_CALLBACKS		10

#define CHUNK_MAX_HDR_SIZE	10
#define CHUNK_MAX_TAIL_SIZE	10

#define STD_HEADER "Connection: close\r\n" \
                   "Server: NetOnTV-DMS/0.2\r\n" \
                   "Cache-Control: no-store, no-cache, must-revalidate, pre-check=0, post-check=0, max-age=0\r\n" \
                   "Pragma: no-cache\r\n"

typedef int (*pfnSendStream_t)(int hSock, char *pszUri);
class CJdHttpSrv;

typedef struct _HTTP_CALLBACK_T {
	char					*pszRootFolder;
	pfnSendStream_t			pfnSendStream;
} HTTP_CALLBACK_T;

/**
 * List of RTSP Server Session context management.
 * A CJdRtspSrvSession object is created for each session.
 */
class CJdHttpCallbackList
{
public:
	CJdHttpCallbackList()
	{
		memset(m_listCallbacks, 0x00, sizeof(void *) * MAX_HTTP_CALLBACKS);
	}

	pfnSendStream_t GetHttpCallback(char *pszRootFolder)
	{
		for (int i=0; i < MAX_HTTP_CALLBACKS;i++)
			if(m_listCallbacks[i] && (strcmp(m_listCallbacks[i]->pszRootFolder, pszRootFolder) == 0))
				return m_listCallbacks[i]->pfnSendStream;
		return NULL;
	}
	int Add(HTTP_CALLBACK_T *pCallback)
	{
		for (int i=0; i < MAX_HTTP_CALLBACKS;i++)
			if(!m_listCallbacks[i]){
				m_listCallbacks[i] = pCallback;
				return 0;
			}
		return -1;
	}
	int Delete(char *pszRootFolder)
	{
		for (int i=0; i < MAX_HTTP_CALLBACKS;i++){
			if(m_listCallbacks[i] && (strcmp(m_listCallbacks[i]->pszRootFolder, pszRootFolder) == 0)){
				free(m_listCallbacks[i]);
				m_listCallbacks[i] = NULL;
				return 0;
			}
		}
		return -1;
	}

	HTTP_CALLBACK_T *m_listCallbacks[MAX_HTTP_CALLBACKS];
};

/**
 * HTTP Server. 
 */
class CJdHttpSession
{
public:
	CJdHttpSession()
	{
	}
#ifdef WIN32
	static DWORD WINAPI SessionThread(void *arg );
#else
	static void *SessionThread( void *arg );
#endif
};
/**
 * HTTP Server. 
 */
class CJdHttpSrv
{
public:
	CJdHttpSrv()
	{
	}
	~CJdHttpSrv()
	{
	}
#ifdef WIN32
	static DWORD WINAPI ServerThread( void *arg );
#else
	static void *ServerThread( void *arg );
#endif
	int Stop(int id);
	int Run(
			int			id,			// Int server ID
			int			port		// RTSP Port for this instance
		);
	int RegisterCallback(
			const char			*pszFolder,	// Base folder to located the video resource
			pfnSendStream_t		pfnCallbak	// Callback RTSP Session State
		);

public:
	int						m_hSock;
};

/**
 * Helper Functions
 */
int SendHttpOK(int fd, int nContentLen,  const char *szHeader);
void SendError(int fd, int which, char *message);
#endif // __JD_HTTP_SRV_H__