#ifdef WIN32
#include <Windows.h>
#endif
#include <memory.h>
#include <stdio.h>
#include "AccessUnit.h"
#include "SimpleTsMux.h"
#include <stdio.h>
#include <stdarg.h>


#define EN_AUDIO

#define MUX_BUFF_SIZE              1024
#define LMEM_SYS_MUX_TS_BASE       muxBuffer
#define PES_HEADER_FLAGS_VIDEO  (LMEM_SYS_MUX_TS_BASE + 0x00)
#define PES_HEADER_FLAGS_AUDIO  (LMEM_SYS_MUX_TS_BASE + 0x80)
#define TS_HEADER_ADDRESS_UNIT_START_VIDEO (LMEM_SYS_MUX_TS_BASE + 0x100)
#define TS_HEADER_ADDRESS_UNIT_CONTINUE_VIDEO (LMEM_SYS_MUX_TS_BASE + 0x104)
#define TS_HEADER_ADDRESS_UNIT_START_AUDIO (LMEM_SYS_MUX_TS_BASE + 0x108)
#define TS_HEADER_ADDRESS_UNIT_CONTINUE_AUDIO (LMEM_SYS_MUX_TS_BASE + 0x10c)
#define TS_HEADER_ADDRESS_UNIT_START_PCM (LMEM_SYS_MUX_TS_BASE + 0x110)
#define TS_HEADER_ADDRESS_UNIT_CONTINUE_PCM (LMEM_SYS_MUX_TS_BASE + 0x114)
#define TS_HEADER_ADDRESS_UNIT_PTS (LMEM_SYS_MUX_TS_BASE + 0x118)

#define TS_HEADER_ADDRESS_UNIT_START_PAT (LMEM_SYS_MUX_TS_BASE + 0x11c)
#define TS_HEADER_ADDRESS_UNIT_START_PMT (LMEM_SYS_MUX_TS_BASE + 0x120)
#define PAT_TABLE_ADDRESS   (LMEM_SYS_MUX_TS_BASE + 0x124)
#define PMT_TABLE_ADDRESS   (LMEM_SYS_MUX_TS_BASE + 0x180)


#define TS_NULL_PACKETS     (LMEM_SYS_MUX_TS_BASE + 0x210)



//#define ReservedBytes  400 
#define TS_PACKET_SIZE        188
//Fixme: hardcode the stream id for now
#define VIDEO_STREAM_ID       0xE0
#define AUDIO_STREAM_ID       0xC0
#define PCM_STREAM_ID         0xBF // PES packets containing private data
#define TRANSPORT_STREAM_ID   0x1
#define PROGRAM_NUMBER        0x1


#define PROGRAM_ASSOCIATION_TABLE_PID        0x0
#define PROGRAM_MAP_TABLE_PID                0x100
#define VIDEO_ELEMENTARY_PID                 0x1011
#define AUDIO_ELEMENTARY_PID                 0x1012
#define PCM_ELEMENTARY_PID                   0x1013
#define SYSTEM_PCR_PID                       0x1001
#define NULL_PACKET_PID                      0x1FFF

#define PSI_PERIOD  200 * 90000 / 1000    // 200 ms
#define PCR_PERIOD  100 * 90000 / 1000    // 100 ms
//#define PCR_PTS_DELAY  200 * 90000/ 1000  // 200 ms
#define TRANSFER_RATE        10000000

#define USB_HEADER_EOH       0x80
#define USB_HEADER_EOF       0x2


#define VIDEO_TS_PACKET    0x0
#define AUDIO_TS_PACKET    0x1

#define VIDEO_PES_HEADER_LENGTH 19
#define AUDIO_PES_HEADER_LENGTH 16

enum DBG_LVL_T 	{ LVL_ERR, LVL_WARN, LVL_SETUP, LVL_MSG, LVL_TRACE,	LVL_STRM };

#ifdef NO_DEBUG_LOG
#define DbgLog(DbgLevel, x)
#else
static int modDbgLevel = LVL_ERR;
static void dbg_printf (const char *format, ...)
{
	char buffer[1024 + 1];
	va_list arg;
	va_start (arg, format);
	vsprintf (buffer, format, arg);
#ifdef WIN32
	OutputDebugStringA(buffer);
#else
	fprintf(stderr, ":%s", buffer);
#endif
}

#define DbgLog(DbgLevel, x) do { if(DbgLevel <= modDbgLevel) { \
									fprintf(stderr,"%s:%s:%d:", __FILE__, __FUNCTION__, __LINE__); \
									dbg_printf x;                                          \
									fprintf(stderr,"\n");                                          \
								}                                                                  \
							} while(0);
#endif

static unsigned char muxBuffer[MUX_BUFF_SIZE] = {0};
static int HDCP = 0;
static unsigned long long inputCtr;

unsigned long psi_crc32_table[256] =
{
  0x00000000, 0x04c11db7, 0x09823b6e, 0x0d4326d9,
  0x130476dc, 0x17c56b6b, 0x1a864db2, 0x1e475005,
  0x2608edb8, 0x22c9f00f, 0x2f8ad6d6, 0x2b4bcb61,
  0x350c9b64, 0x31cd86d3, 0x3c8ea00a, 0x384fbdbd,
  0x4c11db70, 0x48d0c6c7, 0x4593e01e, 0x4152fda9,
  0x5f15adac, 0x5bd4b01b, 0x569796c2, 0x52568b75,
  0x6a1936c8, 0x6ed82b7f, 0x639b0da6, 0x675a1011,
  0x791d4014, 0x7ddc5da3, 0x709f7b7a, 0x745e66cd,
  0x9823b6e0, 0x9ce2ab57, 0x91a18d8e, 0x95609039,
  0x8b27c03c, 0x8fe6dd8b, 0x82a5fb52, 0x8664e6e5,
  0xbe2b5b58, 0xbaea46ef, 0xb7a96036, 0xb3687d81,
  0xad2f2d84, 0xa9ee3033, 0xa4ad16ea, 0xa06c0b5d,
  0xd4326d90, 0xd0f37027, 0xddb056fe, 0xd9714b49,
  0xc7361b4c, 0xc3f706fb, 0xceb42022, 0xca753d95,
  0xf23a8028, 0xf6fb9d9f, 0xfbb8bb46, 0xff79a6f1,
  0xe13ef6f4, 0xe5ffeb43, 0xe8bccd9a, 0xec7dd02d,
  0x34867077, 0x30476dc0, 0x3d044b19, 0x39c556ae,
  0x278206ab, 0x23431b1c, 0x2e003dc5, 0x2ac12072,
  0x128e9dcf, 0x164f8078, 0x1b0ca6a1, 0x1fcdbb16,
  0x018aeb13, 0x054bf6a4, 0x0808d07d, 0x0cc9cdca,
  0x7897ab07, 0x7c56b6b0, 0x71159069, 0x75d48dde,
  0x6b93dddb, 0x6f52c06c, 0x6211e6b5, 0x66d0fb02,
  0x5e9f46bf, 0x5a5e5b08, 0x571d7dd1, 0x53dc6066,
  0x4d9b3063, 0x495a2dd4, 0x44190b0d, 0x40d816ba,
  0xaca5c697, 0xa864db20, 0xa527fdf9, 0xa1e6e04e,
  0xbfa1b04b, 0xbb60adfc, 0xb6238b25, 0xb2e29692,
  0x8aad2b2f, 0x8e6c3698, 0x832f1041, 0x87ee0df6,
  0x99a95df3, 0x9d684044, 0x902b669d, 0x94ea7b2a,
  0xe0b41de7, 0xe4750050, 0xe9362689, 0xedf73b3e,
  0xf3b06b3b, 0xf771768c, 0xfa325055, 0xfef34de2,
  0xc6bcf05f, 0xc27dede8, 0xcf3ecb31, 0xcbffd686,
  0xd5b88683, 0xd1799b34, 0xdc3abded, 0xd8fba05a,
  0x690ce0ee, 0x6dcdfd59, 0x608edb80, 0x644fc637,
  0x7a089632, 0x7ec98b85, 0x738aad5c, 0x774bb0eb,
  0x4f040d56, 0x4bc510e1, 0x46863638, 0x42472b8f,
  0x5c007b8a, 0x58c1663d, 0x558240e4, 0x51435d53,
  0x251d3b9e, 0x21dc2629, 0x2c9f00f0, 0x285e1d47,
  0x36194d42, 0x32d850f5, 0x3f9b762c, 0x3b5a6b9b,
  0x0315d626, 0x07d4cb91, 0x0a97ed48, 0x0e56f0ff,
  0x1011a0fa, 0x14d0bd4d, 0x19939b94, 0x1d528623,
  0xf12f560e, 0xf5ee4bb9, 0xf8ad6d60, 0xfc6c70d7,
  0xe22b20d2, 0xe6ea3d65, 0xeba91bbc, 0xef68060b,
  0xd727bbb6, 0xd3e6a601, 0xdea580d8, 0xda649d6f,
  0xc423cd6a, 0xc0e2d0dd, 0xcda1f604, 0xc960ebb3,
  0xbd3e8d7e, 0xb9ff90c9, 0xb4bcb610, 0xb07daba7,
  0xae3afba2, 0xaafbe615, 0xa7b8c0cc, 0xa379dd7b,
  0x9b3660c6, 0x9ff77d71, 0x92b45ba8, 0x9675461f,
  0x8832161a, 0x8cf30bad, 0x81b02d74, 0x857130c3,
  0x5d8a9099, 0x594b8d2e, 0x5408abf7, 0x50c9b640,
  0x4e8ee645, 0x4a4ffbf2, 0x470cdd2b, 0x43cdc09c,
  0x7b827d21, 0x7f436096, 0x7200464f, 0x76c15bf8,
  0x68860bfd, 0x6c47164a, 0x61043093, 0x65c52d24,
  0x119b4be9, 0x155a565e, 0x18197087, 0x1cd86d30,
  0x029f3d35, 0x065e2082, 0x0b1d065b, 0x0fdc1bec,
  0x3793a651, 0x3352bbe6, 0x3e119d3f, 0x3ad08088,
  0x2497d08d, 0x2056cd3a, 0x2d15ebe3, 0x29d4f654,
  0xc5a92679, 0xc1683bce, 0xcc2b1d17, 0xc8ea00a0,
  0xd6ad50a5, 0xd26c4d12, 0xdf2f6bcb, 0xdbee767c,
  0xe3a1cbc1, 0xe760d676, 0xea23f0af, 0xeee2ed18,
  0xf0a5bd1d, 0xf464a0aa, 0xf9278673, 0xfde69bc4,
  0x89b8fd09, 0x8d79e0be, 0x803ac667, 0x84fbdbd0,
  0x9abc8bd5, 0x9e7d9662, 0x933eb0bb, 0x97ffad0c,
  0xafb010b1, 0xab710d06, 0xa6322bdf, 0xa2f33668,
  0xbcb4666d, 0xb8757bda, 0xb5365d03, 0xb1f740b4
};

CCRC32::CCRC32(void)
{
	this->ulTable = psi_crc32_table;
}



CCRC32::~CCRC32(void)
{
	//No destructor code.
}

int CCRC32::Validate(const unsigned char *pData, unsigned long ulDataLength)
{
  //if(p_section->b_syntax_indicator) {
	const unsigned char *pEnd = pData + ulDataLength;
	if(1) {
		/* Check the CRC_32 if b_syntax_indicator is 0 */
		unsigned long ulCrc = 0xffffffff;

		while(pData < pEnd)  {
			ulCrc = (ulCrc << 8) ^ ulTable[(ulCrc >> 24) ^ (*pData)];
			pData++;
		}

		if(ulCrc == 0)  {
			return 1;
		} else   {
			//DbgOut(DBG_LVL_ERR("misc PSI", "Bad CRC_32 (0x%08x) !!!", ulCrc));
			return 0;
			}
	}  else  {
		/* No check to do if b_syntax_indicator is 0 */
		return 1;
	}
}


unsigned long CCRC32::Generate(const unsigned char *pData, unsigned long ulDataLength)
{
    /* CRC_32 */
    unsigned long ulCrc = 0xffffffff;
	const unsigned char *pEnd = pData + ulDataLength;
    while(pData < pEnd)  {
      ulCrc =   (ulCrc << 8) ^ ulTable[(ulCrc >> 24) ^ (*pData)];
      pData++;
    }
	return ulCrc;
}

inline unsigned long CBitstream::GetBsOffset()
{
    return((unsigned long)(m_pbs - m_pbsBase) * 8 + m_bitOffset);
}

void  CBitstream::Init(char *pBs)
{
	m_pbsBase = m_pbs = pBs;
	m_bitOffset = 0;
}

unsigned long  CBitstream::Stop()
{
    return (GetBsOffset()+7)>>3;
}

void CBitstream::PutBit(unsigned long code)
{
    if (code & 1)
        m_pbs[0] = (unsigned char)(m_pbs[0] | (unsigned char)(0x01 << (7 - m_bitOffset)));
    //else
    //    m_pbs[0] = (unsigned char)(m_pbs[0] & (unsigned char)(0xff << (8 - m_bitOffset)));
    m_bitOffset ++;
    if (m_bitOffset == 8) {
        m_pbs ++;
        m_pbs[0] = 0;
        m_bitOffset = 0;
    }
} // CBitstream::PutBit()

void CBitstream::PutBits(unsigned long code, unsigned long length)
{
	code <<= (32 - length);

	// shift field so that the given code begins at the current bit
	// offset in the most significant byte of the 32-bit word
	length += m_bitOffset;
	code >>= m_bitOffset;

	// write bytes back into memory, big-endian
	m_pbs[0] = (unsigned char) ((code >> 24) | m_pbs[0]);
	m_pbs[1] = (unsigned char) (code >> 16);

	m_pbs[2] = 0;
	if (length > 16) {
		m_pbs[2] = (unsigned char) (code >> 8);
		m_pbs[3] = (unsigned char) code;
	}

	// update bitstream pointer and bit offset
	m_pbs += (length >> 3);
	m_bitOffset = (length & 7);
} // CBitstream::PutBits()


CTsMux::CTsMux() 
{

}

void CTsMux::SetPacketsPackNumber(unsigned long packNum)
{
    m_TSPackNum = packNum;
}

void CTsMux::SetPCRPTSDelay(unsigned long delay)
{

    mPCR_PTS_Delay = delay;
}

int CTsMux::Start() 
{
    GeneratePESHeaderFlags(PES_HEADER_FLAGS_VIDEO, VIDEO_STREAM_ID);
    GeneratePESHeaderFlags(PES_HEADER_FLAGS_AUDIO, AUDIO_STREAM_ID);
    GenerateTSHeader(TS_HEADER_ADDRESS_UNIT_START_PAT, 1, PROGRAM_ASSOCIATION_TABLE_PID, 0); 
    GenerateTSHeader(TS_HEADER_ADDRESS_UNIT_START_PMT, 1, PROGRAM_MAP_TABLE_PID, 0); 
    GenerateTSHeader(TS_HEADER_ADDRESS_UNIT_START_VIDEO, 1, VIDEO_ELEMENTARY_PID, 1);
    GenerateTSHeader(TS_HEADER_ADDRESS_UNIT_CONTINUE_VIDEO, 0, VIDEO_ELEMENTARY_PID,1);
    GenerateTSHeader(TS_HEADER_ADDRESS_UNIT_START_AUDIO, 1, AUDIO_ELEMENTARY_PID, 1);
    GenerateTSHeader(TS_HEADER_ADDRESS_UNIT_CONTINUE_AUDIO, 0, AUDIO_ELEMENTARY_PID,1);
    GenerateTSHeader(TS_HEADER_ADDRESS_UNIT_START_PCM, 1, PCM_ELEMENTARY_PID, 1);
    GenerateTSHeader(TS_HEADER_ADDRESS_UNIT_CONTINUE_PCM, 0, PCM_ELEMENTARY_PID,1);
    GenerateTSHeader(TS_HEADER_ADDRESS_UNIT_PTS, 0, SYSTEM_PCR_PID, 0);
    GeneratePAT(PAT_TABLE_ADDRESS);
    GeneratePMT(PMT_TABLE_ADDRESS);
    
    for(int i=0; i<MAX_NUM_TS_STREAMS; i++) {
        
        mContinuity_counter_pes[i][VIDEO_TS_PACKET] = 0;
        mContinuity_counter_pes[i][AUDIO_TS_PACKET] = 0;
        mContinuity_counter_pat[i] = 0;
        mContinuity_counter_pmt[i] = 0;
        mContinuity_counter_pcr[i] = 0;
        mPSISent[i] = 0;
        mPCR[i] = 0;
        /*
        mBytesCount[i] = 0;
        mOnePacket[i] = 0;
        */
        mPCRStart[i] = 0;
        mPCRInterval[i] = 0;
        mPrevPCR[i] = 0;
        m_PTS[i] = 0;
        m_PTS[i] = 0;
    }
    m_Init = true;
    mPCR_PTS_Delay = 200 * 90000 / 1000;
    
    return 1;
}


// Generate PES header and stored in local memory 

void CTsMux::GeneratePESHeaderFlags(unsigned char *localAddr, int stream_id)
{   
    unsigned char *localMem = (unsigned char *)localAddr;

    int two_bits = 2;
    int PES_scrambling_control = 0; 
    int PES_priority = 0;
    int data_alignment_indicator = 1; 
    int copyright = 0;
    int original_or_copy = 0; // not defined protected by copyright or not
    int PTS_DTS_flags = (stream_id == VIDEO_STREAM_ID) ? 3 : 2; // PTS and DTS for video stream; PTS only for audio stream
    int ESCR_flag = 0;      // no ESCR fields
    int ES_rate_flag = 0;  // no ES_rate field
    int DSM_trick_mode_flag = 0; // no trick mode field
    int additional_copy_info_flag = 0; // no additional_copy_info field 
    int PES_CRC_flag = 0;       // no CRC field
    int PES_extension_flag = HDCP; // extension field present

    m_BS.Init((char *)localMem);
    
    m_BS.PutBits(two_bits, 2);
    m_BS.PutBits(PES_scrambling_control, 2);
    m_BS.PutBits(PES_priority, 1);
    m_BS.PutBits(data_alignment_indicator, 1);
    m_BS.PutBits(copyright, 1);
    m_BS.PutBits(original_or_copy, 1);
    m_BS.PutBits(PTS_DTS_flags, 2);
    m_BS.PutBits(ESCR_flag, 1);
    m_BS.PutBits(ES_rate_flag, 1);
    m_BS.PutBits(DSM_trick_mode_flag, 1);
    m_BS.PutBits(additional_copy_info_flag, 1);
    m_BS.PutBits(PES_CRC_flag, 1);
    m_BS.PutBits(PES_extension_flag, 1);

    //Add the data length field
    
    m_PESHeaderFlagsLen = m_BS.Stop();
}


// Generate TS header and stored in local memory, note: it doesn't contain
// the last byte as the continuity_counter keep changing

void CTsMux::GenerateTSHeader(unsigned char * localAddr, bool unitStart, int pid, int priority)
{
    unsigned char * localMem = (unsigned char *)localAddr;

    int sync_byte = 0x47;
    int transport_error_indicator = 0;
    int payload_unit_start_indicator = unitStart;
    int transport_priority = priority;
    int Pid = pid; 
    //int transport_scrambling_control = 0;
    //int adaptation_field_control = 0x01; // no adaptation_field, payload only 

    m_BS.Init((char *)localMem);

    m_BS.PutBits(sync_byte, 8);
    m_BS.PutBits(transport_error_indicator, 1);
    m_BS.PutBits(payload_unit_start_indicator, 1);
    m_BS.PutBits(transport_priority, 1);
    m_BS.PutBits(Pid, 13);
    //m_BS.PutBits(transport_scrambling_control, 2);
    //m_BS.PutBits(adaptation_field_control, 2);

    m_TSHeaderLength = m_BS.Stop();
}
    

// Generate Program association section stored in local memory,


void CTsMux::GeneratePAT(unsigned char * localAddr)
{
	unsigned char * localMem = (unsigned char *)localAddr;
	int table_id = 0x00;
	int section_syntax_indicator = 1;
	//int reserved = 0;
	int section_length = 13; // section length starts from section_length field including CRC
	int transport_stream_id = TRANSPORT_STREAM_ID;
	int version_number = 0;
	int current_next_indicator = 1;
	int section_number = 0;
	int last_section_number = 0;
	int program_number = PROGRAM_NUMBER;
	int program_map_PID = PROGRAM_MAP_TABLE_PID;
	unsigned long CRC = 0;

	m_BS.Init((char *)localMem);
	m_BS.PutBits(table_id, 8);
	m_BS.PutBits(section_syntax_indicator, 1);
	m_BS.PutBits(0, 1);
	m_BS.PutBits(0x3, 2);
	m_BS.PutBits(section_length, 12);
	m_BS.PutBits(transport_stream_id, 16);
	m_BS.PutBits(0x3, 2);
	m_BS.PutBits(version_number, 5);
	m_BS.PutBits(current_next_indicator, 1);
	m_BS.PutBits(section_number, 8);
	m_BS.PutBits(last_section_number, 8);
	m_BS.PutBits(program_number, 16);
	m_BS.PutBits(0x7, 3);
	m_BS.PutBits(program_map_PID, 13);
	mPATLength = m_BS.Stop();

	CRC = m_Crc.Generate((unsigned char *)localMem, mPATLength); 
	m_BS.PutBits(CRC, 32);
	mPATLength += 4;
}


void CTsMux::GeneratePMT(unsigned char * localAddr)
{   
	unsigned char * localMem = (unsigned char *)localAddr;

	int table_id = 0x02;
	int section_syntax_indicator = 1;
	int section_length = 0; // to be determined
	int program_number = PROGRAM_NUMBER;
	int version_number = 0;
	int current_next_indicator = 1;
	int section_number = 0;
	int last_section_number = 0;
	int PCR_PID = SYSTEM_PCR_PID;
	int program_info_length = 6 + (HDCP ? 7 : 0);
	//Note I don't think any tools really look at any descriptors
	int descriptor_tag = 0x88;
	int descriptor_length = 4;
	int profile_idc = 100;
	int constraint_set0_flag = 0;
	int constraint_set1_flag = 0;
	int constraint_set2_flag = 0;
	int AVC_compatible_flags = 0;
	int level_idc = 40;
	int AVC_still_present = 0;
	int AVC_24_hour_picture_flag = 1;

	int hdcp_descriptor_tag = 0x5;
	int hdcp_descriptor_length = 0x5;
	int hdcp_format_identifier = 'HDCP';
	int hdcp_version = 0x20;

	int mpeg4_audio_descriptor_tag = 28;
	int mpeg4_descriptor_length = 1;
	int mpeg4_audio_profile_and_level = 0x38;

	int stream_type_video = 0x1b; // AVC video stream 
	int elementary_PID_video = VIDEO_ELEMENTARY_PID;
	int stream_type_audio = 0xF; // mpeg4 AAC audio with ADTS transport syntax
	int elementary_PID_audio = AUDIO_ELEMENTARY_PID;
	int stream_type_private = 0x6; // PES packets containing private data
	int elementary_PID_PCM = PCM_ELEMENTARY_PID; 
	int ES_info_length = 0;
	unsigned long CRC = 0;


	m_BS.Init((char *)localMem);
	m_BS.PutBits(table_id, 8);
	m_BS.PutBits(section_syntax_indicator, 1);
	m_BS.PutBits(0, 1);
	m_BS.PutBits(0x3, 2);
	m_BS.PutBits(section_length, 12);
	m_BS.PutBits(program_number, 16);
	m_BS.PutBits(0x3, 2);
	m_BS.PutBits(version_number, 5);
	m_BS.PutBits(current_next_indicator, 1);
	m_BS.PutBits(section_number, 8);
	m_BS.PutBits(last_section_number, 8);
	m_BS.PutBits(0x7, 3);

	m_BS.PutBits(PCR_PID, 13);
	m_BS.PutBits(0xf, 4);
	m_BS.PutBits(program_info_length, 12);
	m_BS.PutBits(descriptor_tag, 8);
	m_BS.PutBits(descriptor_length, 8);
	m_BS.PutBits(profile_idc, 8);
	m_BS.PutBits(constraint_set0_flag, 1);
	m_BS.PutBits(constraint_set1_flag, 1);
	m_BS.PutBits(constraint_set2_flag, 1);
	m_BS.PutBits(AVC_compatible_flags, 5);
	m_BS.PutBits(level_idc, 8);
	m_BS.PutBits(AVC_still_present, 1);
	m_BS.PutBits(AVC_24_hour_picture_flag,1);
	m_BS.PutBits(0x3f, 6);


	m_BS.PutBits(stream_type_video, 8);
	m_BS.PutBits(0x7, 3);
	m_BS.PutBits(elementary_PID_video, 13);
	m_BS.PutBits(0xf, 4);
	m_BS.PutBits(ES_info_length, 12);

#ifdef EN_AUDIO
	m_BS.PutBits(stream_type_audio, 8);
	m_BS.PutBits(0x7, 3);
	m_BS.PutBits(elementary_PID_audio, 13);
	m_BS.PutBits(0xf, 4);

	//ES_info_length = 3;
	ES_info_length = 0;
	m_BS.PutBits(ES_info_length, 12);
#endif

	m_BS.PutBits(stream_type_private, 8);
	m_BS.PutBits(0x7, 3);
	m_BS.PutBits(elementary_PID_PCM, 13);
	m_BS.PutBits(0xf, 4);
	m_BS.PutBits(ES_info_length, 12);
	mPMTLength = m_BS.Stop();

	/* fill in section length bits */
	localMem[1] |= (mPMTLength+1) >> 8;
	localMem[2] |= (mPMTLength+1) & 0xff;

	CRC = m_Crc.Generate((unsigned char *)localMem, mPMTLength); 
	m_BS.PutBits(CRC, 32);
	mPMTLength += 4;
}

//write pointer_field
int CTsMux::psi_packet(char * localPsiAddr, char * localTSHeaderAddr, char *bitBuffAddr, unsigned long length,   unsigned char counter) 
{

    int  pointer_field = 0;
    int  packet_size = 0;
    int  padding = 0;

    for(int i=0; i<m_TSHeaderLength; i++ ) {
        *bitBuffAddr++ = localTSHeaderAddr[i];
    }

    packet_size += m_TSHeaderLength;

    m_BS.Init(bitBuffAddr);

    
    // write the last byte of TS header to bit buffer
    unsigned char continuity_counter = counter;
    int transport_scrambling_control = 0;
    int adaptation_field_control = 0x01; // no adaptation_field, payload only

    m_BS.PutBits(transport_scrambling_control, 2);
    m_BS.PutBits(adaptation_field_control, 2);
    m_BS.PutBits(continuity_counter, 4);


    int bytes = m_BS.Stop();

    packet_size += bytes;
    bitBuffAddr += bytes;

    *bitBuffAddr++ = pointer_field;
            
    // Write PAT table immediated following pointer_field
    for(int i=0; i<length; i++ ) {
        *bitBuffAddr++ = localPsiAddr[i];
    }

    packet_size += length + 1;

    int psi_payload_size = TS_PACKET_SIZE - (packet_size - padding);

    //fill the rest of ts payloads with 0xff
    for(int i=0; i<psi_payload_size; i++) {

        *bitBuffAddr ++ = 0xff;
        packet_size ++;
    }

    return packet_size;
}



int CTsMux::pcr_packet(unsigned long long pts, char *bitBuffAddr, unsigned char counter) 
{
    int  packet_size = 0;
    int  padding = 0;
    char* localTSHeaderAddr = (char *)TS_HEADER_ADDRESS_UNIT_PTS;
         
    for(int i=0; i<m_TSHeaderLength; i++ ) {
        *bitBuffAddr++ = localTSHeaderAddr[i];
    }

    packet_size += m_TSHeaderLength;

    m_BS.Init(bitBuffAddr);

    // write the last byte of TS header to bit buffer
    int continuity_counter = counter;
    int transport_scrambling_control = 0;
    int adaptation_field_control = 2; // adaptation_field only, no payload
    
    m_BS.PutBits(transport_scrambling_control, 2);
    m_BS.PutBits(adaptation_field_control, 2);
    m_BS.PutBits(continuity_counter, 4);

    // adaption field with PTS
    int adaptation_field_length = 183;
    int discontinuity_indicator = 0;
    int random_access_indicator = 0;
    int elementary_stream_priority_indicator = 0;
    int PCR_flag = 1;
    int OPCR_flag = 0;
    int splicing_point_flag = 0;
    int transport_private_data_flag = 0;
    int adaptation_field_extension_flag = 0;

    unsigned long long program_clock_reference_base = pts; //& (unsigned long long)0x1ffffffff;
    unsigned long program_clock_reference_base_low = pts & 0xffffffff;
    unsigned long program_clock_reference_base_high = pts >> 32;
    int reserved = 0x3f;
    int program_clock_reference_extension = 0;

    
    m_BS.PutBits(adaptation_field_length, 8);
    m_BS.PutBits(discontinuity_indicator, 1);
    m_BS.PutBits(random_access_indicator, 1);
    m_BS.PutBits(elementary_stream_priority_indicator, 1);
    m_BS.PutBits(PCR_flag, 1);
    m_BS.PutBits(OPCR_flag, 1);
    m_BS.PutBits(splicing_point_flag, 1);
    m_BS.PutBits(transport_private_data_flag, 1);
    m_BS.PutBits(adaptation_field_extension_flag, 1);

    if(PCR_flag == 1) {
        m_BS.PutBits(program_clock_reference_base_high, 1);
        m_BS.PutBits(program_clock_reference_base_low, 32);
        m_BS.PutBits(reserved, 6);
        m_BS.PutBits(program_clock_reference_extension, 9);
    }


    int bytes = m_BS.Stop();

    packet_size += bytes;
    bitBuffAddr += bytes;

    
    int stuff_size = TS_PACKET_SIZE - (packet_size - padding);

    //fill the rest of ts payloads with 0xff
    for(int i=0; i<stuff_size; i++) {

        *bitBuffAddr ++ = 0xff;
        packet_size ++;

    }
    return packet_size;
}


int CTsMux::AddAPesHeader(CAccessUnit *pAu, char *pBuffer, int nPlLen)
{

	// m_PESHeaderFlagsLen only to PES_extension_flag field, add PES_heaer_data_length add PTS_DTS fields
	int bytes = 0, pad = 0;
	char *bitBufPtr = pBuffer;
	int AUBytes = nPlLen;
    unsigned long long   pts = pAu->m_PTS;
    unsigned long long   dts = pAu->m_DTS;

	int pes_header_total_length = 6 + m_PESHeaderFlagsLen + 1 + 5;
	char* lmemPESAddr = (char *)PES_HEADER_FLAGS_AUDIO;
	int packet_start_code_prefix = 0x000001;
	int stream_id = AUDIO_STREAM_ID;
	int PES_packet_length = 0;

    m_BS.Init(bitBufPtr);
    m_BS.PutBits(packet_start_code_prefix, 24);
    m_BS.PutBits(stream_id, 8);
    m_BS.PutBits(PES_packet_length, 16);
	bytes = m_BS.Stop();
	unsigned char * pes_packet_length_addr = (unsigned char *)(bitBufPtr + 4);

    bitBufPtr += bytes; 
                
    // copy PES header from local memory to bit buffer
    for(int i=0; i<m_PESHeaderFlagsLen; i++) {
        *bitBufPtr++ = lmemPESAddr[i];
    }

	bytes += m_PESHeaderFlagsLen;
	//pes header total length needs to be word aligned, add pad
	pad = (4 - pes_header_total_length & 0x3) & 0x3; 
	//PES_header_data_length only include optional fields + pads
	int PES_header_data_length = pad + 5;
	PES_packet_length += pad + 8 + AUBytes;

	*pes_packet_length_addr = PES_packet_length >> 8;
	*(pes_packet_length_addr + 1) = PES_packet_length & 0xff;
	int pts_dts_flags = 2; //PTS_DTS_flags=='10'
                
	unsigned short pts_14_0 = pts & 0x7fff;
	pts >>= 15;
	unsigned short pts_29_15 = pts & 0x7fff;
	pts >>= 15;
	unsigned short pts_32_30 = pts & 0x7;

	int marker_bit = 1;
    
    m_BS.Init(bitBufPtr);
    m_BS.PutBits(PES_header_data_length, 8);
    m_BS.PutBits(pts_dts_flags, 4);
    m_BS.PutBits(pts_32_30, 3);
    m_BS.PutBits(marker_bit, 1);
    m_BS.PutBits(pts_29_15, 15);
    m_BS.PutBits(marker_bit, 1);
    m_BS.PutBits(pts_14_0,  15);
    m_BS.PutBits(marker_bit, 1);
	
	int tmpBytes = m_BS.Stop();

    bytes += tmpBytes;
	bitBufPtr += tmpBytes; 
	for(int i=0; i<pad; i++) {
		*bitBufPtr++ = 0xff;
	}
	bytes += pad;

	return bytes;
}
            
int CTsMux::AddVPesHeader(CAccessUnit *pAu, char *pBuffer, int nPlLen)
{
	int bytes = 0, pad = 0;
	char *bitBufPtr = pBuffer;
	int AUBytes = nPlLen;
    unsigned long long   pts = pAu->m_PTS;
    unsigned long long   dts = pAu->m_DTS;
	int nAddBytes = 0;
	char* lmemPESAddr = (char *)PES_HEADER_FLAGS_VIDEO;

	int packet_start_code_prefix = 0x000001;

	int stream_id = VIDEO_STREAM_ID;
	// allowed for video elementary stream contained in TS packets.
	int PES_packet_length = 0;

	/*
	**
	*/
	m_BS.Init(bitBufPtr);
	m_BS.PutBits(packet_start_code_prefix, 24);
	m_BS.PutBits(stream_id, 8);
	m_BS.PutBits(PES_packet_length, 16);
	nAddBytes = m_BS.Stop();
	bytes += nAddBytes;
	bitBufPtr += nAddBytes; 
    
	// copy PES header from local memory to bit buffer
	for(int i=0; i<m_PESHeaderFlagsLen; i++) {
		*bitBufPtr++ = lmemPESAddr[i];
	}
    bytes += m_PESHeaderFlagsLen;

	// m_PESHeaderFlagsLen only to PES_extension_flag field, add PES_heaer_data_length add PTS_DTS fields
	int pes_header_total_length = 6 + m_PESHeaderFlagsLen + 1 + 10;
                
	//pes header total length needs to be word aligned, add pad
	pad = (4 - pes_header_total_length & 0x3) & 0x3; 
	//PES_header_data_length only include optional fields + pads
	int PES_header_data_length = pad + 10;
	if(HDCP) PES_header_data_length += 1 + 16;

	int pts_dts_flags = 3; //PTS_DTS_flags=='11'
                
	unsigned short pts_14_0 = pts & 0x7fff;
	pts >>= 15;
	unsigned short pts_29_15 = pts & 0x7fff;
	pts >>= 15;
	unsigned short pts_32_30 = pts & 0x7;
    
	unsigned short dts_14_0 = dts & 0x7fff;
	dts >>= 15;
	unsigned short dts_29_15 = dts & 0x7fff;
	dts >>= 15;
	unsigned short dts_32_30 = dts & 0x7;
	int marker_bit = 1;
    
	m_BS.Init(bitBufPtr);
	m_BS.PutBits(PES_header_data_length, 8);
	m_BS.PutBits(pts_dts_flags, 4);
	m_BS.PutBits(pts_32_30, 3);
	m_BS.PutBits(marker_bit, 1);
	m_BS.PutBits(pts_29_15, 15);
	m_BS.PutBits(marker_bit, 1);
	m_BS.PutBits(pts_14_0,  15);
	m_BS.PutBits(marker_bit, 1);
	m_BS.PutBits(1,          4);
	m_BS.PutBits(dts_32_30, 3);
	m_BS.PutBits(marker_bit, 1);
	m_BS.PutBits(dts_29_15, 15);
	m_BS.PutBits(marker_bit, 1);
	m_BS.PutBits(dts_14_0,  15);
	m_BS.PutBits(marker_bit, 1);
	nAddBytes = m_BS.Stop();
	bytes += nAddBytes;
	bitBufPtr += nAddBytes; 

	for(int i=0; i<pad; i++) {
		*bitBufPtr++ = 0xff;
	}
	bytes += pad;

	return bytes;
}


int CTsMux::Mux(CAccessUnit *pAu)
{
	unsigned char    streamID = pAu->m_TSStreamID;
	unsigned short   streamType = pAu->m_SampleStreamType;

	bool       IsVideoStream = (streamType == SAMPLE_TYPE_H264);
	bool       IsAudioStream = (streamType == SAMPLE_TYPE_AAC);
	bool       pcrMaster = pAu->m_TSPCRMaster;
	bool       IsAudioOnly = pAu->m_TSAudioOnly;
    int        nAddBytes = 0;
	unsigned long long   pcrDelta = 0;
	unsigned long long   pts = 0;
	unsigned long long   dts = 0;
	unsigned long long   pcr = 0;
	char *     lmemTSAddr;

	if (streamID > MAX_NUM_TS_STREAMS) {
		while(streamID > MAX_NUM_TS_STREAMS) {
			streamID = streamID - MAX_NUM_TS_STREAMS;
		}
	}

	switch(streamType) {
		case SAMPLE_TYPE_H264:
			DbgLog(LVL_TRACE, ("SAMPLE_TYPE_H264"));
			lmemTSAddr =  (char *)TS_HEADER_ADDRESS_UNIT_START_VIDEO;
			break;
		case SAMPLE_TYPE_AAC:
			DbgLog(LVL_TRACE, ("SAMPLE_TYPE_AAC"));
			lmemTSAddr =  (char *)TS_HEADER_ADDRESS_UNIT_START_AUDIO;
			break;
	}
    

	char* bitBufPtr = pAu->m_pTsData;
	char* payloadStartPtr = pAu->m_pRawData;

	int AUBytes = pAu->m_RawSize;
	pAu->m_TsSize = pAu->m_RawSize;
	DbgLog(LVL_TRACE, ("AUBytes=%d", AUBytes));
	/*
	 * Update mPCR and mContinuity_counter_pcr
	 */
	if(IsVideoStream && pcrMaster ) {
		pcrDelta = 0;
		dts = pAu->m_DTS;
		pts = pAu->m_PTS;
		pcr = dts > mPCR_PTS_Delay ? dts - mPCR_PTS_Delay : 0;
           
		m_DTS[streamID] = dts;

		if(m_Init == true){
			mPCRStart[streamID] = pcr;
			mPCR[streamID] = mPCRStart[streamID];
		} else {

            if(pcr >= mPCR[streamID]) {
                mPCRInterval[streamID] = pcr - mPrevPCR[streamID];
                mPCR[streamID] = pcr;
            } else {
				// reset 
                mPCRStart[streamID] = pcr;
                mPCR[streamID] = mPCRStart[streamID];
            }
        }

		m_Init = false;

		mPSIInterval[streamID] = m_PTS[streamID] - mPSISent[streamID];

		if(mPCRInterval[streamID] >= PCR_PERIOD || mPCR[streamID] == mPCRStart[streamID]) {
			memset(bitBufPtr, 0x00, TS_PACKET_SIZE);
			nAddBytes= pcr_packet(mPCR[streamID], bitBufPtr, mContinuity_counter_pcr[streamID] );
			pAu->m_TsSize += nAddBytes;
			bitBufPtr += nAddBytes;

			mPrevPCR[streamID] = mPCR[streamID];

			mPCRInterval[streamID] = 0;
			mContinuity_counter_pcr[streamID]++;
			mContinuity_counter_pcr[streamID] &= 0xf;
		} 
	} else if(IsAudioStream && pcrMaster) {
        pcrDelta = 0;
        pts = pAu->m_PTS;

        pcr = pts > mPCR_PTS_Delay ? pts - mPCR_PTS_Delay : 0;

        m_PTS[streamID] = pts;
         
        if(m_Init == true){
            mPCRStart[streamID] = pcr;
            mPCR[streamID] = mPCRStart[streamID];
             
        }else {
    
            if(pcr >= mPCR[streamID]) {
                mPCRInterval[streamID] = pcr - mPrevPCR[streamID];
                mPCR[streamID] = pcr;
                    
            }else // reset 
            {
                mPCRStart[streamID] = pcr;
                mPCR[streamID] = mPCRStart[streamID];
            }
        }
    
		m_Init = false;

		mPSIInterval[streamID] = m_PTS[streamID] - mPSISent[streamID];

		if(mPCRInterval[streamID] >= PCR_PERIOD || mPCR[streamID] == mPCRStart[streamID]) {
			memset(bitBufPtr, 0x00, TS_PACKET_SIZE);
			nAddBytes = pcr_packet(mPCR[streamID], bitBufPtr, mContinuity_counter_pcr[streamID] );
			pAu->m_TsSize += nAddBytes;
			bitBufPtr += nAddBytes;;

			mPCRInterval[streamID] = 0;
			mPrevPCR[streamID] = mPCR[streamID];
			mContinuity_counter_pcr[streamID]++;
			mContinuity_counter_pcr[streamID] &= 0xf;
		}
	}
    
    
     if((IsVideoStream || (IsAudioStream && IsAudioOnly) ) && (mPSISent[streamID] == 0 || mPSIInterval[streamID] > PSI_PERIOD)) {

		char* lmemTSPATAddr = (char *)TS_HEADER_ADDRESS_UNIT_START_PAT;
		char* lmemTSPMTAddr = (char *)TS_HEADER_ADDRESS_UNIT_START_PMT;

		char* lmemPATAddr = (char *)PAT_TABLE_ADDRESS;
		char* lmemPMTAddr = (char *)PMT_TABLE_ADDRESS;
		memset(bitBufPtr, 0x00, TS_PACKET_SIZE);
		nAddBytes = psi_packet(lmemPATAddr, lmemTSPATAddr, bitBufPtr, mPATLength, mContinuity_counter_pat[streamID]);
		bitBufPtr += nAddBytes;
		pAu->m_TsSize += nAddBytes;

		memset(bitBufPtr, 0x00, TS_PACKET_SIZE);
		nAddBytes = psi_packet(lmemPMTAddr, lmemTSPMTAddr, bitBufPtr, mPMTLength, mContinuity_counter_pmt[streamID]);
		bitBufPtr += nAddBytes;
		pAu->m_TsSize += nAddBytes;

		mContinuity_counter_pat[streamID]++;
		mContinuity_counter_pat[streamID] &= 0xf;

		mContinuity_counter_pmt[streamID]++;
		mContinuity_counter_pmt[streamID] &= 0xf;


		mPSISent[streamID] = IsVideoStream ? m_PTS[streamID] : m_PTS[streamID];
		mPSIInterval[streamID] = 0;
	}


    bool newPES_Video = true;
    bool newPES_Audio = true;

    char* usbHeader = 0;
    int   bytes = 0;
   
    while(AUBytes > 0) {
       
   		/*
		 *  TS header
		 */
        for(int i=0; i<m_TSHeaderLength; i++ ) {
            *bitBufPtr++ = lmemTSAddr[i];
        }
        pAu->m_TsSize += m_TSHeaderLength;
		DbgLog(LVL_TRACE, ("m_TSHeaderLength=%d", m_TSHeaderLength));
        int pesHeaderLength = (IsVideoStream) ? (newPES_Video == true ? (VIDEO_PES_HEADER_LENGTH + 3) & ~0x3 : 0) : (newPES_Audio == true ? (AUDIO_PES_HEADER_LENGTH + 3) & ~0x3 : 0);
		int adapMinLength = 0;
        int padNum;
        int padNumSource = (4 - (int)payloadStartPtr & 0x3) & 0x3;

        if(AUBytes >= 184 - pesHeaderLength - adapMinLength) {                
            padNum = (4 - (int)(bitBufPtr + 1 + adapMinLength) & 0x3) & 0x3;

			if(padNum + padNumSource - 4 == 0) {
                padNum = 0;
			} else {
                padNumSource = 0;
			}
		}  else {
            padNum = 184 - pesHeaderLength - adapMinLength - AUBytes ;
		}

		DbgLog(LVL_TRACE, ("padNum=%d", padNum));
		/*
		 *
		 */
		memset(bitBufPtr, 0x00, 2);
		m_BS.Init(bitBufPtr);
    
		int transport_scrambling_control = 0;
		int adaptation_field_control = padNum + adapMinLength > 0 ? 0x3 : 0x1; //adaptation followed by payload : no adaptation_field, payload only

		m_BS.PutBits(transport_scrambling_control, 2);
		m_BS.PutBits(adaptation_field_control, 2);
		int counter = (IsVideoStream) ? mContinuity_counter_pes[streamID][VIDEO_TS_PACKET]:
									mContinuity_counter_pes[streamID][AUDIO_TS_PACKET];          
		m_BS.PutBits(counter, 4);
		bytes = m_BS.Stop();

		bitBufPtr += bytes;
		pAu->m_TsSize += bytes;

		int ES_payload_size = TS_PACKET_SIZE - (m_TSHeaderLength + 1) ;

		/*
		 * Padding
		 */
		int adaptation_total_length = 0;
		if(adapMinLength == 0) {
			for(int i=0; i<padNum ; i++) {
                if(i==0)
                    *bitBufPtr++ = padNum-1;
                else if(i==1)
                    *bitBufPtr++ = 0; // all TS adaptation flags are zero
                else
                    *bitBufPtr++ = 0xff;
            }
			adaptation_total_length = padNum;
		} else {
			// TODO
			// Add adaptation
		}
        
        pAu->m_TsSize += adaptation_total_length ;
        ES_payload_size -= adaptation_total_length;

		/*
		 * Pes Header
		 */
		if((IsVideoStream && newPES_Video) || (IsAudioStream && newPES_Audio)){
			if( IsVideoStream && newPES_Video){
				memset(bitBufPtr, 0x00, TS_PACKET_SIZE);
				bytes = AddVPesHeader(pAu, bitBufPtr, AUBytes);
				newPES_Video = false;
				lmemTSAddr =  (char *)TS_HEADER_ADDRESS_UNIT_CONTINUE_VIDEO;
			} else if (IsAudioStream && newPES_Audio){
				memset(bitBufPtr, 0x00, TS_PACKET_SIZE);
				bytes = AddAPesHeader(pAu, bitBufPtr, AUBytes);
				newPES_Audio = false;
				lmemTSAddr =  (char *)TS_HEADER_ADDRESS_UNIT_CONTINUE_AUDIO;
			}
			bitBufPtr += bytes; 
			pAu->m_TsSize += bytes;
			ES_payload_size -=  bytes;
		}

		/*
		 * Payload 4 byte alignment padding
		 */
		if(((int)payloadStartPtr & 0x3 )|| ((int)bitBufPtr & 0x3)) {
			for(int i=0; i<padNumSource; i++) {
				*bitBufPtr++ = *payloadStartPtr++;
			}
			ES_payload_size -= padNumSource;
			AUBytes -= padNumSource;
		}

		/*
		 * Payload 
		 */
		for(int i=0; i<ES_payload_size; i++) {
			*bitBufPtr++ = *payloadStartPtr++;
		}
		AUBytes = AUBytes - ES_payload_size;
		DbgLog(LVL_TRACE, ("ES_payload_size=%d", ES_payload_size));
		if(IsVideoStream) {
			mContinuity_counter_pes[streamID][VIDEO_TS_PACKET] ++;
			mContinuity_counter_pes[streamID][VIDEO_TS_PACKET] &= 0xf;
		} else {
			mContinuity_counter_pes[streamID][AUDIO_TS_PACKET] ++;
			mContinuity_counter_pes[streamID][AUDIO_TS_PACKET] &= 0xf;
		}
	}
    return 0;
}
