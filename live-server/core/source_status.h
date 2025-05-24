#pragma once
namespace mms {
    enum SourceStatus {
        E_SOURCE_STATUS_INIT                =   0,
        E_SOURCE_STATUS_OK                  =   200,
        E_SOURCE_STATUS_GONE                =   410,
        E_SOURCE_STATUS_UNAUTHORIZED        =   401,
        E_SOURCE_STATUS_FORBIDDEN           =   403,
        E_SOURCE_STATUS_NOT_FOUND           =   404,
        E_SOURCE_STATUS_TIMEOUT             =   504,
        E_SOURCE_STATUS_CONN_FAIL           =   500,
    };
}