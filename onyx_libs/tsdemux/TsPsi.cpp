//#include "dbg.h"
#include "TsPsi.h"
//#include "resource.h"
#include "ts_dbg.h"

CTsPsi::CTsPsi() 
{
	int hr = 0;
    m_Programs.init_programs();
    m_pPatProcessor = new CPATProcessor(this, &m_Programs,  &hr);
    m_pPmtProcessor = new CPMTProcessor(this,  &m_Programs, &hr);
	m_Programs.init_programs();
	m_fnCallback = NULL;
}


CTsPsi::~CTsPsi()
{
	if(m_pPatProcessor)
		delete 	m_pPatProcessor;;
	if(m_pPmtProcessor)
		delete 	m_pPmtProcessor;
}

int CTsPsi::GetTransportStreamId(int *pstreamId)
{
    if(!pstreamId)
        return -1;

    CPATProcessor*   pPatProcessor = NULL;
    int hr = GetPatProcessor(&pPatProcessor);
    if(hr == 0 && pPatProcessor)
        *pstreamId = (int) pPatProcessor->m_current_transport_stream_id;
    return 0;
}

int CTsPsi::GetPatVersionNumber(unsigned char *pPatVersion)
{
    if(!pPatVersion)
        return -1;


    CPATProcessor*   pPatProcessor;
    int hr = GetPatProcessor(&pPatProcessor);
    if(hr == 0)
        *pPatVersion = (unsigned char) pPatProcessor->m_current_pat_version_number;

    return 0;
}

int CTsPsi::GetCountOfPrograms()
{
   return m_Programs.m_ProgramCount;

}

int CTsPsi::GetRecordProgramNumber(int dwIndex, int * pwVal)
{
    if(pwVal == NULL)
        return -1;

    *pwVal = (int) m_Programs.m_programs[dwIndex]->program_number;

    return 0;
}

int CTsPsi::GetRecordProgramMapPid(int dwIndex, int * pwVal)
{
    *pwVal = (int) m_Programs.m_programs[dwIndex]->network_or_program_map_PID;

    return 0;
}

int CTsPsi::FindRecordProgramMapPid(int wProgramNumber)
{
    int i;
    for (i = 0; i<(int) m_Programs.m_ProgramCount; i++){
        if(m_Programs.m_programs[i]->program_number == wProgramNumber){        
            return (int) m_Programs.m_programs[i]->network_or_program_map_PID;
        }
    }
    return 0;
}

int CTsPsi::GetRecordProgramPcrPid(int wProgramNumber)
{
    int i;
    for (i = 0; i<(int) m_Programs.m_ProgramCount; i++){
        if(m_Programs.m_programs[i]->program_number == wProgramNumber){        
            return (int) m_Programs.m_programs[i]->mpeg2_pmt_section.PCR_PID;
        }
    }
    return 0;
}

int CTsPsi::GetPmtVersionNumber(int wProgramNumber, unsigned char *pVal)
{
    for (int i = 0; i<(int) m_Programs.m_ProgramCount; i++){
        if(m_Programs.m_programs[i]->program_number == wProgramNumber){
            *pVal = (unsigned char) m_Programs.m_programs[i]->mpeg2_pmt_section.version_number;
            return 0;

        }
    }

    return -1;
}

int CTsPsi::GetCountOfElementaryStreams(int wProgramNumber)
{
    for (int i = 0; i<(int) m_Programs.m_ProgramCount; i++){
        if(m_Programs.m_programs[i]->program_number == wProgramNumber){
            return m_Programs.m_programs[i]->mpeg2_pmt_section.number_of_elementary_streams;

        }
    }
    return 0;
}

int CTsPsi::GetRecordStreamType(int wProgramNumber,int dwRecordIndex, unsigned char *pbVal)
{
    int i;

    for (i = 0; i<m_Programs.m_ProgramCount; i++){
        if(m_Programs.m_programs[i]->program_number == wProgramNumber){
            *pbVal = (unsigned char) m_Programs.m_programs[i]->mpeg2_pmt_section.elementary_stream_info[dwRecordIndex].stream_type;
            return 0;
        }
    }

    return -1;
}

int CTsPsi::GetRecordElementaryPid(int wProgramNumber,int dwRecordIndex, int *pwVal)
{
    int i;

    for (i = 0; i<m_Programs.m_ProgramCount; i++){
        if(m_Programs.m_programs[i]->program_number == wProgramNumber){
            *pwVal = (int) m_Programs.m_programs[i]->mpeg2_pmt_section.elementary_stream_info[dwRecordIndex].elementary_PID;
            return 0;
        }
    }

    return -1;
}

int CTsPsi::GetElementaryPidByStreamType(int wProgramNumber,int dwStreamType)
{
    int i,j;

    for (i = 0; i<m_Programs.m_ProgramCount; i++){
        if(m_Programs.m_programs[i]->program_number == wProgramNumber){
			int nNumEs = m_Programs.m_programs[i]->mpeg2_pmt_section.number_of_elementary_streams;
			for (j = 0; j < nNumEs; j++){
				if((int) m_Programs.m_programs[i]->mpeg2_pmt_section.elementary_stream_info[j].stream_type == dwStreamType) {
					return m_Programs.m_programs[i]->mpeg2_pmt_section.elementary_stream_info[j].elementary_PID;
				}
			}
        }
    }

    return 0;
}


int CTsPsi::Parse(unsigned char  *pData, int lDataLen)
{
	int hr = 0;
	
	int nPid = PID_VALUE(pData);
	int nProgram = 0;
	int nNumPrograms = 0;
	int nProgPid = 0;

	if((nNumPrograms = GetCountOfPrograms()) > 0) {
		int i;
		for (i =0; i < nNumPrograms; i++) {
			if(GetRecordProgramNumber(i, &nProgram) == 0) {
				nProgPid = FindRecordProgramMapPid(nProgram);
				if(nProgPid == nPid)
					break;
			}
		}
	}
	if(nPid == 0/*PAT PID*/ || nPid == nProgPid) {
		int pos = 0;
		
		/* Check Adaptation field present and move the pos, bt adaptation field lenght, if required*/
		if ((0x20 & pData[pos+3]) == 0x20) {
			pos += pData[pos+4] + 1;
		}

		long lPointer = pData[pos + 4];
		long lOffset = pos + 4 + 1 /*Pointer Byte*/ + lPointer/* Pointer offset */;
		pData += lOffset;
		// PAT section
		if(*pData == PAT_TABLE_ID && VALID_PSI_HEADER(pData)){
			m_pPatProcessor->process(pData, lDataLen);
		}
		//PMT section
		else if(*pData == PMT_TABLE_ID && VALID_PSI_HEADER(pData)){
			m_pPmtProcessor->process(pData, lDataLen);
		}
	}
    return hr;

}

int CTsPsi::IsTableComplete()
{
	for (int i = 0; i<(int) m_Programs.m_ProgramCount; i++){
		if (m_Programs.m_programs[i]->mpeg2_pmt_section.number_of_elementary_streams == 0)
			return 0;
    }
	return m_Programs.m_ProgramCount;
}

//
// GetPatProcessor 
//
int CTsPsi::GetPatProcessor(CPATProcessor ** ppPatProcessor)
{
    if(ppPatProcessor == NULL)
        return -1;

	if(m_pPatProcessor != NULL)
        *ppPatProcessor = m_pPatProcessor;
    else
        return -1;

    return 0;
} //GetPatProcessor

bool CTsPsi::AddPidToFilter(int dwPmtPid)
{
	if(m_fnCallback) {
		PMT_INF_T pmtInf;
		pmtInf.nPmtPid = dwPmtPid;
		m_fnCallback(m_pCallerCtx, EVENT_NEW_PMT, &pmtInf);
	}
	return true;
}

bool CTsPsi::NotifyEvent(int nEventId)
{
	if(m_fnCallback) {
		m_fnCallback(m_pCallerCtx, EVENT_PROGRAM_CHANGED, NULL);
	}
	return true;

}

CPATProcessor::CPATProcessor(CTsPsi *pParser,
                             CPrograms * pPrograms, 
                             int *phr)
: m_pParser(pParser)
, m_pPrograms(pPrograms)
, m_current_transport_stream_id( 0xff)
, m_pat_section_count(0)
, m_mapped_pmt_pid_count(0)
{
    //pDemux->Release();
}

CPATProcessor::~CPATProcessor()
{
}


bool CPATProcessor::IsNewPATSection(int dwSectionNumber)
{

    bool bIsNewSection = true;

    for(int i = 0; i<(int)m_pat_section_count; i++) {
        if(m_pat_section_number_array[i] == dwSectionNumber) {
            return false;
        }
    }

    return bIsNewSection;

}

//
// MapPmtPid
//
bool CPATProcessor::MapPmtPid(int dwPmtPid)
{
    bool bResult = true;
	m_pParser->AddPidToFilter(dwPmtPid);
    return bResult;
}// MapPmtPid


//
// UnmapPmtPid
//
// unmap all pmt pids from demux psi output pin
bool CPATProcessor::UnmapPmtPid()
{
    bool bResult = true;
    return bResult;
} // UnmapPmtPid

//
// process
//
// process the pat section received (see data flow chart in SDK document)
bool CPATProcessor::process(unsigned char * pbBuffer, long lDataLen)
{
    bool bResult = true;

    // new transport stream
    int transport_stream_id = PAT_TRANSPORT_STREAM_ID_VALUE(pbBuffer);
    if( transport_stream_id != m_current_transport_stream_id ) {
        // flush all programs
        if(!flush())
            return false;
        bResult = store(pbBuffer, lDataLen);
    }   else {

        // new section
        int section_number = PAT_SECTION_NUMBER_VALUE(pbBuffer);
        if( IsNewPATSection(section_number) ){
            bResult = store(pbBuffer, lDataLen);
        } else { 
            // new PAT version, i.e. transport stream changed (such as adding or deleting programs)
            int version_number = PAT_VERSION_NUMBER_VALUE(pbBuffer);
            if( m_current_pat_version_number != version_number ){
                if(!flush())
                    return false;
                bResult = store(pbBuffer, lDataLen);
            }
            // else discard the section and do nothing;
        }
    }
    return bResult;
} //process

//
//store
//
// add new program or update existing program
bool CPATProcessor::store(unsigned char * pbBuffer, long lDataLen)
{
    bool bResult =true;
    //if it is not valid pat section, ignore it
    if(!VALID_PAT_SECTION(pbBuffer))
        return false;
    MPEG2_PAT_SECTION mpeg2_pat_section;
    // if we can't parse it, then ignore it
    if(!mpeg2_pat_section.Parse(pbBuffer))
        return false;


    // update m_current_transport_stream_id
    m_current_transport_stream_id = mpeg2_pat_section.transport_stream_id;

    // update m_current_pat_version_number
    m_current_pat_version_number = mpeg2_pat_section.version_number;

    // update m_pat_section_number_list 
    m_pat_section_number_array[m_pat_section_count] = mpeg2_pat_section.section_number; 
    m_pat_section_count ++;

    // update s_mpeg2_programs, for each program:
    for( int i=0;i<(int) (mpeg2_pat_section.number_of_programs); i++)// For each program contained in this pat section
    {
    
        //  add new program or update existing program  
        m_pPrograms->add_program_from_pat(&mpeg2_pat_section, i);
    
        // if the pmt PID has not been mapped to demux output pin, map it
        if( !HasPmtPidMappedToPin(mpeg2_pat_section.program_descriptor[i].program_number)) {
            if(!MapPmtPid(mpeg2_pat_section.program_descriptor[i].network_or_program_map_PID))
                return false;
        }
    }
#if 0 // TODO
    m_pParser->NotifyEvent(EC_PROGRAM_CHANGED,0,(LONG_PTR)(IBaseFilter*)m_pParser);
#endif
    return bResult;

} // store

//
//HasPmtPidMappedToPin
//
// if the pmt pid has been mapped to demux's psi output pin, return true;
bool CPATProcessor::HasPmtPidMappedToPin(int dwPid)
{
    bool bMapped = false;
    for(int i = 0; i<(int)m_mapped_pmt_pid_count;i++){
        if(m_mapped_pmt_pid_array[i] == dwPid)
            return true;
    }
    return bMapped;
}//HasPmtPidMappedToPin

//
// flush
//
// flush an array of struct: m_mpeg2_program[];
// and unmap all PMT_PIDs pids, except one: PAT
bool CPATProcessor::flush()
{
    bool bResult = true;
    bResult = m_pPrograms->free_programs();
    if(bResult == false)
        return bResult;
    bResult = UnmapPmtPid();
    return bResult;
}// flush

CPMTProcessor::CPMTProcessor(CTsPsi *pParser,CPrograms * pPrograms, int *phr)
: m_pParser(pParser)
, m_pPrograms(pPrograms)
, m_pmt_section_count(0)
{
}

CPMTProcessor::~CPMTProcessor()
{
}

//
// HasPMTVersionOfThisProgramChanged
//
// for a pmt section of a given program_number, if the version is the same as recorded before,
// return true; otherwise, return false;
bool CPMTProcessor::HasPMTVersionOfThisProgramChanged(int dwProgramNumber,
                                                      int dwSectionVersion)
{
    if(m_pmt_section_count != 0){
        for(int i = 0; i<(int)m_pmt_section_count; i++)
            if( dwProgramNumber == m_pmt_program_number_version_array[i].pmt_program_number &&
                dwSectionVersion == m_pmt_program_number_version_array[i].pmt_section_version )
                return false;
    }
    return true;
}// HasPMTVersionOfThisProgramChanged


//
// IsNewPMTSection
//
// if the section number has been received before, return true; else, return false
bool CPMTProcessor::IsNewPMTSection(int dwProgramNumber)
{
    if(m_pmt_section_count != 0){
        // in each pmt section, the section number field shall be set to zero. 
        // Sections are identified by the program_number field (ISO/IEC 13818-1:1996(E))
        for(int i = 0; i<(int)m_pmt_section_count; i++)
            if(dwProgramNumber == m_pmt_program_number_version_array[i].pmt_program_number)
                return false;

    }
    return true;
}// IsNewPMTSection

bool CPMTProcessor::process(unsigned char * pbBuffer, long lDataLen)
{
    bool bResult = true;
    bool current_next_indicator = PMT_CURRENT_NEXT_INDICATOR_BIT(pbBuffer);
    if( current_next_indicator == 0 ){      
        return false;
    }

    int program_number = PMT_PROGRAM_NUMBER_VALUE(pbBuffer);
    if( IsNewPMTSection(program_number)) {    
        bResult = store(pbBuffer, lDataLen);
    }
    else{
        // new pmt version, just for this program
        int version_number = PMT_VERSION_NUMBER_VALUE(pbBuffer);
        if( HasPMTVersionOfThisProgramChanged(program_number, version_number )){
            bResult = store(pbBuffer, lDataLen);
        }
        //else discard and do nothing
    }
    return bResult;
}

bool CPMTProcessor::store(unsigned char * pbBuffer, long lDataLen)
{
    bool bResult = true;

	int fValid = 0;
	unsigned char * pbPMT = pbBuffer;
	fValid = VALID_PSI_HEADER(pbPMT);
	int section_length = PMT_SECTION_LENGTH_VALUE(pbPMT);
	int info_length = PMT_PROGRAM_INFO_LENGTH_VALUE(pbPMT) + 9 + 4;
	fValid = (PMT_SECTION_LENGTH_VALUE(pbPMT) >= PMT_PROGRAM_INFO_LENGTH_VALUE(pbPMT) + 9 + 4);
	fValid = (PMT_TABLE_ID_VALUE(pbPMT) == PMT_TABLE_ID);


    if(!VALID_PMT_SECTION(pbBuffer))
        return false;
    MPEG2_PMT_SECTION mpeg2_pmt_section;

    if(!mpeg2_pmt_section.Parse(pbBuffer))
        return false;

    // keep track of received pmt sections with their program_number and version_number 
    m_pmt_program_number_version_array[m_pmt_section_count].pmt_program_number =
        mpeg2_pmt_section.program_number;
    m_pmt_program_number_version_array[m_pmt_section_count].pmt_section_version =
        mpeg2_pmt_section.version_number;

    // keep track of number of received pmt sections
    m_pmt_section_count++;

    //  add new program or update existing program  
    m_pPrograms->update_program_from_pmt(&mpeg2_pmt_section);

    m_pParser->NotifyEvent(EVENT_PROGRAM_CHANGED);

    return bResult;

}// store


CTsPsiIf *CTsPsiIf::CreateInstance()
{
	CTsPsiIf *pTsPsi =  new CTsPsi();
	return pTsPsi;
}

void CTsPsiIf::CloseInstance(CTsPsiIf *pInstance)
{
	delete pInstance;
}
