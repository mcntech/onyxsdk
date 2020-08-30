#include <assert.h> 
#include <string.h> 
#ifdef WIN32 
#include <winsock2.h> 
#include <fcntl.h> 
#include <io.h> 
 
#define read	    _read 
#define pthread_t   void *
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

 
#include <fcntl.h> 
//#include "JdMimeTypes.h" 
#include "JdHttpSrv.h" 
#include "JdNetUtil.h" 
 
#define CAMERA_RESOURCE	"camera" 
 
#ifdef WIN32 
#define close		closesocket 
#define open		_open 
#define O_RDONLY	_O_RDONLY 
#define O_BINARY	_O_BINARY 
#else // Linux 
#define O_BINARY	0 
#endif 
 
 
#ifdef WIN32 
#define RESOURCE_FOLDER	"c:/dms/"			// TODO: Get it from application 
#else 
#define RESOURCE_FOLDER	"/data/dms/" 
#endif 
 
 
#define MSG_BUFFER_SIZE		(1024) 
#define MAX_SERVERS			10 
 
 
#define MIN(a, b) (((a) < (b)) ? (a) : (b)) 
#define MAX(a, b) (((a) > (b)) ? (a) : (b)) 
 
 
/* context of each server thread */ 
typedef struct _context { 
	int sd; 
	int id; 
	pthread_t threadID; 
	int port; 
	int stop; 
} context; 
 
CJdHttpCallbackList gJdHttpCallbackList; 
/* 
 * this struct is just defined to allow passing all necessary details to a worker thread 
 * "cfd" is for connected/accepted filedescriptor 
 */ 
typedef struct { 
	context *pc; 
	int fd; 
} cfd; 
 
extern context servers[MAX_SERVERS]; 
context servers[MAX_SERVERS]; 
 
 
void SendError(int fd, int which, char *message) { 
  char buffer[MSG_BUFFER_SIZE] = {0}; 
 
  if ( which == 401 ) { 
    sprintf(buffer, "HTTP/1.0 401 Unauthorized\r\n" \
                    "Content-type: text/plain\r\n" \
                    STD_HEADER \
                    "WWW-Authenticate: Basic realm=\"Jade Camera\"\r\n" \
                    "\r\n" \
                    "401: Not Authenticated!\r\n" \
                    "%s", message);
  } else if ( which == 404 ) { 
    sprintf(buffer, "HTTP/1.0 404 Not Found\r\n" \
                    "Content-type: text/plain\r\n" \
                    STD_HEADER \
                    "\r\n" \
                    "404: Not Found!\r\n" \
                    "%s", message); 
  } else if ( which == 500 ) { 
    sprintf(buffer, "HTTP/1.0 500 Internal Server Error\r\n" \
                    "Content-type: text/plain\r\n" \
                    STD_HEADER \
                    "\r\n" \
                    "500: Internal Server Error!\r\n" \
                    "%s", message); 
  } else if ( which == 400 ) { 
    sprintf(buffer, "HTTP/1.0 400 Bad Request\r\n" \
                    "Content-type: text/plain\r\n" \
                    STD_HEADER \
                    "\r\n" \
                    "400: Not Found!\r\n" \
                    "%s", message); 
  } else { 
    sprintf(buffer, "HTTP/1.0 501 Not Implemented\r\n" \
                    "Content-type: text/plain\r\n" \
                    STD_HEADER \
                    "\r\n" \
                    "501: Not Implemented!\r\n" \
                    "%s", message); 
  } 
 
  send(fd, buffer, strlen(buffer), 0); 
} 
 
 
int SendHttpOK(int fd, int nContentLen, const char *szHeader) 
{ 
	const char *mimetype="video/mpeg"; 
	char buffer[MSG_BUFFER_SIZE] = {0};
	char szTmp[128];
	int res = 0; 
 
	sprintf(buffer, "HTTP/1.0 200 OK\r\n");

	if(nContentLen == 0){
		sprintf(szTmp,"TRANSFER-ENCODING: chunked\r\n");
		strcat(buffer,szTmp);
	} else {
		sprintf(szTmp,"Content-Length: %d\r\n", nContentLen);
		strcat(buffer,szTmp);
	}
	strcat(buffer,szHeader);

	{
		//sprintf(szTmp,"TransferMode.DLNA.ORG: Streaming\r\n");
		sprintf(szTmp,"TransferMode.DLNA.ORG: Interactive\r\n");
		strcat(buffer,szTmp);
	}
	strcat(buffer, "\r\n");

	res = send(fd, buffer, strlen(buffer), 0);
	if (res < 0) {
		fprintf(stderr,	"Socket write failed res=%d\n", res);
		return -1;
	}
	return 0;
} 
#ifdef WIN32
DWORD WINAPI CJdHttpSession::SessionThread(void *arg ) { 
#else
void *CJdHttpSession::SessionThread(void *arg ) { 
#endif
	int cnt; 
	char buffer[MSG_BUFFER_SIZE]={0}, *pb=buffer; 
	cfd lcfd; /* local-connected-file-descriptor */ 
	char *pszResource = NULL; 
	char *pszRootFolder = NULL; 
	pfnSendStream_t pSendStrm = NULL; 
	int len; 
 
	if (arg != NULL) { 
		memcpy(&lcfd, arg, sizeof(cfd)); 
		free(arg); 
	} else { 
		 return NULL; 
	} 
 
#ifdef SO_NOSIGPIPE 
	int set = 1; 
	int ret = setsockopt(lcfd.fd, SOL_SOCKET, SO_NOSIGPIPE, &set, sizeof(set)); 
	printf("%s\n", strerror(errno)); 
	assert(ret == 0); 
#endif 
 
 
	/* What does the client want to receive? Read the request. */ 
	memset(buffer, 0, sizeof(buffer)); 
 
	int nHeaderLen = ReadHeader(lcfd.fd, buffer); 
 
	if ( (pb = strstr(buffer, "GET /")) == NULL ) { 
          fprintf(stderr, "Malformed HTTP request\n"); 
		SendError(lcfd.fd, 400, "Malformed HTTP request"); 
		close(lcfd.fd); 
		return NULL; 
	} 
 
	pb += strlen("GET /"); 
	nHeaderLen = nHeaderLen - strlen("GET /"); 
	len = MIN(MAX(strspn(pb, "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ._-1234567890=&;?/:"), 0), nHeaderLen); 
	pszResource = (char *)malloc(len+1); 
	if ( pszResource == NULL ) { 
		fprintf(stderr, "No resource...\n"); 
		return NULL; 
	} 
	memset(pszResource, 0, len+1); 
	strncpy(pszResource, pb, len); 
	{ 
		/* Get Root Folder */ 
		int len = 0; 
		const char *pStart = pszResource; 
		const char *pEnd = strstr(pszResource,"/"); 
		if(pEnd) { 
			len = pEnd - pStart; 
		} else { 
			len = strlen(pStart); 
		} 
		if(len >= 0 && len < 1024){ 
			pszRootFolder = (char *)malloc(len + 1 + 1); 
			pszRootFolder[0] = '/'; 
			strncpy(pszRootFolder + 1, pStart, len); 
			pszRootFolder[len + 1] = 0x00; 
		} 
	} 
	if(pszRootFolder){ 
		pSendStrm = gJdHttpCallbackList.GetHttpCallback(pszRootFolder); 
	} 
	if(pSendStrm){ 
		pSendStrm(lcfd.fd, pszResource); 
	} else { 
		fprintf(stderr,"!!!Streaming callback not found for res=%s folder=%s!!!\n",pszResource, pszRootFolder); 
	} 
 
	close(lcfd.fd); 
	if(pszResource) 
		free(pszResource); 
	if(pszRootFolder) 
		free(pszRootFolder); 
	return NULL; 
} 
 
void ServerCleanup(void *arg) { 
  context *pcontext = (context *)arg; 
  close(pcontext->sd); 
} 
#ifdef WIN32
DWORD WINAPI  CJdHttpSrv::ServerThread( void *arg ) {
#else
void *CJdHttpSrv::ServerThread( void *arg ) { 
#endif
	struct sockaddr_in addr, client_addr; 
	int on; 
	pthread_t client; 
	int addr_len = sizeof(struct sockaddr_in); 
 
	context *pcontext = (context *)arg; 
#ifdef WIN32
#else
	/* set cleanup handler to cleanup ressources */ 
	pthread_cleanup_push(ServerCleanup, pcontext); 
#endif
	/* open socket for server */ 
	pcontext->sd = socket(PF_INET, SOCK_STREAM, 0); 
	if ( pcontext->sd < 0 ) { 
		fprintf(stderr, "socket failed\n"); 
		goto Exit; 
	} 
 
	/* ignore "socket already in use" errors */ 
	on = 1; 
	if (setsockopt(pcontext->sd, SOL_SOCKET, SO_REUSEADDR, (const char *)&on, sizeof(on)) < 0) { 
		perror("setsockopt(SO_REUSEADDR) failed"); 
		goto Exit; 
	} 
 
	/* perhaps we will use this keep-alive feature oneday */ 
	/* setsockopt(sd, SOL_SOCKET, SO_KEEPALIVE, &on, sizeof(on)); */ 
 
	/* configure server address to listen to all local IPs */ 
	memset(&addr, 0, sizeof(addr)); 
	addr.sin_family = AF_INET; 
	addr.sin_port = htons(pcontext->port); /* is already in right byteorder */ 
	addr.sin_addr.s_addr = htonl(INADDR_ANY); 
	if ( bind(pcontext->sd, (struct sockaddr*)&addr, sizeof(addr)) != 0 ) { 
		fprintf(stderr,"failed to bind socket"); 
		goto Exit; 
	} 
 
	/* start listening on socket */ 
	if ( listen(pcontext->sd, 10) != 0 ) { 
		fprintf(stderr, "listen failed\n"); 
		goto Exit; 
	} 
 
	/* create a child for every client that connects */ 
	while ( !pcontext->stop ) { 
		 
		cfd *pcfd = (cfd *)malloc(sizeof(cfd)); 
 
		if (pcfd == NULL) { 
			fprintf(stderr, "failed to allocate (a very small amount of) memory\n"); 
			goto Exit; 
		} 
 
		pcfd->fd = accept(pcontext->sd, (struct sockaddr *)&client_addr,  
#ifdef WIN32 
#else 
			(socklen_t *) 
#endif			 
			&addr_len); 
		pcfd->pc = pcontext; 
 
		/* pcfd is released by SessionThread*/ 
#ifdef WIN32 
		DWORD dwThreadId;
	    client = CreateThread(NULL, 0, CJdHttpSession::SessionThread, pcfd, 0, &dwThreadId);
		if(client == NULL) {
#else
		if( pthread_create(&client, NULL, CJdHttpSession::SessionThread, pcfd) != 0 ) { 
#endif
			close(pcfd->fd); 
			free(pcfd); 
			continue; 
		} 
#ifdef WIN32 
#else
		pthread_detach(client); 
#endif
	} 
Exit: 
#ifdef WIN32 
#else
	pthread_cleanup_pop(1); 
#endif
 
	return NULL; 
} 
 
 
int CJdHttpSrv::RegisterCallback( 
		const char				*pszRootFolder,	 
		pfnSendStream_t			pfnSendStream_t 
		) 
{ 
	HTTP_CALLBACK_T *pCallback = (HTTP_CALLBACK_T *)malloc(sizeof(HTTP_CALLBACK_T)); 
											// Callback RTSP Session State 
	pCallback->pszRootFolder = strdup(pszRootFolder); 
	pCallback->pfnSendStream = pfnSendStream_t; 
	if(gJdHttpCallbackList.Add(pCallback) != 0){ 
		free(pCallback); 
		return -1; 
	} 
	return 0; 
} 
 
int CJdHttpSrv::Stop(int id)  
{ 
#ifdef WIN32 
	WSACleanup(); 
#endif 
 #ifdef WIN32 
	servers[id].stop = 1;
	close(servers[id].sd); 
#else
  pthread_cancel(servers[id].threadID); 
#endif
  return 0; 
} 
 
int CJdHttpSrv::Run(int id, int port)  
{ 
#ifdef WIN32 
    WORD wVersionRequested; 
    WSADATA wsaData; 
    int err; 
 
/* Use the MAKEWORD(lowbyte, highbyte) macro declared in Windef.h */ 
    wVersionRequested = MAKEWORD(2, 2); 
 
    err = WSAStartup(wVersionRequested, &wsaData); 
    if (err != 0) { 
        /* Tell the user that we could not find a usable */ 
        /* Winsock DLL.                                  */ 
        printf("WSAStartup failed with error: %d\n", err); 
        return 1; 
    } 
#endif 
  /* create thread and pass context to thread function */ 
	servers[id].port = port; 
#ifdef WIN32
	DWORD dwThreadId;
	servers[id].threadID = CreateThread(NULL, 0, ServerThread, &(servers[id]), 0, &dwThreadId ); 
	//pthread_detach(servers[id].threadID); 
#else
	pthread_create(&(servers[id].threadID), NULL, ServerThread, &(servers[id])); 
	pthread_detach(servers[id].threadID); 
#endif
	return 0; 
} 
