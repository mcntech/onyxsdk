#ifndef __ACCESSUNIT_H__
#define __ACCESSUNIT_H__


#define ACCESSUNIT_MAX_CTRL_SIZE     1024

class CAccessUnit
{
	public:
		CAccessUnit()
		{
			m_PTS = 0;
			m_DTS = 0;
			m_Flags = 0;
			m_SampleStreamID = 0;
			m_SampleStreamType = 0;
			m_TSPackNum = 0;
			m_CodingType=0;
			m_TSAudioOnly = false;
			m_TSPCRMaster = true;
		}
	public:
		unsigned long long  m_PTS;
		unsigned long long  m_DTS;

		char                *m_pRawData;
		unsigned long       m_RawSize;
		char                *m_pTsData;
		unsigned long       m_TsSize;

		unsigned long       m_Flags;
		unsigned short      m_SampleStreamID;
		unsigned short      m_SampleStreamType;
		unsigned long       m_ReservedBytes;
		unsigned char       m_TSPackNum;
		unsigned char       m_TSStreamID;
		bool                m_TSPCRMaster;
		bool                m_TSAudioOnly;
		unsigned char       m_CodingType;
};

#endif // __ACCESSUNIT_H__
