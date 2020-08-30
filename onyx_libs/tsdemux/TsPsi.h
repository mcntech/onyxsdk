#ifndef _PSI_PARSER_H_
#define _PSI_PARSER_H_
#include <stdio.h>
#include <stdlib.h>
#include <memory.h>
#include "TsPsiIf.h"

#ifndef BYTE
#define BYTE unsigned char 
#endif

#ifndef WORD
#define WORD unsigned short 
#endif

#ifndef UNALIGNED
#define UNALIGNED
#endif

//  i 0-based
#if 0
#define BYTE_OFFSET(pb,i)           (& BYTE_VALUE((pb),i))
#define BYTE_VALUE(pb,i)            (((BYTE *) (pb))[i])
#define WORD_VALUE(pb,i)            (* (UNALIGNED WORD *) BYTE_OFFSET((pb),i))
#define DWORD_VALUE(pb,i)           (* (UNALIGNED DWORD *) BYTE_OFFSET((pb),i))
#define Ulong long_VALUE(pb,i)       (* (UNALIGNED Ulong long *) BYTE_OFFSET((pb),i))
#else
#define BYTE_VALUE(pb,i)            (pb[i])
#define WORD_VALUE(pb,i)            ((((unsigned short)pb[i] << 8) & 0xFF00) \
									| ((unsigned short)pb[i+1] & 0x00FF))

#define DWORD_VALUE(pb,i)           ((((unsigned long)pb[i] << 24)    & 0xFF000000)  \
									| (((unsigned long)pb[i+1] << 16) & 0x00FF0000) \
									| (((unsigned long)pb[i+2] << 8)  & 0x0000FF00)  \
									| ((unsigned long)pb[i+3]         & 0x000000FF))

#define Ulong long_VALUE(pb,i)      ((UNALIGNED Ulong long)DWORD_VALUE(pb,i+4) << 32 | (UNALIGNED Ulong long)DWORD_VALUE(pb,i))
#endif

#define PID_VALUE(pb)                                   (WORD_VALUE(pb,1) & 0x00001fff)
#define PSI_TABLE_ID_VALUE(pb)                          (BYTE_VALUE((pb),0))
#define PSI_SECTION_SYNTAX_INDICATOR_BIT(pb)            ((BYTE_VALUE((pb),1) & 0x80) >> 7)
#define PSI_SECTION_LENGTH_VALUE(pb)                    (WORD_VALUE(pb,1) & 0x0fff)
#define PSI_VERSION_NUMBER_VALUE(pb)                    ((BYTE_VALUE((pb),5) >> 1) & 0x1f)
#define PSI_CURRENT_NEXT_INDICATOR_BIT(pb)              (BYTE_VALUE((pb),5) & 0x01)
#define PSI_SECTION_NUMBER_VALUE(pb)                    (BYTE_VALUE((pb),6))
#define PSI_LAST_SECTION_NUMBER_VALUE(pb)               (BYTE_VALUE((pb),7))

//  ---------------------------------------------------------------------------
//  Program Association Table (PAT) macros
//  pb points to the first byte of section

#define PAT_TABLE_ID_VALUE(pb)                          PSI_TABLE_ID_VALUE(pb)
#define PAT_SECTION_SYNTAX_INDICATOR_BIT(pb)            PSI_SECTION_SYNTAX_INDICATOR_BIT(pb)
#define PAT_SECTION_LENGTH_VALUE(pb)                    PSI_SECTION_LENGTH_VALUE(pb)
#define PAT_TRANSPORT_STREAM_ID_VALUE(pb)               (WORD_VALUE(pb,3))
#define PAT_VERSION_NUMBER_VALUE(pb)                    PSI_VERSION_NUMBER_VALUE(pb)
#define PAT_CURRENT_NEXT_INDICATOR_BIT(pb)              PSI_CURRENT_NEXT_INDICATOR_BIT(pb)
#define PAT_SECTION_NUMBER_VALUE(pb)                    PSI_SECTION_NUMBER_VALUE(pb)
#define PAT_LAST_SECTION_NUMBER_VALUE(pb)               PSI_LAST_SECTION_NUMBER_VALUE(pb)


//  PAT program descriptor parsing macros

//  pointer to the nth program descriptor in the section; n is 0-based; does
//  NO range checking on the validity of n; offsets past the header (8 bytes)
//  then into the program descriptors
#define PAT_PROGRAM_DESCRIPTOR(pbPAT,n)                 ((((BYTE *) (pbPAT)) + 8) + ((n) * 4))

//  nth program descriptor field extractions
#define PAT_PROGRAM_DESCRIPTOR_PROGRAM_NUMBER_VALUE(pbPAT,n)    (WORD_VALUE(PAT_PROGRAM_DESCRIPTOR(pbPAT,n),0))
#define PAT_PROGRAM_DESCRIPTOR_PID_VALUE(pbPAT,n)               (WORD_VALUE(PAT_PROGRAM_DESCRIPTOR(pbPAT,n),2) & 0x1fff)
#define PAT_PROGRAM_DESCRIPTOR_IS_PROGRAM(pbPAT,n)              (PAT_PROGRAM_DESCRIPTOR_PROGRAM_NUMBER_VALUE(pbPAT,n) != 0x0000)
#define PAT_PROGRAM_DESCRIPTOR_IS_NETWORK(pbPAT,n)              (PAT_PROGRAM_DESCRIPTOR_IS_PROGRAM(pbPAT,n) == false)
#define PAT_PROGRAM_DESCRIPTOR_PROGRAM_PID_VALUE(pbPAT,n)       PAT_PROGRAM_DESCRIPTOR_PID_VALUE(pbPAT,n)
#define PAT_PROGRAM_DESCRIPTOR_NETWORK_PID_VALUE(pbPAT,n)       PAT_PROGRAM_DESCRIPTOR_PID_VALUE(pbPAT,n)

//  ---------------------------------------------------------------------------
//  Program Map Table (PMT) macros

//  pb points to the first byte of section
#define PMT_TABLE_ID_VALUE(pb)                          BYTE_VALUE(pb,0)
#define PMT_SECTION_SYNTAX_INDICATOR_BIT(pb)            PSI_SECTION_SYNTAX_INDICATOR_BIT(pb)
#define PMT_SECTION_LENGTH_VALUE(pb)                    PSI_SECTION_LENGTH_VALUE(pb)
#define PMT_PROGRAM_NUMBER_VALUE(pb)                    WORD_VALUE(pb,3)
#define PMT_VERSION_NUMBER_VALUE(pb)                    PSI_VERSION_NUMBER_VALUE(pb)
#define PMT_CURRENT_NEXT_INDICATOR_BIT(pb)              PSI_CURRENT_NEXT_INDICATOR_BIT(pb)
#define PMT_SECTION_NUMBER(pb)                          PSI_SECTION_NUMBER_VALUE(pb)
#define PMT_LAST_SECTION_NUMBER(pb)                     PSI_LAST_SECTION_NUMBER_VALUE(pb)
#define PMT_PCR_PID_VALUE(pb)                           (WORD_VALUE(pb,8) & 0x1fff)
#define PMT_PROGRAM_INFO_LENGTH_VALUE(pb)               (WORD_VALUE(pb,10) & 0x0fff)

//  pb points to the stream record block (stream_type, etc...)
#define PMT_STREAM_RECORD_STREAM_TYPE_VALUE(pb)         BYTE_VALUE(pb,0)
#define PMT_STREAM_RECORD_ELEMENTARY_PID(pb)            (WORD_VALUE(pb,1) & 0x1fff)
#define PMT_STREAM_RECORD_ES_INFO_LENGTH(pb)            (WORD_VALUE(pb,3) & 0x0fff)

//  ---------------------------------------------------------------------------
//  Private section
//  pb points to the TS packet payload ! (first byte of section)

#define PRIVATE_SECTION_SYNTAX_INDICATOR_BIT(pb)        PSI_SECTION_SYNTAX_INDICATOR_BIT(pb)


//  general MPEG-2 - related macros follow

//  ---------------------------------------------------------------------------
//  general macros

//  true/false if the PSI header is valid; can be used for PAT, PMT, CAT,
//  non pure-private sections; checks the following items:
//      1. section_length should not exceed maximum allowed
//      2. section_length should include at least remaining header
//      3. last_section_number should be >= section_number
//      4. syntax_indicator bit should be '1'
//      5. section_length should have first 2 bits == '00'
#define VALID_PSI_HEADER(pbSection)                                                                 \
            ( (PSI_SECTION_LENGTH_VALUE(pbSection) <= 0x3fd)                                    &&  \
              (PSI_SECTION_LENGTH_VALUE(pbSection) >= 5)                                        &&  \
              (PSI_LAST_SECTION_NUMBER_VALUE(pbSection) >= PSI_SECTION_NUMBER_VALUE(pbSection)) &&  \
              (PSI_SECTION_SYNTAX_INDICATOR_BIT(pbSection) == true)                             &&  \
              (((PSI_SECTION_LENGTH_VALUE(pbSection) & 0xc000)) == 0x0000) )

//  checks that he PAT section given is valid; checks the following items:
//      1. valid PSI header
//      2. even number of 4 byte program descriptors (section_length - 5 bytes
//          remaining in header - 4 byte CRC_32)
//      3. table_id is 0x00 (see table 2-26)
#define VALID_PAT_SECTION(pbPAT)                                            \
            ( VALID_PSI_HEADER(pbPAT)                                   &&  \
              (((PAT_SECTION_LENGTH_VALUE(pbPAT) - 4 - 5) % 4) == 0)    &&  \
              (PAT_TABLE_ID_VALUE(pbPAT) == PAT_TABLE_ID) )

//  returns the number of programs described in the passed PAT section
#define NUMBER_PROGRAMS_IN_PAT_SECTION(pbPAT)   ((DWORD) ((PAT_SECTION_LENGTH_VALUE(pbPAT) - 5 - 4) / 4))

//  checks that he PAT section given is value; checks the following items:
//      1. valid PSI header
//      2. section_length, program_info_length are valid wrt each other; checked as
//          follows: section_length >= program_info_length + 9 + 4, where
//              9 is the number of bytes that FOLLOW section_length and include program_info_length
//              4 is the size of the CRC_32 value at the end of the section
//      3. table_id is PMT_TABLE_ID
#define VALID_PMT_SECTION(pbPMT)                                                                    \
            ( VALID_PSI_HEADER(pbPMT)                                                           &&  \
              (PMT_SECTION_LENGTH_VALUE(pbPMT) >= PMT_PROGRAM_INFO_LENGTH_VALUE(pbPMT) + 9 + 4) &&  \
              (PMT_TABLE_ID_VALUE(pbPMT) == PMT_TABLE_ID) )

//  confirms that the PID value does not have bits higher than the first 13
//  are set
#define VALID_PID(pid)                          (((pid) & 0xffffe000) == 0x00000000)

//  confirms the continuity_counter holds a valid value
#define VALID_CONTINUITY_COUNTER(counter)       (((counter) & 0xffffff00) == 0x00000000)

//  see page 20 of the MPEG-2 systems spec
#define EXPECT_CONTINUITY_COUNTER_INCREMENT(pid, adaptation_field_control)                      \
                                                 ((pid) != TS_NULL_PID &&                       \
                                                  (adaptation_field_control) != 0x00000000 &&   \
                                                  (adaptation_field_control) != 0x00000002)

//  increments and takes care of the wrapping case
#define INCREMENTED_CONTINUITY_COUNTER_VALUE(c) ((BYTE) (((c) + 1) & 0x0000000f))

//  adaptation field macros follow --

//  pp. 20 of the MPEG-2 systems spec
//      adaptation_field_control == '11' ==> adaptation_field_length in [0, 182]
//      adaptation_field_control == '10' ==> adaptation_field_length == 183
#define VALID_ADAPTATION_FIELD(adaptation_field_control, adaptation_field_length)    \
                                                (((adaptation_field_control) == 0x3 && (adaptation_field_length) <= 182) ||   \
                                                 ((adaptation_field_control) == 0x2 && (adaptation_field_length) == 183))

//  pp. 41 of the MPEG-2 systems spec
//      make sure that the pointer_field field that precedes a PSI section is valid
//      i.e. doesn't index us off the end of the packet; we check validity by making
//      sure that it falls in the range [0, TransportPayloadLength - 1); 0 means
//      the section follows immediately; if it's > 0, it must be
//      < TransportPayloadLength - 1 because TransportPayloadLength - 1 is the payload
//      that remains AFTER pointer_field, and we need at least 1 byte of section data
#define VALID_POINTER_FIELD(pointer_field, TransportPayloadLength)      ((pointer_field) < (TransportPayloadLength) - 1)

#define IS_AV_STREAM(stream_type)               (((stream_type) == 0x00000001) ||   \
                                                 ((stream_type) == 0x00000002) ||   \
                                                 ((stream_type) == 0x00000003) ||   \
                                                 ((stream_type) == 0x00000004))

//  ---------------------------------------------------------------------------
//  PES packet - related

//  true/false if the stream is a padding stream
#define IS_PADDING_STREAM(stream_id)            ((stream_id) == STREAM_ID_PADDING_STREAM)

//  true/false if stream is tier_1 (see first conditional in table 2-17,
//  MPEG2 systems specification, p. 30)
#define IS_TIER1_STREAM(stream_id)      (((stream_id) != STREAM_ID_PROGRAM_STREAM_MAP)          &&  \
                                         ((stream_id) != STREAM_ID_PADDING_STREAM)              &&  \
                                         ((stream_id) != STREAM_ID_PRIVATE_STREAM_2)            &&  \
                                         ((stream_id) != STREAM_ID_ECM)                         &&  \
                                         ((stream_id) != STREAM_ID_EMM)                         &&  \
                                         ((stream_id) != STREAM_ID_PROGRAM_STREAM_DIRECTORY)    &&  \
                                         ((stream_id) != STREAM_ID_DSMCC_STREAM)                &&  \
                                         ((stream_id) != STREAM_ID_H_222_1_TYPE_E))



//  true/false if stream is tier_2 (see second conditional in table 2-17,
//  MPEG2 systems specification, p. 32)
#define IS_TIER2_STREAM(stream_id)      (((stream_id) == STREAM_ID_PROGRAM_STREAM_MAP)          ||  \
                                         ((stream_id) == STREAM_ID_PRIVATE_STREAM_2)            ||  \
                                         ((stream_id) == STREAM_ID_ECM)                         ||  \
                                         ((stream_id) == STREAM_ID_EMM)                         ||  \
                                         ((stream_id) == STREAM_ID_PROGRAM_STREAM_DIRECTORY)    ||  \
                                         ((stream_id) == STREAM_ID_DSMCC_STREAM)                ||  \
                                         ((stream_id) == STREAM_ID_H_222_1_TYPE_E))

#define IS_TIER3_STREAM(stream_id)      ((stream_id) == STREAM_ID_PADDING_STREAM)

//  total PES header length (includes usual fields, optional fields,
//  stuffing bytes); add the return value to a byte pointer to PES header
//  to point to the byte that follows
//  ho = PES_header_data_length
#define PES_HEADER_TOTAL_LENGTH(ho)     (PES_TIER1_REQ_HEADER_FIELD_LEN + (ho))

//  returns the total PES packet length, or -1 if pl (PES_packet_length) is 0
#define TOTAL_PES_PACKET_LENGTH(pl)     ((pl) != 0 ? ((pl) + PES_ALWAYS_HEADER_FIELD_LEN) : -1)

//  true/false if packet has video
#define IS_PES_VIDEO(id)                IN_RANGE(id,STREAM_ID_VIDEO_STREAM_MIN,STREAM_ID_VIDEO_STREAM_MAX)

//  true/false if the packet has audio
#define IS_PES_AUDIO(id)                IN_RANGE(id,STREAM_ID_AUDIO_STREAM_MIN,STREAM_ID_AUDIO_STREAM_MAX)

//  ---------------------------------------------------------------------------
//  macrovision-related

//  PES_packet_length defines the number of bytes that *follow* the field
#define PES_PACKET_LENGTH_IS_MIN_VALID_MACROVISION(pbPES)   (PES_PACKET_LENGTH_VALUE(pbPES) >= (1 + 4 + 2))

#define SUBSTREAMID_IS_MACROVISION(pbPES)                   (PES_MV_SUBSTREAM_ID(pbPES) == SUBSTREAM_ID_MACROVISION)
#define IS_VALID_MACROVISION_PACKET(pbPES)                  (PES_PACKET_LENGTH_IS_MIN_VALID_MACROVISION(pbPES) && SUBSTREAMID_IS_MACROVISION(pbPES))

class CTsPsi;
class CPATProcessor;
class CPMTProcessor;


#define STREM_TYPE_H264			0x1B
#define STREM_TYPE_AAC_ADTS		0x0F
#define MAX_PROGRAM_NUMBER_IN_PAT_SECTION               252  // (1021-12)/4 = 252 

// max section length = 1021 bytes
// total bytes between program_number and program_info_length is 9
// CRC_32 is 4 bytes elementary inforamtion loop is 5 bytes each
//
#define MAX_ELEMENTARY_STREAM_NUMBER_IN_PMT_SECTION     202   //(1021-9-4)/5

#define PMT_BYTE_NUMBER_BEFORE_ES                       12
#define PMT_ES_BYTE_NUMBER_BEFORE_ESDESCRIPTOR          5
#define PAT_SECTION_LENGTH_EXCLUDE_PROGRAM_DESCRIPTOR   9
#define PAT_PROGRAM_DESCRIPTOR_LENGTH                   4

#define PMT_BYTE_NUMBER_BEFORE_PROGRAM_NUMBER           3
#define PMT_CRC_LENGTH                                  4

#define MAX_PROGRAM_NUMBER_IN_TRANSPORT_STREAM          500  // (arbitrary)

#define PAT_TABLE_ID                                    0x00 // Table 2-26 ISO/IEC 13818-1:1996(E)
#define PMT_TABLE_ID                                    0x02 // (see above table)



// inside PAT section
typedef struct Program_Descriptor
{
    int program_number;
    int network_or_program_map_PID;
} PROGRAM_DESCRIPTOR;


// inside PMT section
typedef struct Elementary_Stream_Info
{
    int stream_type;
    int elementary_PID;
    int ES_info_length;
} ELEMENTARY_STREAM_INFO;


// PSI section: common fields in PAT and PMT
struct MPEG2_PSI_SECTION
{
    int table_id;
    bool  section_syntax_indicator;
    int section_length;
    int version_number;
    bool  current_next_indicator;
    int section_number;
    int last_section_number;

    long long CRC_32;        // ignore for now
};


// PAT section store object
struct MPEG2_PAT_SECTION :
    public MPEG2_PSI_SECTION
{

    int transport_stream_id;

    int number_of_programs; // derived
    PROGRAM_DESCRIPTOR program_descriptor[MAX_PROGRAM_NUMBER_IN_PAT_SECTION];
#if 1
	void DumpPsi()
	{
		printf("table_id = %d", table_id);
		printf("section_syntax_indicator = %d\n", section_syntax_indicator);
		printf("section_length = %d\n", section_length);
		printf("version_number = %d\n", version_number);
		printf("current_next_indicator = %d\n", current_next_indicator);
		printf("section_number = %d\n", section_number);
		printf("last_section_number = %d\n", last_section_number);
		printf("number_of_programs = %d\n",number_of_programs);
		printf("transport_stream_id = %d\n",transport_stream_id);
	}
#endif
    bool Parse(unsigned char * pbBuf // points to first byte of psi section
                        )
    {
        // common psi fields
        table_id                    =   PAT_TABLE_ID_VALUE                  (pbBuf) ;
        section_syntax_indicator    =   PAT_SECTION_SYNTAX_INDICATOR_BIT    (pbBuf) ;
        section_length              =   PAT_SECTION_LENGTH_VALUE            (pbBuf) ;
        version_number              =   PAT_VERSION_NUMBER_VALUE            (pbBuf) ;
        current_next_indicator      =   PAT_CURRENT_NEXT_INDICATOR_BIT      (pbBuf) ;
        section_number              =   PAT_SECTION_NUMBER_VALUE            (pbBuf) ;
        last_section_number         =   PAT_LAST_SECTION_NUMBER_VALUE       (pbBuf) ;

        // pat specific fields
        transport_stream_id         =   PAT_TRANSPORT_STREAM_ID_VALUE       (pbBuf) ;
        number_of_programs          =   (section_length - PAT_SECTION_LENGTH_EXCLUDE_PROGRAM_DESCRIPTOR)/PAT_PROGRAM_DESCRIPTOR_LENGTH; 


        // parse PAT program descriptor
        for(int i=0; i< number_of_programs ; i++)
        {
            program_descriptor[i].program_number             = PAT_PROGRAM_DESCRIPTOR_PROGRAM_NUMBER_VALUE   (pbBuf,i);
            program_descriptor[i].network_or_program_map_PID = PAT_PROGRAM_DESCRIPTOR_PID_VALUE              (pbBuf,i);
        } 
		DumpPsi();
        return true;
    }

    bool Copy(MPEG2_PAT_SECTION* pSource)
    {       
        table_id                    = pSource->table_id;
        section_syntax_indicator    = pSource->section_syntax_indicator;
        section_length              = pSource->section_length;
        version_number              = pSource->version_number;
        current_next_indicator      = pSource->current_next_indicator;
        section_number              = pSource->section_number;
        last_section_number         = pSource->last_section_number;

        transport_stream_id         = pSource->transport_stream_id;
        number_of_programs          = pSource->number_of_programs;

        for(int i=0; i< number_of_programs ; i++)
        {
            program_descriptor[i].program_number             = pSource->program_descriptor[i].program_number;
            program_descriptor[i].network_or_program_map_PID = pSource->program_descriptor[i].network_or_program_map_PID;
        } 

        return true;
    }

};


struct MPEG2_PMT_SECTION :
    public MPEG2_PSI_SECTION
{
    int program_number;
    int PCR_PID;
    int program_info_length;

    int number_of_elementary_streams; //derived
    ELEMENTARY_STREAM_INFO elementary_stream_info[MAX_ELEMENTARY_STREAM_NUMBER_IN_PMT_SECTION];

    bool Parse(unsigned char * pbBuf) // points to first byte of psi section)
    {
        // common psi fields
        table_id                    =   PMT_TABLE_ID_VALUE                  (pbBuf) ;
        section_syntax_indicator    =   PMT_SECTION_SYNTAX_INDICATOR_BIT    (pbBuf) ;
        section_length              =   PMT_SECTION_LENGTH_VALUE            (pbBuf) ;
        version_number              =   PMT_VERSION_NUMBER_VALUE            (pbBuf) ;
        current_next_indicator      =   PMT_CURRENT_NEXT_INDICATOR_BIT      (pbBuf) ;
        section_number              =   PMT_SECTION_NUMBER                  (pbBuf) ;
        last_section_number         =   PMT_LAST_SECTION_NUMBER             (pbBuf) ;

        // pmt specific fields
        program_number              =   PMT_PROGRAM_NUMBER_VALUE            (pbBuf) ;
        PCR_PID                     =   PMT_PCR_PID_VALUE                   (pbBuf) ;
        program_info_length         =   PMT_PROGRAM_INFO_LENGTH_VALUE       (pbBuf) ;

        BYTE* pStartEs  = pbBuf + PMT_BYTE_NUMBER_BEFORE_ES + program_info_length;//points to the stream_type of the 1st elementary stream
        BYTE* pEndEs    = pbBuf + section_length + PMT_BYTE_NUMBER_BEFORE_PROGRAM_NUMBER - PMT_CRC_LENGTH   ;   //points to the byte before CRS_323, 4
        BYTE* pTmp ;

        unsigned short EsNumber = 0;
        int offset = 0;

        for(pTmp = pStartEs; pTmp < pEndEs; pTmp += offset)
        {
            offset = PMT_ES_BYTE_NUMBER_BEFORE_ESDESCRIPTOR + PMT_STREAM_RECORD_ES_INFO_LENGTH      (pTmp); //5+ES_info_length
            elementary_stream_info[EsNumber].stream_type    = PMT_STREAM_RECORD_STREAM_TYPE_VALUE   (pTmp);

            elementary_stream_info[EsNumber].elementary_PID = PMT_STREAM_RECORD_ELEMENTARY_PID      (pTmp);

            elementary_stream_info[EsNumber].ES_info_length = PMT_STREAM_RECORD_ES_INFO_LENGTH      (pTmp);
        
            EsNumber++;
        }

        number_of_elementary_streams = EsNumber;

        return true;
    }

    bool Copy(MPEG2_PMT_SECTION* pSource)
    {
        
        table_id                    = pSource->table_id;
        section_syntax_indicator    = pSource->section_syntax_indicator;
        section_length              = pSource->section_length;
        version_number              = pSource->version_number;
        current_next_indicator      = pSource->current_next_indicator;
        section_number              = pSource->section_number;
        last_section_number         = pSource->last_section_number;

        program_number              = pSource->program_number;
        PCR_PID                     = pSource->PCR_PID;
        program_info_length         = pSource->program_info_length;

        number_of_elementary_streams= pSource->number_of_elementary_streams;

        for(int i=0; i< (int)number_of_elementary_streams ; i++)
        {
            elementary_stream_info[i].stream_type    = pSource->elementary_stream_info[i].stream_type;
            elementary_stream_info[i].elementary_PID = pSource->elementary_stream_info[i].elementary_PID;
            elementary_stream_info[i].ES_info_length = pSource->elementary_stream_info[i].ES_info_length;
        }

        return true;
    }


};

typedef struct Mpeg2_Program
{
    int               program_number;
    int               network_or_program_map_PID;
    int               number_of_ESs;

    MPEG2_PAT_SECTION   mpeg2_pat_section;
    MPEG2_PMT_SECTION   mpeg2_pmt_section;
}MPEG2_PROGRAM;

class CPrograms
{
public:
    MPEG2_PROGRAM*  m_programs[MAX_PROGRAM_NUMBER_IN_TRANSPORT_STREAM];
    int             m_ProgramCount;

    void init_programs()
    {
        m_ProgramCount = 0;
    }

    // is the program existing in the array already?
    bool find_program(const int dwProgramNumber, int *pIndex)
    {
        for (int i = 0; i<m_ProgramCount; i++){
            if(m_programs[i]->program_number == dwProgramNumber){
                *pIndex = i;
                return true;
            }
        }
        return false;
    }

    // add a program from a PAT section, if existing in the array, update it
    bool add_program_from_pat(MPEG2_PAT_SECTION* pmpeg2_pat_section, int SectionIndex)
    {
        MPEG2_PROGRAM *tmp;

        // if already exit, update it:
        int index;
        if(find_program(pmpeg2_pat_section->program_descriptor[SectionIndex].program_number, &index)){
            return update_program_from_pat(index, pmpeg2_pat_section, SectionIndex);
        }

        // no more room, failed
        if(m_ProgramCount + 1 > MAX_PROGRAM_NUMBER_IN_TRANSPORT_STREAM)
            return false;

        // copy program from pat section to the allocated space:
        tmp = (MPEG2_PROGRAM *)malloc(sizeof( MPEG2_PROGRAM ) );
        if( tmp == NULL) {
            return false;
        }
		memset(tmp, 0x00, sizeof( MPEG2_PROGRAM ));
        copy_program_from_pat(pmpeg2_pat_section, SectionIndex, tmp);  

        // put new program on array
        m_programs[m_ProgramCount] = tmp;
        m_ProgramCount++;

        return true;
    }

    bool update_program_from_pmt(MPEG2_PMT_SECTION* pmpeg2_pmt_section)
    {
        // if already exit, update it:
        int index;
        if(find_program(pmpeg2_pmt_section->program_number, &index)){
            m_programs[index]->number_of_ESs = pmpeg2_pmt_section->number_of_elementary_streams;
            m_programs[index]->mpeg2_pmt_section.Copy(pmpeg2_pmt_section);
            return true;
        }   
        return false;
    }

    bool copy_program_from_pat(MPEG2_PAT_SECTION * pSection, 
                                       int SectionIndex, MPEG2_PROGRAM *pDest)
    {
        pDest->program_number               = pSection->program_descriptor[SectionIndex].program_number;
        pDest->network_or_program_map_PID   = pSection->program_descriptor[SectionIndex].network_or_program_map_PID;
        pDest->mpeg2_pat_section.Copy(pSection);

        return true;
    }

    bool update_program_from_pat(int ProgramIndex, 
                                         MPEG2_PAT_SECTION* pmpeg2_pat_section, 
                                         int SectionIndex)
    {
        return copy_program_from_pat(pmpeg2_pat_section, SectionIndex, m_programs[ProgramIndex]);
    }

    bool free_programs()
    {
        for(int i= 0; i<m_ProgramCount; i++){
            free(m_programs[i]);
        }

        return true;
    }

};

// PAT section processor:

class CPATProcessor
{
public:

    CPATProcessor(CTsPsi *pParser,CPrograms * pPrograms, int *phr);
    ~CPATProcessor();

    bool process(unsigned char * pbBuffer, long lDataLen);
    bool store(unsigned char * pbBuffer, long lDataLen);
    bool flush(); // delete all programs, PAT and PMT section list

    // from pat: keep track of stream id and pat version # change
    int           m_current_transport_stream_id;  // one pat table per transport stream
    int           m_current_pat_version_number;   // one version# per PAT table

private:

    //parser filter
    CTsPsi     *m_pParser;
    CPrograms  *m_pPrograms;

    int             *m_phr;

    // keep track of section number
    int           m_pat_section_count;
    int           m_pat_section_number_array[MAX_PROGRAM_NUMBER_IN_TRANSPORT_STREAM];

    // keep track of pmt pids
    int           m_mapped_pmt_pid_array[MAX_PROGRAM_NUMBER_IN_TRANSPORT_STREAM];
    int           m_mapped_pmt_pid_count;

    bool IsNewPATSection(int dwSectionNumber);
    bool HasPmtPidMappedToPin(int dwPid);
    bool MapPmtPid(int dwPid);
    bool UnmapPmtPid();
};

// keep track of section version_number for each pmt section identified by program_number
typedef struct Pmt_Version_Of_A_Program
{
    // in each pmt section, the section number field shall be set to zero. 
    // Sections are identified by the program_number field (ISO/IEC 13818-1:1996(E))
    int pmt_program_number;
    int pmt_section_version;
} PMT_VERSION_OF_A_PROGRAM;

// PMT processor:

class CPMTProcessor
{
public:

    CPMTProcessor(CTsPsi *pParser,CPrograms * pPrograms,int *phr);
    ~CPMTProcessor();

    bool process(unsigned char * pbBuffer, long lDataLen);
    bool store(unsigned char * pbBuffer, long lDataLen);


private:
    CTsPsi           *m_pParser;
    CPrograms        *m_pPrograms;

    // from pmt
    // keep track of section # of PMT sections read 
    // one pmt section per program, so we keep an array of section #, 
    // along with their corresponding current version#
    int                m_pmt_section_count; 

    // note: the number of pmt sections should be >= MAX_PROGRAM_NUMBER_IN_TRANSPORT_STREAM

    PMT_VERSION_OF_A_PROGRAM  m_pmt_program_number_version_array[MAX_PROGRAM_NUMBER_IN_TRANSPORT_STREAM];

    bool HasPMTVersionOfThisProgramChanged(int dwProgramNumber,int dwVersionNumber);
    bool IsNewPMTSection(int dwProgramNumber);
};


class CTsPsi  :   public CTsPsiIf
{
public:

    CTsPsi();
    ~CTsPsi();

	int GetCountOfPrograms();
	int GetRecordProgramMapPid (int nIndex, int *nVal);
    int GetCountOfElementaryStreams(int nProgramNumber);
	int GetElementaryPidByStreamType(int nProgNumber, int nStreamType);

    // from PAT
    int GetTransportStreamId(int *pstreamId);
    int GetPatVersionNumber (unsigned char *pPatVersion);
    
	int GetRecordProgramNumber(int dwIndex, int * pwVal);
    int FindRecordProgramMapPid(int wProgramNumber);
	int RegisterCallback(void *pCallerCtx, PsiCallback_T fnCallback)
	{
		m_fnCallback = fnCallback; m_pCallerCtx = pCallerCtx;
		return 0;
	}

    // from PMT
    int GetPmtVersionNumber        (int wProgramNumber, unsigned char *pPmtVersion);

    int GetRecordStreamType        (int wProgramNumber,int dwRecordIndex, unsigned char *pbVal);
    int GetRecordElementaryPid     (int wProgramNumber,int dwRecordIndex, int *pwVal);
	int GetRecordProgramPcrPid     (int wProgramNumber);

    CPATProcessor*   m_pPatProcessor;   // Handles PAT sections
    CPMTProcessor*   m_pPmtProcessor;   // Handles PMT sections

    CPrograms        m_Programs;               // Updated by the above two objects
    bool m_bCreated;

	int Parse(unsigned char  *pData, int lDataLen);
	int  GetPatProcessor(CPATProcessor ** ppPatProcessor);
	int IsTableComplete();

	bool AddPidToFilter(int dwPmtPid);
	bool NotifyEvent(int nEvent);

	PsiCallback_T   m_fnCallback;
	void *          m_pCallerCtx;
}; // CTsPsi


#endif


