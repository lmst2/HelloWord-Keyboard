#ifndef EINK_CANVAS_H
#define EINK_CANVAS_H

#include <stdint.h>
#include <string.h>

// Monochrome framebuffer for 128x296 E-Ink display (1bpp, ~4.7KB)
class EinkCanvas {
public:
    static constexpr uint16_t WIDTH = 128;
    static constexpr uint16_t HEIGHT = 296;
    static constexpr uint32_t BUF_SIZE = WIDTH * HEIGHT / 8;

    void Clear(bool white = true);

    // Pixel ops (x=0..127, y=0..295). black=true draws black pixel.
    void SetPixel(int16_t x, int16_t y, bool black = true);
    bool GetPixel(int16_t x, int16_t y) const;

    // Primitives
    void DrawHLine(int16_t x, int16_t y, uint16_t w, bool black = true);
    void DrawVLine(int16_t x, int16_t y, uint16_t h, bool black = true);
    void DrawLine(int16_t x0, int16_t y0, int16_t x1, int16_t y1, bool black = true);
    void DrawRect(int16_t x, int16_t y, uint16_t w, uint16_t h, bool fill = false, bool black = true);
    void DrawProgressBar(int16_t x, int16_t y, uint16_t w, uint16_t h, uint8_t percent);

    // Bitmap: data is 1bpp packed, MSB first, width must be multiple of 8
    void DrawBitmap(int16_t x, int16_t y, uint16_t w, uint16_t h, const uint8_t* data);

    // Text: simple 5x7 built-in font. Returns advance width.
    uint16_t DrawChar(int16_t x, int16_t y, char c, bool black = true);
    uint16_t DrawText(int16_t x, int16_t y, const char* text, bool black = true);

    // Number rendering
    uint16_t DrawNumber(int16_t x, int16_t y, int32_t num, bool black = true);

    // Large digit (11x16) for stats display
    void DrawLargeDigit(int16_t x, int16_t y, uint8_t digit, bool black = true);
    uint16_t DrawLargeNumber(int16_t x, int16_t y, int32_t num, bool black = true);

    const uint8_t* GetBuffer() const { return buf_; }
    uint8_t* GetMutableBuffer() { return buf_; }

    // Dirty region tracking for partial refresh
    bool HasDirtyRegion() const { return dirty_; }
    void GetDirtyRegion(uint16_t& x, uint16_t& y, uint16_t& w, uint16_t& h) const;
    void ClearDirty() { dirty_ = false; dirtyX0_ = WIDTH; dirtyY0_ = HEIGHT; dirtyX1_ = 0; dirtyY1_ = 0; }

private:
    void MarkDirty(int16_t x, int16_t y);
    void MarkDirtyRect(int16_t x, int16_t y, uint16_t w, uint16_t h);

    uint8_t buf_[BUF_SIZE]{};
    bool dirty_ = false;
    uint16_t dirtyX0_ = WIDTH, dirtyY0_ = HEIGHT;
    uint16_t dirtyX1_ = 0, dirtyY1_ = 0;
};

#endif // EINK_CANVAS_H
