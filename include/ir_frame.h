#pragma once
#include <stdint.h>

namespace irlink {
struct Pulse { uint16_t mark,space; };
static constexpr unsigned FRAME_ITEMS=34;
inline void necFrame(uint32_t raw,Pulse (&out)[FRAME_ITEMS]) {
    out[0]={8960,4480};
    for (unsigned i=0;i<32;++i) out[i+1]={560,uint16_t(raw&(1u<<(31-i)) ? 1680 : 560)};
    // The hardware carrier is off during spaces. Keep both RMT durations below 32768 us.
    out[33]={560,20000};
}
}
