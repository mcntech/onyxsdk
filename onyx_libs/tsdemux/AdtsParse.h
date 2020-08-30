#ifndef __ADTS_PARSE_H__
#define __ADTS_PARSE_H__

#define OBJECTTYPE_AACLC    (2)
#define ADTS_HEADER_LEN     (7)

#define MAX_SAMPLE_RATES            16
// Various aac sample rates
class AdtsParse
{
public:
static int  GetAudSamplerateFromAdt(unsigned char *pAdt)
{
	static int adtsSamplingRates[MAX_SAMPLE_RATES] = 
                               {96000, 88200, 64000, 48000, 
							    44100, 32000, 24000, 22050, 
								16000, 12000, 11025, 8000,  
								7350,  0,     0,     0 };

    int   sampleRateId;           /* Sample rate ID                         */
	if( pAdt[0] == 0xff && (pAdt[1]&0xF0) == 0xf0)  {
        sampleRateId    = (pAdt[2] >> 2 ) & 0x0F;
		if(sampleRateId >= 0 && sampleRateId < MAX_SAMPLE_RATES){
			return adtsSamplingRates[sampleRateId];
		}
	}
	return 0;
}

static void getAacConfigFromAdts(unsigned char *pConfig, unsigned char *pAdts)
{
        int profileVal      = pAdts[2] >> 6;
        int sampleRateId    = (pAdts[2] >> 2 ) & 0x0f;
        int channelsVal     = ((pAdts[2] & 0x01) << 2) | ((pAdts[3] >> 6) & 0x03);
        pConfig[0]  = (profileVal + 1) << 3 | (sampleRateId >> 1);
        pConfig[1]  = ((sampleRateId & 0x01) << 7) | (channelsVal <<3);
}

/*
 * ASC 5bits : ObjectType, 
 *     4bits : SamplingFrequencyIndex
 *     4bits : ChannelConfiguration
 */
static void ConvertAdt2Mp4Asc(unsigned char *pAdt, char *Asc)
{
    int   profileVal;             /* Profile value                          */
    int   sampleRateId;           /* Sample rate ID                         */
    int   channelsVal;            /* Channel Value                          */

	if( pAdt[0] == 0xff && pAdt[1] == 0xf1)  {
        profileVal      = pAdt[2] >> 6;
        sampleRateId    = (pAdt[2] >> 2 ) & 0x0f;
        channelsVal     = ((pAdt[2] & 0x01) << 2) | ((pAdt[3] >> 6) & 0x03);
        Asc[0]  = (profileVal + 1) << 3 | (sampleRateId >> 1);
        Asc[1]  = ((sampleRateId & 0x01) << 7) | (channelsVal <<3);
	} else {

	}
}

static int getAdtsHdrLen(unsigned char *pAdts)
{
	return (pAdts[1] & 0x01) ? 7 : 9;
}

static unsigned short getAdtsFrameLen(unsigned char *pAdts)
{
	unsigned short usLen = (((unsigned short)pAdts[3] << 11) & 0x1800) |
							(((unsigned short)pAdts[4] << 3) & 0x07F8) |
							(((unsigned short)pAdts[5] >> 5) & 0x0007);
	return usLen;
}
static int  GetAudNumChanFromAdt(unsigned char *pAdts)
{
	int channelsVal = ((pAdts[2] & 0x01) << 2) | ((pAdts[3] >> 6) & 0x03);
	return channelsVal;
}

};


#endif //__ADTS_PARSE_H__