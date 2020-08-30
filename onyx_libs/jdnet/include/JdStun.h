#ifndef __JD_STUN_H__
#define __JD_STUN_H__

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
#include <string.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#endif

typedef enum _STUN_MSG_T
{
	STUN_MSG_BINDING_REQUEST			= 0x0001,
	STUN_MSG_BINDING_RESPONSE			= 0x0101,
	STUN_MSG_BINDING_ERROR_RESPONSE		= 0x0111,
    STUN_MSG_SHARED_SECRET_REQUEST		= 0x0002,
    STUN_MSG_SHARED_SECRET_RESPONSE		= 0x0102,
    STUN_MSG_SHARED_SECRET_ERROR_RESPONSE = 0x0112
} STUN_MSG_T;

typedef enum _STUN_ATTRIB_T
{
	STUN_ATTRIB_RESERVED				= 0x0000,
	STUN_ATTRIB_MAPPED_ADDRESS			= 0x0001,
	STUN_ATTRIB_RESPONSE_ADDRESS		= 0x0002,
	STUN_ATTRIB_CHANGE_REQUEST			= 0x0003,
	STUN_ATTRIB_SOURCE_ADDRESS			= 0x0004,
	STUN_ATTRIB_CHANGED_ADDRESS			= 0x0005,
	STUN_ATTRIB_USERNAME				= 0x0006,
	STUN_ATTRIB_PASSWORD				= 0x0007,
	STUN_ATTRIB_MESSAGE_INTEGRITY		= 0x0008,
	STUN_ATTRIB_ERROR_CODE				= 0x0009,
	STUN_ATTRIB_UNKNOWN_ATTRIBUTES		= 0x000a,
	STUN_ATTRIB_REFLECTED_FROM			= 0x000b,
	STUN_ATTRIB_COUNT					= 0x000c
} STUN_ATTRIB_T;

#define STUN_CHANGE_REQUEST_PORT		2
#define STUN_CHANGE_REQUEST_IP			4

enum NAT_TYPE
{
	NAT_VARIANT_UNKNOWN,
	NAT_VARIANT_OPEN_INTERNET,
	NAT_VARIANT_FIREWALL_BLOCKS_UDP,
	NAT_VARIANT_SYMMETRIC_UDP_FIREWALL,
	NAT_VARIANT_FULL_CONE,
	NAT_VARIANT_SYMMETRIC,
	NAT_VARIANT_RESTRICTED_CONE,
	NAT_VARIANT_RESTRICTED_PORT_CONE,
};

#define STUN_MAX_ATTRIBUTES				12

#define BINDING_RUN_ONCE				1
#define BINDING_RUN_CONTINUOUS			2

// TODO: Map to system timer
#define GET_SYSTEM_TICK					0

#ifdef __BIG_ENDIAN__
// TODO
#else
#define CPU_TO_MEM_U16(m,v) m[0] = (unsigned char)(v >> 8) & 0x00FF; \
							m[1] = (unsigned char)(v & 0x00FF);
#define MEM_TO_CPU_U16(m,v) v = ((((unsigned short)m[0]) << 8) & 0xFF00) | \
								((unsigned short)m[1] & 0x00FF);

#define CPU_TO_MEM_U32(m,v) m[0] = (v >> 24) & 0x00FF; \
							m[1] = (v >> 24) & 0x00FF; \
							m[2] = (v >> 16) & 0x00FF; \
							m[3] = v & 0x00FF;

#define MEM_TO_CPU_32(m,v) v = (((unsigned long)m[0]) << 24) & 0xFF000000 | \
								(((unsigned long)m[1]) << 16) & 0x00FF0000 | \
								(((unsigned long)m[2]) << 8) & 0x0000FF00 | \
								(unsigned long)m[3] & 0x000000FF;


#endif

class CStunHeader
{
public:
	CStunHeader(unsigned short usMsgType):m_usMsgType(usMsgType)
	{
		srand (GET_SYSTEM_TICK);
		for (int i = 0; i < 16; ++i) {
			m_TransId[i] = rand ();
		}
	}
	int Encode(char *pData)
	{
		char *pOut = pData;
		CPU_TO_MEM_U16(pOut,m_usMsgType)
		pOut += 2;
		CPU_TO_MEM_U16(pOut,m_usLen);
		pOut += 2;
		memcpy(pOut, m_TransId, 16);
		pOut += 16;
		return (pOut - pData);
	}

	int Parse(char *pData)
	{
		char *pIn = pData;
		MEM_TO_CPU_U16(pIn,m_usMsgType)
		pIn += 2;
		MEM_TO_CPU_U16(pIn,m_usLen);
		pIn += 2;
		memcpy(m_TransId,pIn, 16);
		pIn += 16;
		return (pIn - pData);
	}

	unsigned short	m_usMsgType;
	unsigned short 	m_usLen;
	unsigned char	m_TransId[16];
};

class CStunAttrib
{
public:
	CStunAttrib()
	{
	}
	virtual int Encode(char *pData)
	{
		CPU_TO_MEM_U16(pData, m_usType)
		pData += 2;
		CPU_TO_MEM_U16(pData, m_usLen)
		pData += 2;
		return 4;
	}
	virtual int Parse(char *pData)
	{
		char *pIn = pData;
		MEM_TO_CPU_U16(pIn, m_usType)
		pIn += 2;
		MEM_TO_CPU_U16(pIn, m_usLen)
		pIn += 2;
		return 4;
	}
	virtual void Dump() {}
	unsigned short m_usType;
	unsigned short m_usLen;

};

class CStunChangeReq :	public CStunAttrib
{
public:
	CStunChangeReq(unsigned long ulChangeFlag)
	{
		m_usType = STUN_ATTRIB_CHANGE_REQUEST;
		m_usLen = 4;
		m_ulChangeFlag = ulChangeFlag;
	}

	int Encode(char *pData)
	{
		char *pOut = pData;
		pOut += CStunAttrib::Encode(pOut);
		CPU_TO_MEM_U32(pOut, m_ulChangeFlag);
		pOut += sizeof(m_ulChangeFlag);
		return (pOut - pData);
	}

	int Parse(char *pData)
	{
		char *pIn = pData;
		pIn += CStunAttrib::Encode(pIn);
		return m_usLen;
	}
	unsigned long m_ulChangeFlag;
};

class CStunAddrAttrib :	public CStunAttrib
{
public:
	CStunAddrAttrib()
	{
	}
	int operator ==(CStunAddrAttrib &rAddr)
	{
		return (this->m_ulAddress == rAddr.m_ulAddress &&
			this->m_usPort == rAddr.m_usPort);
	}
	int Encode(char *pData)
	{
		char *pOut = pData;
		pOut += CStunAttrib::Encode(pOut);
		*pOut++ = m_cReserve;
		*pOut++ = m_cFamily;
		CPU_TO_MEM_U16(pOut, m_usPort);
		pOut += sizeof(m_usPort);
		CPU_TO_MEM_U32(pOut, m_ulAddress)
		pOut += sizeof(m_ulAddress);
		return (pOut - pData);
	}

	int Parse(char *pData)
	{

		char *pIn = pData;
		pIn += CStunAttrib::Parse(pIn);
		m_cReserve = *pIn++;
		m_cFamily = *pIn++;
		MEM_TO_CPU_U16(pIn, m_usPort);
		pIn += sizeof(m_usPort);
		MEM_TO_CPU_32(pIn, m_ulAddress)
		pIn += sizeof(m_ulAddress);
		return (pIn - pData);
	}
	virtual void Dump() 
	{
		unsigned long ulAddress = htonl(m_ulAddress);
		fprintf(stderr,"Addr=%s Port=%u Family=%u\n",inet_ntoa(*((struct in_addr*)&(ulAddress))),m_usPort,m_cFamily);
	}

	unsigned long	m_ulAddress;
	unsigned short	m_usPort;
	unsigned char	m_cFamily;
	unsigned char	m_cReserve;
};


class CStunMappedAddr :	public CStunAddrAttrib
{
	virtual void Dump() 
	{
		unsigned long ulAddress = htonl(m_ulAddress);
		fprintf(stderr,"MappedAddr=%s Port=%u Family=%u\n",inet_ntoa(*((struct in_addr*)&(ulAddress))),m_usPort,m_cFamily);
	}
};

class CStunChangedAddr :	public CStunAddrAttrib
{
	virtual void Dump() 
	{
		unsigned long ulAddress = htonl(m_ulAddress);
		fprintf(stderr,"ChangedAddr=%s Port=%u Family=%u\n",inet_ntoa(*((struct in_addr*)&(ulAddress))),m_usPort,m_cFamily);
	}
};

class CStunSourceAddr :	public CStunAddrAttrib
{
	virtual void Dump() 
	{
		unsigned long ulAddress = htonl(m_ulAddress);
		fprintf(stderr,"SourceAddr=%s Port=%u Family=%u\n",inet_ntoa(*((struct in_addr*)&(ulAddress))),m_usPort,m_cFamily);
	}
};

class CStunResponseAddr :	public CStunAddrAttrib
{
	virtual void Dump() 
	{
		unsigned long ulAddress = htonl(m_ulAddress);
		fprintf(stderr,"ResponseAddr=%s Port=%u Family=%u\n",inet_ntoa(*((struct in_addr*)&(ulAddress))),m_usPort,m_cFamily);
	}
};

class CStunMsg
{
public:
	CStunMsg(unsigned short usMsgType=0):m_Header(usMsgType)
	{
		memset(m_plistAttrib,0x00, sizeof(void *) * STUN_ATTRIB_COUNT);
	}

	int Encode(char *pData)
	{
		char *pOut = pData;
		/* Leave Space for Header. Dummy update */
		pOut += m_Header.Encode(pOut);
		for (int i = 0; i < STUN_ATTRIB_COUNT; i++){
			if(m_plistAttrib[i])
				pOut += m_plistAttrib[i]->Encode(pOut);
		}
		/* Update the header */
		m_Header.m_usLen = pOut - pData - 20/*size of hdr*/;
		m_Header.Encode(pData);
		return pOut - pData;
	}

	int Parse(char *pData, int nLen)
	{
		int nAttrType;
		char *pIn = pData;
		pIn += m_Header.Parse(pIn);
		CStunAttrib SkipAttrib;
		while(pIn < pData + nLen){
			CStunAttrib *pAttrib = NULL;
			MEM_TO_CPU_U16(pIn,nAttrType);
			switch(nAttrType)
			{
				case STUN_ATTRIB_MAPPED_ADDRESS:
					pAttrib = new CStunMappedAddr;
					break;

				case STUN_ATTRIB_RESPONSE_ADDRESS:
					pAttrib = new CStunResponseAddr;
					break;


				case STUN_ATTRIB_SOURCE_ADDRESS:
					pAttrib = new CStunSourceAddr;
					break;

				case STUN_ATTRIB_CHANGED_ADDRESS:
					pAttrib = new CStunChangedAddr;
					break;

				case STUN_ATTRIB_USERNAME:
				case STUN_ATTRIB_PASSWORD:	
				case STUN_ATTRIB_MESSAGE_INTEGRITY:
				case STUN_ATTRIB_ERROR_CODE:
				case STUN_ATTRIB_UNKNOWN_ATTRIBUTES:
				case STUN_ATTRIB_REFLECTED_FROM:
				case STUN_ATTRIB_CHANGE_REQUEST:
				case STUN_ATTRIB_RESERVED:
				default:
					pAttrib = &SkipAttrib;
					break;
			}
			pAttrib->Parse(pIn);
			pIn += pAttrib->m_usLen + 4;
			AddAttrib(pAttrib);
			
		}
		return pIn - pData;
	}
	
	/* Index is made equal to attrib id */
	void AddAttrib(CStunAttrib *pAttrib)
	{
		if(pAttrib->m_usType < STUN_ATTRIB_COUNT)
			m_plistAttrib[pAttrib->m_usType] = pAttrib;
		else
			fprintf(stderr,"Unknown Attrib\n");
	}

	CStunAttrib *GetAttrib(unsigned short usAttribType)
	{
		return m_plistAttrib[usAttribType];
	}
	virtual void Dump()
	{
		for (int i = 0; i < STUN_ATTRIB_COUNT; i++){
			if(m_plistAttrib[i])
				m_plistAttrib[i]->Dump();
		}
	}

public:
	CStunHeader m_Header;
	CStunAttrib *m_plistAttrib[STUN_ATTRIB_COUNT];
};


class CJdStunTransact
{
public:
	CJdStunTransact();
	int Init(const char *pszServer);
	void Deinit();
	int SendRequest(struct sockaddr_in *stunServer, char *pMsgBuff, int nMsgLen, int timeout);
	int GetResponse(char *pMsgBuff, int nMaxLen, int timeout);
	unsigned short	m_usHostExternalPort;
	struct in_addr	m_HostInternalAddr;
	unsigned short	m_usHostInternalPort;
    struct in_addr	m_HostExternalAddr;
    struct in_addr	m_SeverAddr;
	int				m_hSock;
};

class CJdStunBinding : public CJdStunTransact
{
public:
	int Run(const char *pszServer, int nFlag);
	int Stop();
};

class CJdStunDetect : public CJdStunTransact
{
public:
	/**
	 * Detects NAT type and obtins mapped external IP Address
	 * Set m_HostInternalAddr, m_usHostInternalPort for systems 
	 * with multiple interfaces.
	 */
	int GetNatType(
		const char *pszServer		// STUN Server
		);

	int GetNatType(
		const char *pszServer,						// STUN Server
		struct in_addr	HostInternalAddr,		// Internal address
		unsigned short	usHostInternalPort		// Internal port
		);

	/**
	 * Used for detecting detecting extenral address of the host
	 * for use with port forwarding
	 */
	int GetMappedAddr(
		const char		*pszStunServer,			// STUN Server
		struct in_addr	HostInternalAddr,		// Internal address
		unsigned short	usHostInternalPort,			// Internal Port
		struct in_addr	*pHostExternalAddr,		// External address
		unsigned short	*pusHostExternalPort		// External port
		);

private:
	int TestI(struct sockaddr_in *stunServe, CStunMsg *pResponse);
	int TestII(struct sockaddr_in *stunServer, CStunMsg *pResponse);
	int TestIII(struct sockaddr_in *stunServer, CStunMsg *pResponse);

};

#endif //__JD_STUN_H__