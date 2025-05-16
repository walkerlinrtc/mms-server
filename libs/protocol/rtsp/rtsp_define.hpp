#pragma once
#include <string>
namespace mms
{

#define RTSP_MAX_BUF (1024*1024)

#define RTSP_METHOD_DESCRIBE "DESCRIBE"
#define RTSP_METHOD_ANNOUNCE "ANNOUNCE"
#define RTSP_METHOD_GET_PARAMETER "GET_PARAMETER"
#define RTSP_METHOD_OPTIONS "OPTIONS"
#define RTSP_METHOD_PAUSE "PAUSE"
#define RTSP_METHOD_PLAY "PLAY"
#define RTSP_METHOD_RECORD "RECORD"
#define RTSP_METHOD_REDIRECT "REDIRECT"
#define RTSP_METHOD_SETUP "SETUP"
#define RTSP_METHOD_SET_PARAMETER "SET_PARAMETER"
#define RTSP_METHOD_TEARDOWN "TEARDOWN"

#define RTSP_CRLF "\r\n"
#define RTSP_HEADER_DIVIDER "\r\n\r\n"

enum E_RTSP_METHOD
{
    E_RTSP_METHOD_DESCRIBE = 0,
    E_RTSP_METHOD_ANNOUNCE = 1,
    E_RTSP_METHOD_GET_PARAMETER = 2,
    E_RTSP_METHOD_OPTIONS = 3,
    E_RTSP_METHOD_PAUSE = 4,
    E_RTSP_METHOD_PLAY = 5,
    E_RTSP_METHOD_RECORD = 6,
    E_RTSP_METHOD_REDIRECT = 7,
    E_RTSP_METHOD_SETUP = 8,
    E_RTSP_METHOD_SET_PARAMETER = 9,
    E_RTSP_METHOD_TEARDOWN = 10,
};

// refer@RFC2326 A.2 Server State Machine
enum E_RTSP_STATE {
    E_RTSP_STATE_INIT = 0,
    E_RTSP_STATE_READY = 1,
    E_RTSP_STATE_PLAYING = 2,
    E_RTSP_STATE_RECORDING = 3
};

// server state
// state           message received  next state
// Init            SETUP             Ready
//                 TEARDOWN          Init
// Ready           PLAY              Playing
//                 SETUP             Ready
//                 TEARDOWN          Init
//                 RECORD            Recording
// Playing         PLAY              Playing
//                 PAUSE             Ready
//                 TEARDOWN          Init
//                 SETUP             Playing
// Recording       RECORD            Recording
//                 PAUSE             Ready
//                 TEARDOWN          Init
//                 SETUP             Recording


#define RTSP_STATUS_CODE_CONTINUE 100      //Continue
#define RTSP_STATUS_CODE_OK 200      //OK
#define RTSP_STATUS_CODE_CREATED 201      //Created
#define RTSP_STATUS_CODE_LOW_ON_STORAGE_SPACE 250      //Low on Storage Space
#define RTSP_STATUS_CODE_MULTIPLE_CHOICES 300      //Multiple Choices
#define RTSP_STATUS_CODE_MOVED_PERMANENTLY 301      //Moved Permanently
#define RTSP_STATUS_CODE_MOVED_TEMPORARILY 302      //Moved Temporarily
#define RTSP_STATUS_CODE_SEE_OTHER 303      //See Other
#define RTSP_STATUS_CODE_NOT_MODIFIED 304      //Not Modified
#define RTSP_STATUS_CODE_USE_PROXY 305      //Use Proxy
#define RTSP_STATUS_CODE_BAD_REQUEST 400      //Bad Request
#define RTSP_STATUS_CODE_UNAUTHORIZED 401      //Unauthorized
#define RTSP_STATUS_CODE_PAYMENT_REQUIRED 402      //Payment Required
#define RTSP_STATUS_CODE_FORBIDDEN 403      //Forbidden
#define RTSP_STATUS_CODE_NOT_FOUND 404      //Not Found
#define RTSP_STATUS_CODE_METHOD_NOT_ALLOWED 405      //Method Not Allowed
#define RTSP_STATUS_CODE_NOT_ACCEPTABLE 406      //Not Acceptable
#define RTSP_STATUS_CODE_PROXY_AUTHENTICATION_REQUIRED 407      //Proxy Authentication Required
#define RTSP_STATUS_CODE_REQUEST_TIMEOUT 408      //Request Time-out
#define RTSP_STATUS_CODE_GONE 410      //Gone
#define RTSP_STATUS_CODE_LENGTH_REQUIRED 411      //Length Required
#define RTSP_STATUS_CODE_PRECONDITION_FAILED 412      //Precondition Failed
#define RTSP_STATUS_CODE_REQUEST_ENTITY_TOO_LARGE 413      //Request Entity Too Large
#define RTSP_STATUS_CODE_REQUEST_URI_TOO_LARGE 414      //Request-URI Too Large
#define RTSP_STATUS_CODE_UNSUPPORTED_MEDIA_TYPE 415      //Unsupported Media Type
#define RTSP_STATUS_CODE_PARAMETER_NOT_UNDERSTOOD 451      //Parameter Not Understood
#define RTSP_STATUS_CODE_CONFERENCE_NOT_FOUND 452      //Conference Not Found
#define RTSP_STATUS_CODE_NOT_ENOUGH_BANDWIDTH 453      //Not Enough Bandwidth
#define RTSP_STATUS_CODE_SESSION_NOT_FOUND 454      //Session Not Found
#define RTSP_STATUS_CODE_MEDHOD_NOT_VALID_IN_THIS_STATE 455      //Method Not Valid in This State
#define RTSP_STATUS_CODE_HADER_FIELD_NOT_VALID_FOR_RESOURCE 456      //Header Field Not Valid for Resource
#define RTSP_STATUS_CODE_INVALID_RANGE 457      //Invalid Range
#define RTSP_STATUS_CODE_PARAMETER_IS_READONLY 458      //Parameter Is Read-Only
#define RTSP_STATUS_CODE_AGGERGATE_OPERATION_NOT_ALLOWED 459      //Aggregate operation not allowed
#define RTSP_STATUS_CODE_ONLY_AGGERGATE_OPERATION_ALLOWED 460      //Only aggregate operation allowed
#define RTSP_STATUS_CODE_UNSUPPORTED_TRANSPORT 461      //Unsupported transport
#define RTSP_STATUS_CODE_DESTINATION_UNREACHABLE 462      //Destination unreachable
#define RTSP_STATUS_CODE_INTERNAL_SERVER_ERROR 500      //Internal Server Error
#define RTSP_STATUS_CODE_NOT_IMPLEMENTED 501      //Not Implemented
#define RTSP_STATUS_CODE_BAD_GATEWAY 502      //Bad Gateway
#define RTSP_STATUS_CODE_SERVICE_UNAVAILABLE 503      //Service Unavailable
#define RTSP_STATUS_CODE_GATEWAY_TIMEOUT 504      //Gateway Time-out
#define RTSP_STATUS_CODE_RTSP_VERSION_NOT_SUPPORTED 505      //RTSP Version not supported
#define RTSP_STATUS_CODE_OPTION_NOT_SUPPORT 551      //Option not supported

using RTSPTransport = struct {
    std::string protocol;//RTP/AVP/TCP  RTP/AVP/UDP

    bool unicast = true;
};
};