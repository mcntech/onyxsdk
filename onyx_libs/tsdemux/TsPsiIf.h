#ifndef __TS_PSI_IF__
#define __TS_PSI_IF__

#define EVENT_NEW_PMT             1
#define EVENT_PROGRAM_CHANGED     2

typedef struct _PMT_INF_T
{
	int nPmtPid;
} PMT_INF_T;


typedef int (*PsiCallback_T)(void *pCallerCtx, int EvenId, void *pData);

/**
 * Program specifi information
 */
class CTsPsiIf
{
public:
	CTsPsiIf(){}
	virtual ~CTsPsiIf(){}
	static CTsPsiIf *CreateInstance();
	static void CloseInstance(CTsPsiIf *pInstance);
	
	virtual int Parse(unsigned char *pData, int nLen) = 0;
    virtual int GetCountOfPrograms() = 0;
	virtual int GetRecordProgramNumber(int dwIndex, int * pwVal) = 0;
	virtual int GetCountOfElementaryStreams(int nProgramNumber) = 0;
	virtual int GetRecordProgramMapPid (int nIndex, int *nVal) = 0;
	virtual int GetElementaryPidByStreamType(int nProgNumber, int nStreamType) = 0;
	virtual int GetRecordProgramPcrPid(int nProgramNumber) = 0;
	virtual int IsTableComplete() = 0;
	virtual int RegisterCallback(void *pCallerCtx, PsiCallback_T fnCallback) = 0;
};

#endif //__TS_PSI_IF__