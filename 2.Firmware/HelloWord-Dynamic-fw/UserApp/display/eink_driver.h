#ifndef EINK_DRIVER_H
#define EINK_DRIVER_H

#include <stdint.h>
#include "eink_290_bw.h"

class EinkDriver {
public:
    void Init(Eink290BW* hw);

    void FullRefresh(const uint8_t* framebuffer);
    void PartialRefresh(const uint8_t* framebuffer);
    bool IsBusy() const { return busy_; }

    static constexpr uint16_t WIDTH = 128;
    static constexpr uint16_t HEIGHT = 296;
    static constexpr uint32_t BUF_SIZE = WIDTH * HEIGHT / 8;

private:
    Eink290BW* hw_ = nullptr;
    bool busy_ = false;
    bool initialized_ = false;
};

extern EinkDriver einkDriver;

#endif // EINK_DRIVER_H
