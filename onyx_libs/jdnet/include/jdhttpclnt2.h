#ifndef  __JdHttpClnt2_H__
#define __JdHttpClnt2_H__

#define UPNP_E_SUCCESS 0

int UpnpOpenHttpConnection(const char *, void * pHandleM, int timeoutM);
int UpnpMakeHttpRequest(
    JdAws_HttpMethod method,
	const char *url,
	void *handle,
	char **headers,                                    
	const char *contentType,
	int contentLength,
	int timeout);
int UpnpWriteHttpRequest(
	void *handle,
	char *buf,
	size_t *size,
	int timeout);
int UpnpEndHttpRequest(
	void *handle,
	int timeout);
int UpnpGetHttpResponse(
	void *handle,
	char *headers,   
	char **contentType,
	int *contentLength,
	int *httpStatus,
	int timeout);
int UpnpCloseHttpConnection(
	void *handle);

#endif //__JdHttpClnt2_H__