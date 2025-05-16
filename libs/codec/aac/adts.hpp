#pragma once
namespace mms {
enum AdtsAacProfile
{
    AdtsAacProfileReserved = 3,
    
    // @see 7.1 Profiles, ISO_IEC_13818-7-AAC-2004.pdf, page 40
    AdtsAacProfileMain = 0,
    AdtsAacProfileLC = 1,
    AdtsAacProfileSSR = 2,
};
};
