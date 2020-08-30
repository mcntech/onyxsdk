#ifndef __TS_PARSE_H__
#define __TS_PARSE_H__

#ifndef WIN32

#define BYTE unsigned char
#define WORD unsigned short
#define DWORD unsigned long
#define __int64 long long
#define UNALIGNED
#define ULONGLONG	long long
#endif

#ifndef BIG_ENDIAN
#define NTOH_LL(ll)                                             \
                    ( (((ll) & 0xFF00000000000000) >> 56)  |    \
                      (((ll) & 0x00FF000000000000) >> 40)  |    \
                      (((ll) & 0x0000FF0000000000) >> 24)  |    \
                      (((ll) & 0x000000FF00000000) >> 8)   |    \
                      (((ll) & 0x00000000FF000000) << 8)   |    \
                      (((ll) & 0x0000000000FF0000) << 24)  |    \
                      (((ll) & 0x000000000000FF00) << 40)  |    \
                      (((ll) & 0x00000000000000FF) << 56) )

#define NTOH_L(l)                                       \
                    ( (((l) & 0xFF000000) >> 24)    |   \
                      (((l) & 0x00FF0000) >> 8)     |   \
                      (((l) & 0x0000FF00) << 8)     |   \
                      (((l) & 0x000000FF) << 24) )

#define NTOH_S(s)                                   \
                    ( (((s) & 0xFF00) >> 8)     |   \
                      (((s) & 0x00FF) << 8) )

#else   //  BIG_ENDIAN
//  we're already big-endian, so we do nothing
#define NTOH_LL(ll)     (ll)
#define NTOH_L(l)       (l)
#define NTOH_S(s)       (s)

#endif  //  BIG_ENDIAN

#define HTON_L(l)       NTOH_L(l)
#define HTON_S(s)       NTOH_S(s)

//  i 0-based
#define BIT_VALUE(v,b)              ((v & (0x00 | (1 << b))) >> b)
#define BYTE_OFFSET(pb,i)           (& BYTE_VALUE((pb),i))
#define BYTE_VALUE(pb,i)            (((BYTE *) (pb))[i])
#define WORD_VALUE(pb,i)            (* (UNALIGNED WORD *) BYTE_OFFSET((pb),i))
#define DWORD_VALUE(pb,i)           (* (UNALIGNED DWORD *) BYTE_OFFSET((pb),i))
#define ULONGLONG_VALUE(pb,i)       (* (UNALIGNED ULONGLONG *) BYTE_OFFSET((pb),i))

#define PID_VALUE(pb)				(NTOH_S(WORD_VALUE(pb,1)) & 0x00001fff)

#define SYNC_BYTE_VALUE(pb)                             (BYTE_VALUE((pb),0))
#define TRANSPORT_ERROR_INDICATOR_BIT(pb)               ((NTOH_S(WORD_VALUE(pb,1)) & 0x00008000) >> 15)
#define PAYLOAD_UNIT_START_INDICATOR_BIT(pb)            ((NTOH_S(WORD_VALUE(pb,1)) & 0x00004000) >> 14)
#define TRANSPORT_PRIORITY_BIT(pb)                      ((NTOH_S(WORD_VALUE(pb,1)) & 0x00002000) >> 13)
#define PID_VALUE(pb)                                   (NTOH_S(WORD_VALUE(pb,1)) & 0x00001fff)
#define TRANSPORT_SCRAMBLING_CONTROL_VALUE(pb)          ((BYTE_VALUE((pb),3) & 0x000000c0) >> 6)
#define ADAPTATION_FIELD_CONTROL_VALUE(pb)              ((BYTE_VALUE((pb),3) & 0x00000030) >> 4)
#define CONTINUITY_COUNTER_VALUE(pb)                    (BYTE_VALUE((pb),3) & 0x0000000f)


#define ADAPTATION_FIELD_LENGTH_VALUE(pb)               (BYTE_VALUE((pb),0))
#define DISCONTINUITY_INDICATOR_BIT(pb)                 ((BYTE_VALUE((pb),1) & 0x00000080) >> 7)
#define RANDOM_ACCESS_INDICATOR_BIT(pb)                 ((BYTE_VALUE((pb),1) & 0x00000040) >> 6)
#define ELEMENTARY_STREAM_PRIORITY_INDICATOR_BIT(pb)    ((BYTE_VALUE((pb),1) & 0x00000020) >> 5)
#define PCR_FLAG_BIT(pb)                                ((BYTE_VALUE((pb),1) & 0x00000010) >> 4)
#define OPCR_FLAG_BIT(pb)                               ((BYTE_VALUE((pb),1) & 0x00000008) >> 3)
#define SPLICING_POINT_FLAG_BIT(pb)                     ((BYTE_VALUE((pb),1) & 0x00000004) >> 2)
#define TRANSPORT_PRIVATE_DATA_FLAG_BIT(pb)             ((BYTE_VALUE((pb),1) & 0x00000002) >> 1)
#define ADAPTATION_FIELD_EXTENSION_FLAG_BIT(pb)         (BYTE_VALUE((pb),1) & 0x00000001)

//  pb points to PCR_BASE block
#define PCR_BASE_VALUE(pb)                              (NTOH_LL(ULONGLONG_VALUE(pb,0)) >> 31)
#define PCR_EXT_VALUE(pb)                               ((DWORD) (NTOH_LL(ULONGLONG_VALUE(pb,0)) & 0x00000000000001ff))

//  pb points to OPCR_BASE block
#define OPCR_BASE_VALUE(pb)                             PCR_BASE_VALUE(pb)
#define OPCR_EXT_VALUE(pb)                              PCR_EXT_VALUE(pb)

//  pb points to splice_countdown block
#define SPLICE_COUNTDOWN_VALUE(pb)                      BYTE_VALUE(pb,0)

//  pb points to transport_private_data_length
#define TRANSPORT_PRIVATE_DATA_LENGTH_VALUE(pb)         BYTE_VALUE(pb,0)

//  pb points to adaptation_field_extension_length
#define ADAPTATION_FIELD_EXTENSION_LENGTH_VALUE(pb)     BYTE_VALUE(pb,0)

//  increments and takes care of the wrapping case
#define INCREMENTED_CONTINUITY_COUNTER_VALUE(c) ((BYTE) (((c) + 1) & 0x0000000f))


#define PRIVATE_SECTION_SYNTAX_INDICATOR_BIT(pb)        PSI_SECTION_SYNTAX_INDICATOR_BIT(pb)

//  pb points to prefix i.e. pb = { 0x00, 0x00, 0x01, ....}
#define START_CODE_PREFIX_VALUE(pb)                     (NTOH_L(DWORD_VALUE(pb,0)) >> 8)
#define START_CODE_VALUE(pb)                            (BYTE_VALUE(pb,3))
#define PACKET_LENGTH_VALUE(pb)                         (NTOH_S(WORD_VALUE(pb,4)))

#define PACK_START_CODE_VALUE(pb)                       NTOH_L(DWORD_VALUE(pb,0))
#define PACK_HEADER_SCR_BASE(pb)                        (((NTOH_LL(ULONGLONG_VALUE(pb,1)) & 0x000000000003FFF8) >> 3) | \
                                                         ((NTOH_LL(ULONGLONG_VALUE(pb,1)) & 0x00000003FFF80000) >> 4) | \
                                                         ((NTOH_LL(ULONGLONG_VALUE(pb,1)) & 0x0000003800000000) >> 5))
#define PACK_HEADER_SCR_EXT(pb)                         ((NTOH_LL(ULONGLONG_VALUE(pb,2)) & 0x00000000000003FE) >> 1)
#define PACK_PROGRAM_MUX_RATE(pb)                       (((NTOH_L(DWORD_VALUE(pb,10)) >> 8) & 0xFFFFFC0) >> 2)
#define PACK_STUFFING_LENGTH(pb)                        (BYTE_VALUE(pb,13) & 0x03)


#define PES_PACKET_START_CODE_PREFIX_VALUE(pb)          START_CODE_PREFIX_VALUE(pb)
#define PES_STREAM_ID_VALUE(pb)                         START_CODE_VALUE(pb)
#define PES_PACKET_LENGTH_VALUE(pb)                     PACKET_LENGTH_VALUE(pb)

//  pb points to TIER_1 header
#define PES_SCRAMBLING_CONTROL_VALUE(pb)                ((BYTE_VALUE(pb,0) & 0x30) >> 4)
#define PES_PRIORITY_BIT(pb)                            BIT_VALUE(BYTE_VALUE(pb,0),3)
#define PES_DATA_ALIGNMENT_INDICATOR_BIT(pb)            BIT_VALUE(BYTE_VALUE(pb,0),2)
#define PES_COPYRIGHT_BIT(pb)                           BIT_VALUE(BYTE_VALUE(pb,0),1)
#define PES_ORIGINAL_OR_COPY_BIT(pb)                    BIT_VALUE(BYTE_VALUE(pb,0),0)
#define PES_PTS_DTS_FLAGS_VALUE(pb)                     ((BYTE_VALUE(pb,1) & 0xc0) >> 6)
#define PES_ESCR_FLAG_BIT(pb)                           BIT_VALUE(BYTE_VALUE(pb,1),5)
#define PES_ES_RATE_FLAG_BIT(pb)                        BIT_VALUE(BYTE_VALUE(pb,1),4)
#define PES_DSM_TRICK_MODE_FLAG_BIT(pb)                 BIT_VALUE(BYTE_VALUE(pb,1),3)
#define PES_ADDITIONAL_COPY_INFO_FLAG_BIT(pb)           BIT_VALUE(BYTE_VALUE(pb,1),2)
#define PES_CRC_FLAG_BIT(pb)                            BIT_VALUE(BYTE_VALUE(pb,1),1)
#define PES_EXTENSION_FLAG_BIT(pb)                      BIT_VALUE(BYTE_VALUE(pb,1),0)
#define PES_PES_HEADER_DATA_LENGTH_VALUE(pb)            BYTE_VALUE(pb,2)

//  pb points to the PTS field
#define PES_PTS_VALUE(pb)                               ((((NTOH_LL(ULONGLONG_VALUE(pb,0)) >> 24) & 0x000000000000FFFE) >> 1) |   \
                                                         (((NTOH_LL(ULONGLONG_VALUE(pb,0)) >> 24) & 0x00000000FFFE0000) >> 2) |   \
                                                         (((NTOH_LL(ULONGLONG_VALUE(pb,0)) >> 24) & 0x0000000E00000000) >> 3))

#define PES_PTS_VALUE_LO(pb)         (                                         \
									 ((((unsigned long)pb[0] & 0x00000006) >> 1) << 30) |   \
									 ((( unsigned long)pb[1] & 0x000000FF      ) << 22) |   \
									 ((((unsigned long)pb[2] & 0x000000FE) >> 1) << 15) |   \
									 ((( unsigned long)pb[3] & 0x000000FF      ) << 7 ) |   \
									 ((( unsigned long)pb[4] &  0x000000FE  >> 1))      \
									 ) 

#define PES_PTS_VALUE_HI(pb)         ((unsigned long)(pb[0] >> 3) & 0x00000001)

#define PES_DTS_VALUE(pb)                               PES_PTS_VALUE(pb)

#define PES_MV_SUBSTREAM_ID(pb)                         BYTE_VALUE(pb,PES_COMMON_HEADER_FIELD_LEN)
#define PES_MV_BITS(pb)                                 ((NTOH_S (WORD_VALUE(pb,MACROVISION_WORD_OFFSET))) >> 14)


#define TS_PKT_LEN	188
#define MAX_PID_COUNT	10

class CPidMap
{
public:
	CPidMap()
	{
		m_lCount = 0;
		m_lCc = 0;
	}
	CPidMap(long lPid)
	{
		m_lCount = 0;
		m_lCc = 0;
		m_PidMap[m_lCount++] = lPid;
	}

	void Add(long lPid)
	{
		if(m_lCount < MAX_PID_COUNT)
			m_PidMap[m_lCount++] = lPid;
	}
	int IsExist(long lPid)
	{
		for (int i=0; i < m_lCount; i++){
			if(m_PidMap[i] == lPid)
				return 1;
		}
		return 0;
	}
	int CheckAndUpdateCc(unsigned char *pTsPkt)
	{
		long lCc = CONTINUITY_COUNTER_VALUE(pTsPkt);
		if(INCREMENTED_CONTINUITY_COUNTER_VALUE(m_lCc) == lCc){
			m_lCc = lCc;
			return 1;
		}
		return 0;
	}
private:	
	long	m_lCount;
	long	m_PidMap[MAX_PID_COUNT];
	long	m_lCc;
};

class CTsParse
{
public:
	static void DumpHex(unsigned char *pData, int nCount)
	{
		for (int i =0; i <nCount;i++)
			fprintf(stderr," %2x",pData[i]);
		fprintf(stderr,"\n");
	}
	static unsigned long GetPcr(unsigned char* pTsPkt, long lLen)
	{
		long long llPcr = 0;
		unsigned long ulPcr = 0;

		while (lLen > 0) {
			long lPid = PID_VALUE(pTsPkt);
			int pos = 0;
			
			/* Check Adaptation field present and move the pos, bt adaptation field lenght, if required*/
			if ((0x20 & pTsPkt[pos+3]) == 0x20) {
				pos += pTsPkt[pos+4] + 1;
			
				__int64 llPcrBase = 0;
				DWORD lPcrExt = 0;
				BYTE *pAdapt = pTsPkt + 4;
				if((ADAPTATION_FIELD_CONTROL_VALUE(pTsPkt) == 0x02)&& PCR_FLAG_BIT(pAdapt)){
					llPcrBase = PCR_BASE_VALUE(pAdapt+2);
					lPcrExt = PCR_EXT_VALUE(pAdapt+2);
					llPcr = (llPcrBase << 33) | lPcrExt;
					ulPcr = llPcrBase;
					break;
				}
			}
			lLen -= TS_PKT_LEN;
			pTsPkt += TS_PKT_LEN;
		}
		return ulPcr;
	}
	
	static int HasSps(unsigned char *pData, long lDataLen, unsigned long *pulPtsHi, unsigned long *pulPtsLo)
	{

		long lPid = PID_VALUE(pData);
		int pos = 0;
		*pulPtsHi = 0;
		*pulPtsLo = 0;
		/* Check Adaptation field present and move the pos, bt adaptation field lenght, if required*/
		if ((0x20 & pData[pos+3]) == 0x20) {
			pos += pData[pos+4] + 1;
		}
		pData += (pos + 4);
		lDataLen -= (pos + 4);
		/* packet start code prefix and stream id*/
		if( pData[0] == 0x00 && pData[1] == 0x00 && pData[2] == 0x01 && pData[3] == 0xE0) {
			int i = 0;

			/* Move the Data pointer to start of PES HEADER */
			pData += 6;
			lDataLen -= 6;

			if((PES_PTS_DTS_FLAGS_VALUE(pData)& 0x02)){
				char *pb = (char *)pData+3;
				*pulPtsHi = PES_PTS_VALUE_HI(pb);
				*pulPtsLo = PES_PTS_VALUE_LO(pb);
			}

			long lPesHdrLen = PES_PES_HEADER_DATA_LENGTH_VALUE(pData);
			
			lDataLen -= (lPesHdrLen + 3);
			pData += lPesHdrLen + 3;
			while(i < lDataLen - 3) {
				if (pData[i] == 0x00 && pData[i+1] == 0x00 && pData[i+2] == 0x01 && (pData[i+3] & 0x1F) == 0x07) {
					return i;
				} else {
					i++;
				}
			}
		}
		return 0;
	}
};
#endif //__TS_PARSE_H__