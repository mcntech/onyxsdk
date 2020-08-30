#ifndef __TS_MUX_H_
#define __TS_MUX_H_


#define  MAX_NUM_TS_STREAMS  100
#define SAMPLE_TYPE_H264	1
#define SAMPLE_TYPE_AAC     2

#define SAMPLE_FLAGS_TS_FRAME_START   1


class CCRC32{

	public:
		CCRC32(void);
		~CCRC32(void);

		unsigned long Generate(const unsigned char *sData, unsigned long ulDataLength);
		int Validate(const unsigned char *pData, unsigned long ulDataLength);

	private:
		unsigned long *ulTable; // CRC lookup table array.
};

class CBitstream{
    protected:
        char* m_pbs;		// m_pbs points to the current position of the buffer.
        char* m_pbsBase;	// m_pbsBase points to the first byte of the buffer.
        unsigned long m_bitOffset;  // Indicates the bit position (0 to 7) in the byte pointed by m_pbs.

    public:
        unsigned long  GetBsOffset();      // Returns the bit position of the buffer pointer relative to the beginning of the buffer.

        // Appends bits into the bitstream buffer.
        void  Init(char *pBs);
        unsigned long  Stop();
        void PutBit(unsigned long code);
        void PutBits(unsigned long code, unsigned long length);
};

class CTsMux
{
public:
    CTsMux( );
    
    int     Start();
    void    SetPacketsPackNumber( unsigned long packNum );
    void    SetPCRPTSDelay( unsigned long delay );
    int     Mux(CAccessUnit *pAu);
    int AddAPesHeader(CAccessUnit *pAu, char *pBuffer, int nPlLen);
    int AddVPesHeader(CAccessUnit *pAu, char *pBuffer, int nPlLen);
protected:
    CBitstream    m_BS;
	CCRC32        m_Crc;

private:
    void    GenerateTSHeader(unsigned char * localAddr, bool startPES, int pid, int priority);
    void    GeneratePESHeaderFlags(unsigned char * localAddr, int stream_id);
    void    GeneratePAT(unsigned char * localAddr);
    void    GeneratePMT(unsigned char * localAddr);
    void    GenerateTSNullPackets(unsigned char * localAddr);
    int     psi_packet(char * lmemAddr, char * localTSAddr, char * bitBufAddr, unsigned long length,  unsigned char continuity_counter); 
    int     pcr_packet(unsigned long long pts, char * bitBufAddr,  unsigned char continuity_counter);

    int     m_PESHeaderFlagsLen;
    int     m_TSHeaderLength;
    int     mPATLength;
    int     mPMTLength;    

    unsigned char mContinuity_counter_pes[MAX_NUM_TS_STREAMS][2];
    unsigned char mContinuity_counter_pat[MAX_NUM_TS_STREAMS];
    unsigned char mContinuity_counter_pmt[MAX_NUM_TS_STREAMS];
    unsigned char mContinuity_counter_pcr[MAX_NUM_TS_STREAMS];
    unsigned char mOnePacket[MAX_NUM_TS_STREAMS];

    unsigned long long  mPSISent[MAX_NUM_TS_STREAMS];
    unsigned long long  mPSIInterval[MAX_NUM_TS_STREAMS];

    unsigned long long mPCR[MAX_NUM_TS_STREAMS];
    unsigned long long mPCRStart[MAX_NUM_TS_STREAMS];
    unsigned long long mPrevPCR[MAX_NUM_TS_STREAMS];

    unsigned long long mPrevDTS[MAX_NUM_TS_STREAMS];
    unsigned long long m_PTS[MAX_NUM_TS_STREAMS];
    unsigned long long mPrevPTS[MAX_NUM_TS_STREAMS];
    unsigned long long m_DTS[MAX_NUM_TS_STREAMS];
    unsigned long long mPCRInterval[MAX_NUM_TS_STREAMS];
    
    bool          m_Init;
    unsigned long m_TSPackNum;
    unsigned long mPCR_PTS_Delay;
    bool          mAdaptationPrivDataEnable;
    //unsigned long mBytesCount[MAX_NUM_TS_STREAMS];
};

#endif  /* __TS_MUX_H_ */
