#ifndef __HLS_PUBLIS_BASE_H__
#define __HLS_PUBLIS_BASE_H__


typedef struct _HLS_PUBLISH_STATS
{
	int               nState;
	int               nSegmentTime;
	int               nLostBufferTime;
	int               nStreamInTime;
	int               nStreamOutTime;
} HLS_PUBLISH_STATS;

class CHlsPublishBase
{
public:
	CHlsPublishBase();
	virtual int ReceiveGop(int nGopNum, const char *pData, int nLen, int nDurarionMs) = 0;
	virtual void Run() = 0;
	virtual void Stop() = 0;
	virtual int GetStats(HLS_PUBLISH_STATS *pStats);

protected:
	int                 m_nError;
	int                 m_nLostBufferTime;
	int                 m_nSegmentTime;
	int                 m_nInStreamTime;
	int                 m_nOutStreamTime;

};
#endif // __HLS_PUBLIS_BASE_H__
