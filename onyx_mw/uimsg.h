#ifndef __UI_MSG_H__
#define __UI_MSG_H__


#define ONYX_CMD_PORT          56323

#ifdef WIN32
#define ONYX_PUBLISH_FILE_NAME "C:/Projects/onyx/onyx_em/conf/onyx_publish.conf"
#else
#define ONYX_PUBLISH_FILE_NAME "/etc/onyx_publish.conf"
#endif
#ifdef WIN32
#define ONYX_CORE_FILE_NAME "C:/Projects/onyx/onyx_em/conf/onyx_core.conf"
#else
#define ONYX_CORE_FILE_NAME "/etc/onyx_core.conf"
#endif
#ifdef WIN32
#define ONYX_USER_FILE_NAME "C:/Projects/onyx/onyx_em/conf/onyx_user.conf"
#else
#define ONYX_USER_FILE_NAME "/etc/onyx_user.conf"
#endif

#define SECTION_HLS_SERVER             "hlsserver"
#define SECTION_HLS_PUBLISH            "hlspublish"
#define SECTION_RTMP_PUBLISH           "rtmppublish"
#define SECTION_RTSP_SERVER            "rtspserver"
#define SECTION_RTSP_PUBLISH           "rtsppublish"

#define KEY_RTMP_PUBLISH_PRIMARY_ENABLE "primary_enable"
#define KEY_RTMP_PUBLISH_PRIMARY_HOST   "primary_host"
#define KEY_RTMP_PUBLISH_PRIMARY_PORT   "primary_rtmp_port"
#define KEY_RTMP_PUBLISH_PRIMARY_APP    "primary_application"
#define KEY_RTMP_PUBLISH_PRIMARY_STRM   "primary_stream"
#define KEY_RTMP_PUBLISH_PRIMARY_USER   "primary_user"
#define KEY_RTMP_PUBLISH_PRIMARY_PASSWD "primary_password"

#define KEY_RTMP_PUBLISH_SECONDARY_ENABLE "secondary_enable"
#define KEY_RTMP_PUBLISH_SECONDARY_HOST   "secondary_host"
#define KEY_RTMP_PUBLISH_SECONDARY_PORT   "secondary_rtmp_port"
#define KEY_RTMP_PUBLISH_SECONDARY_APP    "secondary_application"
#define KEY_RTMP_PUBLISH_SECONDARY_STRM   "secondary_stream"
#define KEY_RTMP_PUBLISH_SECONDARY_USER   "secondary_user"
#define KEY_RTMP_PUBLISH_SECONDARY_PASSWD "secondary_password"

#define KEY_RTSP_PUBLISH_HOST           "host"
#define KEY_RTSP_PUBLISH_RTSP_PORT      "rtsp_port"
#define KEY_RTSP_PUBLISH_APPLICATION    "application"
#define KEY_RTSP_PUBLISH_USER           "user"
#define KEY_RTSP_PUBLISH_PASSWD         "password"

#define KEY_RTSP_SERVER_STREAM         "stream"
#define KEY_RTSP_SERVER_RTSP_PORT      "rtsp_port"

#define KEY_HLS_PROTOCOL               "protocol"
#define KEY_HLS_FOLDER                 "folder"
#define KEY_HLS_STREAM                 "stream"
#define KEY_HLS_SEGMENT_DURATION       "segment_duration"
#define KEY_HLS_LIVE_ONLY              "live_only"
#define KEY_HLS_HTTP_SERVER            "http_server"
#define KEY_HLS_S3_BUCKET              "s3_bucket"
#define KEY_HLS_ACCESS_ID              "access_id"
#define KEY_HLS_SECURITY_KEY           "security_key"

#define HLS_PROTOCOL_HTTP              "http://"
#define HLS_PROTOCOL_FILE              "file:///"
#define KEY_HLS_SRVR_PORT              "port"
#define KEY_HLS_SRVR_ROOT              "srvr_root"
#define KEY_HLS_SRVR_INTERNAL          "internal_server"


#define SECTION_SERVERS                "servers"
#define EN_RTSP_SERVER                 "rtspserver"
#define EN_RTSP_PUBLISH                "rtsppublish"
#define EN_HLS_SERVER                  "hlsserver"
#define EN_HLS_PUBLISH                 "hlspublish"
#define EN_RTMP_SERVER                 "rtmpserver"
#define EN_RTMP_PUBLISH                "rtmppublish"

#define ENABLE_AUDIO                   "enable_audio"

#define KEY_INPUT_TYPE                "type"
#define KEY_INPUT_FILE_LOCATION       "location"

#define KEY_INPUT_HOST                 "host"
#define KEY_INPUT_PORT                 "port"
#define KEY_INPUT_APPLICATION          "application"
#define KEY_INPUT_STREAM               "stream"
#define KEY_INPUT_STREAM_AUD_CODEC     "aud_codec"
#define KEY_INPUT_DEVICE               "device"

#define INPUT_STREAM_TYPE_FILE         "file"
#define INPUT_STREAM_TYPE_HLS          "hls"
#define INPUT_STREAM_TYPE_RTSP         "rtsp"
#define INPUT_STREAM_TYPE_CAPTURE      "capture"

#define AUDIO_CODEC_G711U              "g711u"
#define AUDIO_CODEC_NONE               "none"

#define GLOBAL_SECTION                 "global"
#define KEY_ENABLE_VIDEO_MIXER         "video_mixer"
#define KEY_LIBRARY_NAME                "library_name"

#define UI_MSG_HELLOW               0
#define UI_MSG_EXIT					1
#define UI_MSG_SELECT_LAYOUT		2

#define UI_MSG_HLS_PUBLISH_STATS	64


#define UI_STATUS_OK				1
#define UI_STATUS_ERR				0x8000

typedef struct _ONYX_MSG_HDR_T
{
    unsigned long ulCmdId;
    unsigned long ulSize;
} ONYX_MSG_HDR_T;

typedef struct _uiLayoutSelection_t
{
	int		nLayoutId;
} uiSelectLayout_t;


#define HLS_PUBLISH_STATE_UNKNOWN		0
#define HLS_PUBLISH_STOPPED				1
#define HLS_PUBLISH_RUNNING				2

#define HLS_PUBLISH_ERROR_CONNECT_FAIL	0x80000001
#define HLS_PUBLISH_ERROR_XFR_FAIL		0x80000002
#define HLS_PUBLISH_ERROR_INPUT_FAIL	0x80000003


typedef struct _hlspublisgStats_t
{
	int		nState;
	int		nStreamInTime;
	int		nStreamOutTime;
	int		nTotalSegmentTime;
	int		nLostBufferTime;
} hlspublisgStats_t;

typedef struct _uiExit_t
{
	int		nExitCode;
} uiExit_t;

typedef struct UI_MSG_T {
	int		    nMsgId;
	union {
		uiSelectLayout_t     Layout;
	} Msg;
} UI_MSG_T;


#endif //__UI_MSG_H__
