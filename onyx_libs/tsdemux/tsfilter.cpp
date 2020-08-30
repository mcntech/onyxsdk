#ifdef WIN32
#include <Windows.h>
#include <io.h>
#else
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#define O_BINARY 0
#endif

#include <assert.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>

#include "tsfilter.h"
#include "TsDemuxIf.h"
#include "TsPsiIf.h"
//#include "AvParse.h"
#include "AdtsParse.h"
#include "ts_dbg.h"

#define DEMUX_NEED_MORE_DATA		0x80000001
#define DEMUX_EOF					0x00000000
#define DEMUX_OK					0x00000001
//#define EN_TS
#define DEFAULT_PROGRAM_NUM			1

#define countof(array) (sizeof(array)/sizeof(array[0]))
// Setup data


//==========================================================

CTsFilter *CTsFilter::CreateInstance(long *phr)
{
	CTsFilter *pFilter = new CTsFilter(phr);
	return pFilter;
}

CTsFilter::CTsFilter(long *phr)
{
	//m_pClock = new CStrmClock( NAME(""), GetOwner(), phr );

	m_lVidWidth = 0;
	m_lVidHeight = 0;
	m_ProgramNum = DEFAULT_PROGRAM_NUM;
	m_pDemux = CTsDemuxIf::CreateInstance();
	m_TsBuffer = new CTsbuffer(188 * 1024);
	m_pTsPsi = CTsPsiIf::CreateInstance();
	m_hFile = NULL;
	m_fLoop = 1;
	m_ullTotalBytes = 0;
	m_iPins = 0;
}

CTsFilter::~CTsFilter()
{
	DeinitFileSrc();
	//if(m_pClock) {
	//	delete m_pClock;
	//}
	if(m_TsBuffer)
		delete m_TsBuffer;
	if(m_pDemux) {
		delete m_pDemux;
		m_pDemux = NULL;
	}
	if(m_pTsPsi) {
		delete m_pTsPsi;
		m_pTsPsi = NULL;
	}
}

int CTsFilter::CreateH264OutputPin()
{
	return 0;
}

int CTsFilter::CreateMP4AOutputPin()
{
	long hr = 0;
	return hr;
}


int CTsFilter::Stop()
{
	DbgOut(DBGLVL_TRACE,("%s: Enter",__FUNCTION__));
	for (int i=0; i < m_iPins; i++) {
		ITrackAssocInf *pStrm = (ITrackAssocInf *)(m_paStreams[i]);
		m_paStreams[i]->Stop();
	}

	if(m_hFile)
		lseek(m_hFile, SEEK_SET, 0);
	if(m_pDemux)
		m_pDemux->Reset();

	OnThreadDestroy();

	for (int i=0; i < m_iPins; i++) {
		ITrackAssocInf *pStrm = (ITrackAssocInf *)(m_paStreams[i]);
		pStrm->resetSampleQueue();
	}

	DbgOut(DBGLVL_TRACE,("%s: Leave",__FUNCTION__));
	return 0;
}

int CTsFilter::Pause()
{
	int hr = 0;
	DbgOut(DBGLVL_TRACE,("%s: Enter",__FUNCTION__));
	// TODO	
	DbgOut(DBGLVL_TRACE,("%s: Leave",__FUNCTION__));
	return hr;
}

int CTsFilter::Run(unsigned long long tStart)
{
	m_fRun = 1;
#ifdef WIN32
	DWORD dwThreadId;
	m_hThread = CreateThread( NULL, 0, (LPTHREAD_START_ROUTINE)DoBufferProcessing, this, 0, &dwThreadId );
#else
#endif
	return 0;
}


#define TS_PKT_SIZE			188
#define MAX_PROBE_LEN		(TS_PKT_SIZE * 240000)
/* Initialize PAT and PMT tables */
int CTsFilter::InitFileSrc(char *pszFileName)
{
	unsigned char TsPkt[TS_PKT_SIZE];
	unsigned long dwFileOffset = 0;

#ifdef WIN32
	m_hFile = open (pszFileName, O_RDONLY|O_BINARY, 0);
#else
	m_hFile = open (pszFileName, O_RDONLY, 0);
#endif

	if(m_hFile == -1){
		DbgOut(DBGLVL_ERROR,("Could not open file %s", pszFileName));
		return -1;
	}
	CTsDemuxIf *pDemux = m_pDemux;
	while(dwFileOffset < MAX_PROBE_LEN ){
		unsigned long dwBytesRead = 0;
		dwBytesRead = read(m_hFile, TsPkt, TS_PKT_SIZE);
		if(dwBytesRead == 0)
			break;
		m_pTsPsi->Parse(TsPkt, TS_PKT_SIZE);
		dwFileOffset += dwBytesRead;
		if(m_pTsPsi->IsTableComplete())
			break;
	}

	DbgOut(DBGLVL_TRACE,("Seach len=%d Programs=%d", dwFileOffset, m_pTsPsi->GetCountOfPrograms()));
	for (int i= 0; i <  m_pTsPsi->GetCountOfPrograms(); i++) {
		int nProg = 0;
		m_pTsPsi->GetRecordProgramNumber(i, &nProg);
		if(nProg) {
			DbgOut(DBGLVL_TRACE,("Program=%d ElemStreams=%d", nProg, m_pTsPsi->GetCountOfElementaryStreams(nProg)));
		} else {
			DbgOut(DBGLVL_TRACE,("No prgram found at index %d ", i));
		}
	}
	lseek( m_hFile, SEEK_SET, 0 ); 
	pDemux->Reset();
	return 0;
}

int CTsFilter::InitProgram(int nProgram, CH264Stream **ppVidSrc, CAacStream **ppAudSrc)
{
	unsigned char TsPkt[TS_PKT_SIZE];
	unsigned long dwFileOffset = 0;
	int fAudFoud = false;
	int fVidFound = false;
	CTsDemuxIf *pDemux = m_pDemux;
	CTsPsiIf *pTsPsi = m_pTsPsi;
	if(m_pTsPsi->GetCountOfElementaryStreams(nProgram) > 0) {
		int nVPid = pTsPsi->GetElementaryPidByStreamType(nProgram, PSI_STREM_TYPE_H264);
		if(nVPid > 0) {
			pDemux->EnableElemStream(nVPid, CTsDemuxIf::CODEC_ID_H264);
		}
		int nAPid = pTsPsi->GetElementaryPidByStreamType(nProgram, PSI_STREM_TYPE_AAC_ADTS);
		if(nAPid) {
			pDemux->EnableElemStream(nAPid, CTsDemuxIf::CODEC_ID_AAC);
		}
		int nPcrPid = pTsPsi->GetRecordProgramPcrPid(nProgram);
		if(nPcrPid){
			pDemux->EnablePcrProcessing(nPcrPid);
		}

		lseek(m_hFile, SEEK_SET, 0 ); 
		dwFileOffset = 0;
		int fGetNewData = 1;
		unsigned long dwBytesRead = 0;
		while(dwFileOffset < MAX_PROBE_LEN && (!fAudFoud || !fVidFound)) {
			if(fGetNewData){
				dwBytesRead = read(m_hFile, TsPkt, TS_PKT_SIZE);
				if(dwBytesRead == 0)
					break;
				dwFileOffset += dwBytesRead;
			}
			CAvFrame *pAvFrame = pDemux->ParsePkt(TsPkt);
			if(pAvFrame) {
				fGetNewData = 0;
			} else {
				fGetNewData = 1;
			}

			if(pAvFrame && pAvFrame->mTsPid == nVPid && !fVidFound){
#if 0
				if(h264_decode_h264_bytestream_hdr((char *)pAvFrame->mFrmData, (long)pAvFrame->mUsedSize,&m_lVidWidth, &m_lVidHeight) == 1)
#endif
				int i = 0;
				int spsStart = 0;
				unsigned char *pData = (unsigned char *)pAvFrame->mFrmData;
				while(i < pAvFrame->mUsedSize - 12){
					if(pData[i] == 0x00 && pData[i+1] == 0x00 && pData[i+2] == 0x01 && (pData[i+3] & 0x1F) == 0x07){
						spsStart = i+3;
						break;
					} else {
						i++;
					}
				}
				if(spsStart) {
					long hr = 0;
					int j = spsStart + 1;
					while(j < pAvFrame->mUsedSize - 4){
						if(pData[j] == 0x00 && pData[j+1] == 0x00 && pData[j+2] == 0x01 && (pData[j+3] & 0x1F) == 0x08){

							CH264Stream *pStreamV = new CH264Stream(&hr, this);
							m_paStreams[m_iPins++] = pStreamV;
							*ppVidSrc = pStreamV;
							int nPin = m_iPins - 1;
							ITrackAssocInf *pStrm = (ITrackAssocInf *)(m_paStreams[nPin]);
							pStrm->setTrackId(nVPid);
							fVidFound = true;
							pStreamV->m_nSpsSize = j - spsStart;
							memcpy(pStreamV->m_Sps, pAvFrame->mFrmData + spsStart, pStreamV->m_nSpsSize);
							break;
						} else {
							j++;
						}
					}
				}
				pAvFrame->Reset();
			} else if(pAvFrame && pAvFrame->mTsPid == nAPid && !fAudFoud){
				long hr = 0;
				AdtsParse::getAacConfigFromAdts((unsigned char *)AacCfgRecord.cfgRec, (unsigned char *)pAvFrame->mFrmData);
				AacCfgRecord.cfgLen = 2;
				m_AudSampleRate = AdtsParse::GetAudSamplerateFromAdt((unsigned char *) pAvFrame->mFrmData);
				m_AudNumChannels = AdtsParse::GetAudNumChanFromAdt((unsigned char *)pAvFrame->mFrmData);
				CAacStream *pStreamA = new CAacStream(&hr, this);
				m_paStreams[m_iPins++] = pStreamA;
				*ppAudSrc = pStreamA;
				int nPin = m_iPins - 1;
				ITrackAssocInf *pStrm = m_paStreams[nPin];
				pStrm->setTrackId(nAPid);
				fAudFoud = true;
				pAvFrame->Reset();
			} else if(pAvFrame) {
				// Ignore
				pAvFrame = NULL;
				fGetNewData = 1;
			}
		}

	} else {
		DbgOut(DBGLVL_ERROR,("Could not find programs"));
	}
	if(m_hFile >= 0) {
		lseek( m_hFile, SEEK_SET, 0 ); 
	}
	pDemux->Reset();

	if(fVidFound)
		return 0;
	else
		return -1;
}

int CTsFilter::DeinitFileSrc()
{
	return 0;
}


int CTsFilter::OnThreadDestroy(void)
{
	m_fRun = 0;
#ifdef WIN32
	//WaitForSingleObject(m_hThread,1000);
#else
#endif
    return 0;
}


void *CTsFilter::DoBufferProcessing(void *pArg)
{
	void *res;
	CTsFilter *pFilter = (CTsFilter *)pArg;

	while(pFilter->m_fRun) {
		if(pFilter->DemuxStream() < 0){
			break;
		}
	}
	/* Set EoF on all the streams */
	for (int i=0; i < pFilter->m_iPins; i++) {
		ITrackAssocInf *pStrm = (ITrackAssocInf *)(pFilter->m_paStreams[i]);
		pStrm->setTrackEoF();
	}

    return &res;
}


long CTsFilter::DemuxStream()
{
	unsigned long dwBytesRead = 0;
	long lQboxBytes = 0;
	long lResult = 0;
	// Find Stream Type
	long lStrmType = 0;
	long fEof = 0;
	CTsFilter *pFilter = this;
	int fDone = 0;
	CAvSample *pSample = NULL;
	ITrackAssocInf *pCrntStrm = NULL;
	CAvFrame *pAvFrame = NULL;
	int     fEoF = false;
	int    fDisCont = false;
	
	CTsDemuxIf *pDemux = m_pDemux;
	/* ontinue parsing previous data */
	pAvFrame = pDemux->Parse(m_TsBuffer);

	/*If no frame found, fetch new data and parse */
	while(pAvFrame == NULL && !fEoF){
		unsigned long dwBytesRead = 0;
		dwBytesRead = read(m_hFile, m_TsBuffer->mData, m_TsBuffer->mMaxSize);
		if(dwBytesRead <= 0) {
			if(m_fLoop) {
				lseek( m_hFile, SEEK_SET, 0 ); 
				dwBytesRead = read(m_hFile, m_TsBuffer->mData, m_TsBuffer->mMaxSize);
				fDisCont = true;
			} else {
				fEoF = true;
			}
		}
		
		if(!fEoF) {
			m_TsBuffer->mRdOffset = 0;
			m_TsBuffer->mUsedSize = dwBytesRead;
			pAvFrame = pDemux->Parse(m_TsBuffer);
		}
		m_ullTotalBytes += dwBytesRead;
	}
	if(fEoF)
		return -1;
	// Get AvSample Queue for the codec
	for (int i=0; i < m_iPins; i++) {
		ITrackAssocInf *pStrm = (ITrackAssocInf *)(m_paStreams[i]);
		if(pStrm->getTrackId() == pAvFrame->mTsPid){
			pCrntStrm = pStrm;
			break;
		}
	}

	if((pCrntStrm ==  NULL) || !pCrntStrm->IsStreaming()){
		// Dicard data
		goto Skip;
	}

	if(pCrntStrm){
		while(((pSample = pCrntStrm->getEmptySample()) == NULL) && pFilter->m_fRun){
#ifdef WIN32
			Sleep(1);
#else
			usleep(100);
#endif
		}
	}

	if(pSample) {
		memcpy(pSample->m_pData, pAvFrame->mFrmData, pAvFrame->mUsedSize);
		pSample->m_lUsedLen = pAvFrame->mUsedSize;
		pSample->m_lPts = pAvFrame->mPts;
		pSample->m_lDts = pAvFrame->mDts;
		pCrntStrm->putFilledSample(pSample);
		pSample->m_fDisCont = fDisCont;
		if(fDisCont) {
			fDisCont = 0;
		}
		DbgOut(DBGLVL_TRACE,("%s: nPId=%d len=%d pts=%lld, dts=%lld",__FUNCTION__,pAvFrame->mTsPid, pSample->m_lUsedLen,  pSample->m_lPts/10000, pSample->m_lDts/10000));
	}

Skip:
	if(pAvFrame) {
		pAvFrame->Reset();
	}
	return lResult;
}

//========================================================
CH264Stream::CH264Stream(long *phr,  CTsFilter *pParent)
{
	m_lOutputType = OUTPUT_TYPE_NALU;
	for (int i = 0; i < JB_PKT_COUNT; i++) {
		m_EmptyQueue.push(new CAvSample(JB_PKT_SIZE));
	}
	m_fEof = 0;
	m_nTrackId = -1;
}


CH264Stream::~CH264Stream()
{
	CAvSample *pSample;
	while (!m_EmptyQueue.empty()){
		pSample = m_EmptyQueue.front();
		m_EmptyQueue.pop();
		delete pSample;
	}
	while (!m_FilledQueue.empty()){
		pSample = m_FilledQueue.front();
		m_FilledQueue.pop();
		delete pSample;
	}
}

//
// FillBuffer
// overriding base class member
//
int CH264Stream::FillBuffer(void *pms)
{
#if 0
    BYTE *pData;
	CTsFilter *pParent = (CTsFilter *)m_pFilter;
    pms->GetPointer(&pData);
    long lSize = pms->GetSize();

	CAvSample *pSample = NULL;
	while(m_FilledQueue.size() == 0){
			Sleep(1);
	}
	if(!m_FilledQueue.empty()) {
		pSample = m_FilledQueue.front();
		m_FilledQueue.pop();
	}
	if(pSample){
		static long lTotal = 0;
		long lDataLen = pSample->m_lUsedLen;
		assert(pSample->m_lUsedLen < lSize);
		memcpy(pData, pSample->m_pData,lDataLen );
		pms->SetActualDataLength(lDataLen);

		REFERENCE_TIME rtStart = pSample->m_lPts * 1000 / 9; // Convert 90kHz to 100ns

		if(pParent->m_timeStreamStart <= 0){
			pParent->m_timeStreamStart = rtStart;
		} else {
			rtStart -= pParent->m_timeStreamStart;
		}

		int frameDuration = (double)300 * 1000 / 9;
		REFERENCE_TIME rtEnd = rtStart + frameDuration;
		pms->SetTime(&rtStart, &rtEnd);
		if(pSample->m_fDisCont) {
			pms->SetDiscontinuity(true);
		}
		m_EmptyQueue.push(pSample);
		lTotal += lDataLen;
		//DbgOutL(3,"%d Total=%d",lDataLen, lTotal);
	}else {
		return S_FALSE;
	}
#endif
	return 0;

} // FillBuffer

//===========================================================

CAacStream::CAacStream(long *phr, CTsFilter *pParent)
{
	m_lOutputType = OUTPUT_TYPE_NALU;
	m_pQ2nbuff = (unsigned char *)malloc(MAX_DEMUX_BUFF);

	for (int i = 0; i < AAC_BUFFERED_FRAME_COUNT; i++) {
		m_EmptyQueue.push(new CAvSample(AAC_MAX_FRAME_SIZE));
	}
	m_fEof = 0;
	m_nTrackId = -1;
}


CAacStream::~CAacStream()
{
	if(m_pQ2nbuff)
		free(m_pQ2nbuff);
}

//
// FillBuffer
// overriding base class member
//
int CAacStream::FillBuffer(void *pms)
{
#if 0
	CTsFilter *pParent = (CTsFilter *)m_pFilter;
    BYTE *pData;
    pms->GetPointer(&pData);
    long lSize = pms->GetSize();
    //CAutoLock cAutoLockShared(&m_cSharedState);
	CAvSample *pSample;
	while(m_FilledQueue.size() == 0){
		if((CheckRequest(&com) && com == CMD_STOP) || m_fEof)
			return S_FALSE;
		else
			Sleep(1);
	}
	if(!m_FilledQueue.empty()) {
		pSample = m_FilledQueue.front();
		m_FilledQueue.pop();
	}
	if(pSample){
		static long lTotal = 0;
		long lDataLen = pSample->m_lUsedLen;
		assert(pSample->m_lUsedLen < lSize);
		int nHdrLen = AdtsParse::getAdtsHdrLen( (unsigned char *)pSample->m_pData);
		if(nHdrLen < lDataLen) {
			memcpy(pData, pSample->m_pData + nHdrLen, lDataLen - nHdrLen );
			pms->SetActualDataLength(lDataLen - nHdrLen);
		}

		REFERENCE_TIME rtStart = (REFERENCE_TIME)pSample->m_lPts * 1000 / 9; // Convert 90kHz to 100ns

		if(pParent->m_timeStreamStart <= 0){
			pParent->m_timeStreamStart = rtStart;
		} else {
			rtStart -= pParent->m_timeStreamStart;
		}
		int frameDuration = (double)1024 * 10000000 / pParent->m_AudSampleRate;
		REFERENCE_TIME rtEnd = rtStart + frameDuration;
		pms->SetTime(&rtStart, &rtEnd);

		m_EmptyQueue.push(pSample);
		lTotal += lDataLen;
		DbgOut(3,("%d Total=%d",lDataLen, lTotal));
	}else {
		return S_FALSE;
	}
#endif
	return 0;
}

