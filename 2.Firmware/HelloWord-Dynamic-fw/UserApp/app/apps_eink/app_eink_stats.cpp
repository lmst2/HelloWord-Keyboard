#include "app_eink_stats.h"
#include "display/eink_canvas.h"

void AppEinkStats::OnPcData(uint8_t feedId, const uint8_t* data, uint8_t len)
{
    if (len < 1) return;

    switch (feedId) {
        case FEED_ID_CPU: cpuPercent_ = data[0]; needsRefresh_ = true; break;
        case FEED_ID_RAM: ramPercent_ = data[0]; needsRefresh_ = true; break;
        case FEED_ID_GPU: gpuPercent_ = data[0]; needsRefresh_ = true; break;
        case FEED_ID_TEMPS:
            if (len >= 2) { cpuTemp_ = (int8_t)data[0]; gpuTemp_ = (int8_t)data[1]; needsRefresh_ = true; }
            break;
        default: break;
    }
}

void AppEinkStats::OnEinkRender(EinkCanvas& canvas)
{
    canvas.Clear(true);
    const int16_t startY = 20;
    const int16_t barX = 40;
    const uint16_t barW = 80;
    const uint16_t barH = 12;
    const int16_t spacing = 45;

    // Title
    canvas.DrawText(30, 5, "PC Stats", true);
    canvas.DrawHLine(0, 14, 128, true);

    // CPU
    canvas.DrawText(2, startY, "CPU", true);
    canvas.DrawLargeNumber(2, startY + 10, cpuPercent_, true);
    canvas.DrawText(40, startY + 13, "%", true);
    canvas.DrawProgressBar(barX, startY + 5, barW, barH, cpuPercent_);
    if (cpuTemp_ != 0) {
        canvas.DrawNumber(barX, startY + 22, cpuTemp_, true);
        canvas.DrawChar(barX + 18, startY + 22, 'C', true);
    }

    // RAM
    int16_t ramY = startY + spacing;
    canvas.DrawText(2, ramY, "RAM", true);
    canvas.DrawLargeNumber(2, ramY + 10, ramPercent_, true);
    canvas.DrawText(40, ramY + 13, "%", true);
    canvas.DrawProgressBar(barX, ramY + 5, barW, barH, ramPercent_);

    // GPU
    int16_t gpuY = startY + spacing * 2;
    canvas.DrawText(2, gpuY, "GPU", true);
    canvas.DrawLargeNumber(2, gpuY + 10, gpuPercent_, true);
    canvas.DrawText(40, gpuY + 13, "%", true);
    canvas.DrawProgressBar(barX, gpuY + 5, barW, barH, gpuPercent_);
    if (gpuTemp_ != 0) {
        canvas.DrawNumber(barX, gpuY + 22, gpuTemp_, true);
        canvas.DrawChar(barX + 18, gpuY + 22, 'C', true);
    }

    needsRefresh_ = false;
}

const char* AppEinkStats::GetSubItemName(uint8_t idx) const
{
    static const char* names[] = {"Bars", "Numbers", "Graph"};
    return idx < 3 ? names[idx] : nullptr;
}

void AppEinkStats::OnSubItemSelected(uint8_t idx)
{
    if (idx < 3) { layoutStyle_ = idx; needsRefresh_ = true; }
}
