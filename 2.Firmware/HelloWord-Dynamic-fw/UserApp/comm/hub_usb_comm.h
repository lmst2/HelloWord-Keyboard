#ifndef HUB_USB_COMM_H
#define HUB_USB_COMM_H

#include <stdint.h>

class HubUsbComm {
public:
    void OnCdcData(const uint8_t* data, uint16_t len);
    bool SendCdc(const uint8_t* data, uint16_t len);

    // Helper to build and send a length-prefixed response
    bool SendResponse(const uint8_t* resp, uint16_t respLen);

private:
    void HandleCommand(const uint8_t* data, uint16_t len);
    void HandleConfigGet(const uint8_t* payload, uint8_t len);
    void HandleConfigSet(const uint8_t* payload, uint8_t len);
    void HandleConfigGetAll(const uint8_t* payload, uint8_t len);
    void HandleStatusReq(const uint8_t* payload, uint8_t len);
    void HandleDataFeed(const uint8_t* payload, uint8_t len);
    void HandleEinkImage(const uint8_t* payload, uint16_t len);
    void HandleEinkText(const uint8_t* payload, uint8_t len);
    void HandleFwInfoReq();
    void HandleProfileList();
    void HandleProfileSave(const uint8_t* payload, uint8_t len);
    void HandleProfileLoad(const uint8_t* payload, uint8_t len);
    void HandleProfileDelete(const uint8_t* payload, uint8_t len);
    void HandleAppSwitch(const uint8_t* payload, uint8_t len);
    void HandleEinkSwitch(const uint8_t* payload, uint8_t len);
};

#ifdef __cplusplus
extern "C" {
#endif
void HubUsb_OnCdcData(const uint8_t* data, uint16_t len);
#ifdef __cplusplus
}
#endif

extern HubUsbComm hubUsb;

#endif // HUB_USB_COMM_H
