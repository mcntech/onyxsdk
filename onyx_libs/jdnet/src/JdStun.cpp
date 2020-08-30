#include "JdStun.h"

#ifdef WIN32
#define close	closesocket
#else
#include <errno.h> 
#endif

#define DEF_SRC_PORT	59427
#define DEF_STUN_PORT	3478

#define MAX_NAME_SIZE	256
#define MAX_MSG_BUFF	2400
#define REQ_TIMEOUT		2
#define RESP_TIMEOUT	2
#ifdef WIN32
#define socklen_t	int
#endif

CJdStunTransact::CJdStunTransact()
{
	m_usHostExternalPort = 0;
	m_HostInternalAddr.s_addr = 0;
	m_usHostInternalPort = 0;
	m_HostExternalAddr.s_addr = 0;
	m_SeverAddr.s_addr = 0;
	m_hSock = -1;
}
int CJdStunTransact::Init(const char *pszServer)
{
	struct sockaddr_in localHost = {0};
	m_hSock = -1;
	char szHostName [MAX_NAME_SIZE];
	hostent *pHostent;

	/* Get Local host IP Address, if not set by calling module */
	if(m_HostInternalAddr.s_addr == 0) {
		if (gethostname (szHostName, MAX_NAME_SIZE) == -1)	{
			fprintf(stderr,"\nFailed to get gethostname\n");
			return 0;
		}
		pHostent = gethostbyname (szHostName);
		if(!pHostent) {
			fprintf(stderr,"\nFailed to get gethostbyname:%s\n",szHostName);
			return 0;
		}
		memcpy (&m_HostInternalAddr, pHostent->h_addr_list[0], sizeof (m_HostInternalAddr));
	}
	
	/* Get Stun server IP Address */
	pHostent = gethostbyname (pszServer);
	if(!pHostent) {
		fprintf(stderr,"\nFailed to get gethostbyname:%s\n",pszServer);
		return 0;
	}
	memcpy (&m_SeverAddr, pHostent->h_addr_list [0], sizeof (m_SeverAddr));

	localHost.sin_addr.s_addr = m_HostInternalAddr.s_addr;
	localHost.sin_family = AF_INET;
	localHost.sin_port = htons (m_usHostInternalPort);
	localHost.sin_addr.s_addr = htonl(INADDR_ANY);

	m_hSock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
#ifdef WIN32
	long on = 0;
	if (setsockopt(m_hSock, SOL_SOCKET, SO_EXCLUSIVEADDRUSE, (const char *)&on, sizeof(on)) < 0) {
		fprintf(stderr,"setsockopt(SO_EXCLUSIVEADDRUSE) failed");
		return -1;
	}
#endif
	
	if (bind (m_hSock, (struct sockaddr*)&localHost, sizeof (struct sockaddr)) != 0){
		fprintf(stderr,"%s:bind failed\n",__FUNCTION__);
		return 0;
	}
}
void CJdStunTransact::Deinit()
{
	if(m_hSock != -1){
		close(m_hSock);
	}
}
int CJdStunTransact::SendRequest(struct sockaddr_in *stunServer, char *pMsgBuff, int nMsgLen, int timeout)
{
	int nResult;

	fd_set fds;
	timeval tv;
	tv.tv_sec = timeout; 
	tv.tv_usec = 0;

	FD_ZERO(&fds);
	FD_SET(m_hSock, &fds);

	if ((nResult = select (m_hSock+1, NULL, &fds, NULL, &tv)) <= 0){
		fprintf(stderr,"%s:select failed\n",__FUNCTION__);
		return -1;		
	}
	if (nResult > 0 && FD_ISSET (m_hSock, &fds))	{
		nResult = sendto(m_hSock, pMsgBuff,nMsgLen, 0, (struct sockaddr*)stunServer, sizeof (struct sockaddr));
		if(nResult == -1) {
			in_addr Addr = stunServer->sin_addr;
#ifdef WIN32
			fprintf(stderr,"%s:sendto %s:%u failed errno=%d nMsgLen=%d\n",__FUNCTION__, inet_ntoa(Addr), ntohs(stunServer->sin_port),GetLastError(),nMsgLen);
#else
			fprintf(stderr,"%s:sendto %s:%u failed errno=%d nMsgLen=%d\n",__FUNCTION__, inet_ntoa(Addr), ntohs(stunServer->sin_port),errno, nMsgLen);
#endif
		}
	}
	return nResult;
}

int CJdStunTransact::GetResponse(char *pMsgBuff, int nMaxLen, int timeout)
{
	int nResult;

	fd_set fds;
	timeval tv;
	tv.tv_sec = timeout; 
	tv.tv_usec = 0;
	struct sockaddr_in stunServer;
	int nFromAddrLen = sizeof (stunServer);

	memcpy (&stunServer.sin_addr, &m_SeverAddr, sizeof (m_SeverAddr));
	stunServer.sin_family = AF_INET;
	stunServer.sin_port = htons(DEF_STUN_PORT);

	FD_ZERO(&fds);
	FD_SET(m_hSock, &fds);

	if ((nResult = select (m_hSock+1, &fds, NULL,  NULL, &tv)) <= 0){
		fprintf(stderr,"%s:select failed\n",__FUNCTION__);
		return -1;		
	}
	if (nResult > 0 && FD_ISSET (m_hSock, &fds))	{
		nResult = recvfrom(m_hSock, pMsgBuff, nMaxLen, 0, (struct sockaddr*)&stunServer, (socklen_t *)&nFromAddrLen);
	}

	return nResult;
}

int CJdStunDetect::TestI(struct sockaddr_in *stunServer, CStunMsg *pResponse)
{
	char MsgBuff[MAX_MSG_BUFF];
	int nMsgLen;

	CStunMsg StunReq(STUN_MSG_BINDING_REQUEST);

	nMsgLen = StunReq.Encode(MsgBuff);
	if(SendRequest(stunServer, MsgBuff, nMsgLen, REQ_TIMEOUT) == nMsgLen){
		nMsgLen = GetResponse(MsgBuff, MAX_MSG_BUFF, RESP_TIMEOUT);
		if(nMsgLen > 20) {
			pResponse->Parse(MsgBuff, nMsgLen);
		} else {
			fprintf(stderr,"\nFailed to get response\n");
			return -1;
		}
	} else {
		fprintf(stderr,"%s:SendRequest failed\n",__FUNCTION__);
		return -1;
	}
	return 0;
}

int CJdStunDetect::TestII(struct sockaddr_in *stunServer, CStunMsg *pResponse)
{
	char MsgBuff[MAX_MSG_BUFF];
	int nMsgLen;

	CStunMsg StunReq(STUN_MSG_BINDING_REQUEST);
	CStunChangeReq StunChangeReq(STUN_CHANGE_REQUEST_PORT | STUN_CHANGE_REQUEST_IP);
	StunReq.AddAttrib(&StunChangeReq);
	nMsgLen = StunReq.Encode(MsgBuff);
	if(SendRequest(stunServer, MsgBuff, nMsgLen, REQ_TIMEOUT) == nMsgLen){
		nMsgLen = GetResponse(MsgBuff, MAX_MSG_BUFF, RESP_TIMEOUT);
		if(nMsgLen > 20) {
			pResponse->Parse(MsgBuff, nMsgLen);
		} else {
			return -1;
		}
	} else {
		return -1;
	}
	return 0;
}

int CJdStunDetect::TestIII(struct sockaddr_in *stunServer, CStunMsg *pResponse)
{
	char MsgBuff[MAX_MSG_BUFF];
	int nMsgLen;

	CStunMsg StunReq(STUN_MSG_BINDING_REQUEST);
	CStunChangeReq StunChangeReq(STUN_CHANGE_REQUEST_PORT);
	StunReq.AddAttrib(&StunChangeReq);
	nMsgLen = StunReq.Encode(MsgBuff);
	if(SendRequest(stunServer, MsgBuff, nMsgLen, REQ_TIMEOUT) == nMsgLen){
		nMsgLen = GetResponse(MsgBuff, MAX_MSG_BUFF, RESP_TIMEOUT);
		if(nMsgLen > 20) {
			pResponse->Parse(MsgBuff, nMsgLen);
		} else {
			return -1;
		}
	} else {
		return -1;
	}
	return 0;
}

int CJdStunDetect::GetNatType(
	const char *pszServer,						// STUN Server
	struct in_addr	HostInternalAddr,		// Internal address
	unsigned short	usHostInternalPort		// Internal port
	)
{
	m_usHostInternalPort = usHostInternalPort;
	m_HostInternalAddr = HostInternalAddr;
	return GetNatType(pszServer);
}

int CJdStunDetect::GetNatType(const char *pszServer)
{

	int nRes;
	CStunMsg	Response;
	int nNatType = NAT_VARIANT_UNKNOWN;

	CStunAddrAttrib	*pMappedAddr1 = NULL;
	CStunAddrAttrib	*pChangedAddr = NULL;
	CStunAddrAttrib	*pSourceAddr  = NULL;
	CStunAddrAttrib	*pMappedAddr2 = NULL;

	/* If source port is not specified use default port */
	if(m_usHostInternalPort == 0)
		m_usHostInternalPort = DEF_SRC_PORT;	// Default

	m_usHostExternalPort = 0;
	memset(&m_HostExternalAddr, 0x00, sizeof(m_HostExternalAddr));
	Init(pszServer);

	struct sockaddr_in stunServer = {0};
	memcpy (&stunServer.sin_addr, &m_SeverAddr, sizeof(m_SeverAddr));
	stunServer.sin_family = AF_INET;
	stunServer.sin_port = htons (DEF_STUN_PORT);

	fprintf(stderr,"\nPerforming TestI:\n");
	nRes = TestI(&stunServer, &Response);
	fprintf(stderr,"\nComplete TestI:\n");
	if (nRes != 0){
		nNatType = NAT_VARIANT_FIREWALL_BLOCKS_UDP;
		goto Exit;
	}

	if (Response.m_Header.m_usMsgType != STUN_MSG_BINDING_RESPONSE){
		nNatType =  NAT_VARIANT_UNKNOWN;
		goto Exit;
	}

	Response.Dump();

	pMappedAddr1 = (CStunAddrAttrib *)Response.GetAttrib(STUN_ATTRIB_MAPPED_ADDRESS);
	pChangedAddr = (CStunAddrAttrib *)Response.GetAttrib(STUN_ATTRIB_CHANGED_ADDRESS);
	pSourceAddr = (CStunAddrAttrib *)Response.GetAttrib(STUN_ATTRIB_SOURCE_ADDRESS);
	pMappedAddr2 = NULL;

	if(pMappedAddr1) {
		m_HostExternalAddr.s_addr = htonl(pMappedAddr1->m_ulAddress);
		m_usHostExternalPort = pMappedAddr1->m_usPort;
	}
	// Match the mapped address with the source address
	//if (pMappedAddr1 && pSourceAddr && (*pMappedAddr1 == *pSourceAddr)){
	if (pMappedAddr1 && (pMappedAddr1->m_ulAddress == ntohl(*((unsigned long *)&m_HostInternalAddr)))){
	
		//TODO: Test this path
		CStunMsg	Response2;
		nRes = TestII (&stunServer, &Response2);
		if (nRes != 0) {
			fprintf(stderr,"\n NAT_VARIANT_OPEN_INTERNET\n");
			nNatType = NAT_VARIANT_OPEN_INTERNET;
			goto Exit;
		} else {
			fprintf(stderr,"\n NAT_VARIANT_SYMMETRIC_UDP_FIREWALL\n");
			nNatType = NAT_VARIANT_SYMMETRIC_UDP_FIREWALL;
			goto Exit;
		}
	} else {
		/* We are behind NAT */
		CStunMsg	Response2;

		nRes = TestII (&stunServer, &Response2);
		if (nRes == 0){
			Response2.Dump();
			fprintf(stderr,"\n NAT_VARIANT_FULL_CONE\n");
			nNatType =  NAT_VARIANT_FULL_CONE;
			goto Exit;
		} else {
			struct sockaddr_in stunServerChanged = {0};
			unsigned long ulChangeAddr = htonl(pChangedAddr->m_ulAddress);
			memcpy (&stunServerChanged.sin_addr, &ulChangeAddr, sizeof (ulChangeAddr));
			stunServerChanged.sin_family = AF_INET;
			stunServerChanged.sin_port = htons (pChangedAddr->m_usPort);

			nRes = TestI(&stunServerChanged, &Response2);

			if (nRes != 0)	{
				fprintf(stderr,"\n NAT_VARIANT_UNKNOWN\n");
				nNatType = NAT_VARIANT_UNKNOWN;
				goto Exit;
			}

			if (Response2.m_Header.m_usMsgType != STUN_MSG_BINDING_RESPONSE){				
				fprintf(stderr,"\n NAT_VARIANT_UNKNOWN\n");
				nNatType = NAT_VARIANT_UNKNOWN;
				goto Exit;
			}

			pMappedAddr2 = (CStunAddrAttrib *)Response2.GetAttrib(STUN_ATTRIB_MAPPED_ADDRESS);

			if (pMappedAddr1 && pMappedAddr2 && *pMappedAddr1 == *pMappedAddr2){
				
				fprintf(stderr,"\nPerforming TestIII...\n");
				nRes = TestIII (&stunServer, &Response2);
				fprintf(stderr,"\nComplete TestIII...\n");

				if (nRes != 0) {
					fprintf(stderr,"\n NAT_VARIANT_RESTRICTED_PORT_CONE\n");
					nNatType = NAT_VARIANT_RESTRICTED_PORT_CONE;
					goto Exit;
				} else {
					fprintf(stderr,"\n NAT_VARIANT_RESTRICTED_CONE\n");
					nNatType = NAT_VARIANT_RESTRICTED_CONE;
					goto Exit;
				}
			} else {
				fprintf(stderr,"\n NAT_VARIANT_SYMMETRIC\n");
				nNatType = NAT_VARIANT_SYMMETRIC;
				goto Exit;
			}
		}
	}
Exit:
	Deinit();
	return nNatType;
}

int CJdStunDetect::GetMappedAddr(
	const char		*pszStunServer,			// STUN Server
	struct in_addr	HostInternalAddr,		// Internal address
	unsigned short	usHostInternalPort,			// Internal Port
	struct in_addr	*pHostExternalAddr,		// External address
	unsigned short	*pusHostExternalPort		// External port
	)
{
	// TODO move to STUNT.
	// Currently it requires both UDP and TCP to enable for portforwarding 
	int nRes;
	CStunMsg	Response;
	int nNatType = NAT_VARIANT_UNKNOWN;

	CStunAddrAttrib	*pMappedAddr = NULL;

	m_usHostInternalPort = usHostInternalPort;
	m_HostInternalAddr = HostInternalAddr;

	/* If source port is not specified use default port */
	if(m_usHostInternalPort == 0)
		m_usHostInternalPort = DEF_SRC_PORT;	// Default

	m_usHostExternalPort = 0;
	memset(&m_HostExternalAddr, 0x00, sizeof(m_HostExternalAddr));
	Init(pszStunServer);

	struct sockaddr_in stunServer = {0};
	memcpy (&stunServer.sin_addr, &m_SeverAddr, sizeof(m_SeverAddr));
	stunServer.sin_family = AF_INET;
	stunServer.sin_port = htons (DEF_STUN_PORT);

	fprintf(stderr,"\nPerforming TestI:\n");
	nRes = TestI(&stunServer, &Response);
	fprintf(stderr,"\nComplete TestI:\n");
	if (nRes != 0){
		nNatType = NAT_VARIANT_FIREWALL_BLOCKS_UDP;
		goto Exit;
	}

	if (Response.m_Header.m_usMsgType != STUN_MSG_BINDING_RESPONSE){
		nNatType =  NAT_VARIANT_UNKNOWN;
		nRes = -1;
		goto Exit;
	}
	Response.Dump();
	pMappedAddr = (CStunAddrAttrib *)Response.GetAttrib(STUN_ATTRIB_MAPPED_ADDRESS);

	if(pMappedAddr) {
		m_HostExternalAddr.s_addr = htonl(pMappedAddr->m_ulAddress);
		m_usHostExternalPort = pMappedAddr->m_usPort;
		*pHostExternalAddr = m_HostExternalAddr;
		*pusHostExternalPort = m_usHostExternalPort;
	} else {
		nRes = -1;
	}
Exit:
	Deinit();
	return nRes;	
}

int CJdStunBinding::Run(const char *pszServer, int nMode)
{
	char MsgBuff[MAX_MSG_BUFF];
	int nMsgLen;

	Init(pszServer);
	switch(nMode)
	{
		case BINDING_RUN_ONCE:
			{
				struct sockaddr_in stunServer;
				memcpy (&stunServer.sin_addr, &m_SeverAddr, sizeof (m_SeverAddr));
				stunServer.sin_family = AF_INET;
				stunServer.sin_port = htons (DEF_STUN_PORT);

				CStunMsg StunReq(STUN_MSG_BINDING_REQUEST);
				CStunMsg StunResp(STUN_MSG_BINDING_RESPONSE);
				nMsgLen = StunReq.Encode(MsgBuff);
				if(SendRequest(&stunServer, MsgBuff, nMsgLen, REQ_TIMEOUT) == nMsgLen){
					nMsgLen = GetResponse(MsgBuff, MAX_MSG_BUFF, RESP_TIMEOUT);
					if(nMsgLen > 20) {
						StunResp.Parse(MsgBuff, nMsgLen);
					} else {
						fprintf(stderr,"\nFailed to get response\n");
					}
				} else {
					fprintf(stderr,"\nFailed to send request\n");
				}
			}
			break;

		case BINDING_RUN_CONTINUOUS:
			//TODO
			break;
	}
	return 0;
}

int CJdStunBindingStop()
{
	return 0;
}
