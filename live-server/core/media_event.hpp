/*
 * @Author: jbl19860422
 * @Date: 2023-12-22 22:37:55
 * @LastEditTime: 2023-12-22 22:38:00
 * @LastEditors: jbl19860422
 * @Description: 
 * @FilePath: \mms\mms\core\media_event.hpp
 * Copyright (c) 2023 by jbl19860422@gitee.com, All Rights Reserved. 
 */
#pragma once
namespace mms {
enum E_MEDIA_EVENT_CODE {
    E_MEDIA_EVENT_FOUND,
    E_MEDIA_EVENT_STREAM_END,
    E_MEDIA_EVENT_UNAUTH,
    E_MEDIA_EVENT_NOT_FOUND,
    E_MEDIA_EVENT_TIMEOUT,
    E_MEDIA_EVENT_CONN_FAIL,
};

class MediaEvent {
public:
    MediaEvent(E_MEDIA_EVENT_CODE code);
    virtual ~MediaEvent();
public:
    E_MEDIA_EVENT_CODE code_;
};
};