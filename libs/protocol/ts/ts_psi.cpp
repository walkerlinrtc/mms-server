/*
 * @Author: jbl19860422
 * @Date: 2023-09-16 10:32:17
 * @LastEditTime: 2023-09-16 12:06:47
 * @LastEditors: jbl19860422
 * @Description: 
 * @FilePath: \mms\mms\protocol\ts\ts_psi.cpp
 * Copyright (c) 2023 by jbl19860422@gitee.com, All Rights Reserved. 
 */
#include "ts_psi.hpp"
using namespace mms;
int32_t TsPayloadPSI::encode(uint8_t *data, int32_t len) {
    ((void)data);
    ((void)len);
    return 0;
}

int32_t TsPayloadPSI::decode(uint8_t *data, int32_t len) {
    ((void)data);
    ((void)len);
    return 0;
}

int32_t TsPayloadPSI::size() {
    return 0;
}