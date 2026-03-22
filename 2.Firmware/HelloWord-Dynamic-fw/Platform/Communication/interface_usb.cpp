#include "common_inc.h"
#include "comm/hub_usb_comm.h"
#include "ascii_processor.hpp"
#include "usbd_cdc.h"
#include "usbd_cdc_if.h"
#include "usb_device.h"
#include "interface_usb.hpp"
#include <cstring>

osThreadId_t usbServerTaskHandle;
USBStats_t usb_stats_ = {0};

class USBSender : public PacketSink
{
public:
    USBSender(uint8_t endpoint_pair, const osSemaphoreId &sem_usb_tx)
        : endpoint_pair_(endpoint_pair), sem_usb_tx_(sem_usb_tx)
    {}

    int process_packet(uint8_t *buffer, size_t length) override
    {
        // cannot send partial packets
        if (length > CDC_DATA_MAX_PACKET_SIZE)
            return -1;
        // wait for USB interface to become ready
        if (osSemaphoreAcquire(sem_usb_tx_, PROTOCOL_SERVER_TIMEOUT_MS) != osOK)
        {
            // If the host resets the device it might be that the TX-complete handler is never called
            // and the sem_usb_tx_ semaphore is never released. To handle this we just override the
            // TX buffer if this wait times out. The implication is that the channel is no longer lossless.
            // TODO: handle endpoint reset properly
            usb_stats_.tx_overrun_cnt++;
        }

        // transmit packet
        uint8_t status = CDC_Transmit_FS(const_cast<uint8_t *>(buffer), length, endpoint_pair_);
        if (status != USBD_OK)
        {
            osSemaphoreRelease(sem_usb_tx_);
            return -1;
        }
        usb_stats_.tx_cnt++;

        return 0;
    }

private:
    uint8_t endpoint_pair_;
    const osSemaphoreId &sem_usb_tx_;
};

// Note we could have independent semaphores here to allow concurrent transmission
USBSender usb_packet_output_cdc(CDC_OUT_EP, sem_usb_tx);
USBSender usb_packet_output_native(REF_OUT_EP, sem_usb_tx);

class TreatPacketSinkAsStreamSink : public StreamSink
{
public:
    TreatPacketSinkAsStreamSink(PacketSink &output) : output_(output)
    {}

    int process_bytes(uint8_t *buffer, size_t length, size_t *processed_bytes)
    {
        // Loop to ensure all bytes get sent
        while (length)
        {
            size_t chunk = length < CDC_DATA_MAX_PACKET_SIZE ? length : CDC_DATA_MAX_PACKET_SIZE;
            if (output_.process_packet(buffer, length) != 0)
                return -1;
            buffer += chunk;
            length -= chunk;
            if (processed_bytes)
                *processed_bytes += chunk;
        }
        return 0;
    }

    size_t get_free_space()
    { return SIZE_MAX; }

private:
    PacketSink &output_;
} usb_stream_output(usb_packet_output_cdc);

namespace {

// Host may send one logical [LEN][LEN][CMD][PAYLOAD...] frame across multiple 64-byte USB OUT packets.
// Old logic used (rx_buf[0] < 0x30) to pick "binary" — that rejects valid lengths whose low byte is >= 0x30
// (e.g. PC_HUB_EINK_IMAGE with msgLen=131, first byte 0x83) and never reassembles chunks.
constexpr size_t kHubCdcRxAccumCap = 1024;
constexpr uint16_t kMaxHubCdcBinaryInnerLen = 768; // cmd + payload after the 2-byte LE length

static uint8_t s_hubCdcRxAccum[kHubCdcRxAccumCap];
static size_t s_hubCdcRxAccumLen = 0;

static void HubCdcAppendAndDispatch(const uint8_t* data, uint32_t len)
{
    if (!data || len == 0) {
        return;
    }
    if (s_hubCdcRxAccumLen + len > kHubCdcRxAccumCap) {
        s_hubCdcRxAccumLen = 0; // drop partial frame on overflow
    }
    std::memcpy(s_hubCdcRxAccum + s_hubCdcRxAccumLen, data, len);
    s_hubCdcRxAccumLen += len;

    while (s_hubCdcRxAccumLen >= 3) {
        uint16_t msgLen = (uint16_t)s_hubCdcRxAccum[0] | ((uint16_t)s_hubCdcRxAccum[1] << 8);
        if (msgLen == 0 || msgLen > kMaxHubCdcBinaryInnerLen) {
            // Not a plausible binary frame — legacy ASCII on this endpoint
            ASCII_protocol_parse_stream(s_hubCdcRxAccum, s_hubCdcRxAccumLen, usb_stream_output);
            s_hubCdcRxAccumLen = 0;
            break;
        }
        const size_t need = 2u + (size_t)msgLen;
        if (s_hubCdcRxAccumLen < need) {
            break; // wait for remaining USB OUT chunks
        }
        HubUsb_OnCdcData(s_hubCdcRxAccum, (uint16_t)need);
        const size_t rest = s_hubCdcRxAccumLen - need;
        if (rest > 0) {
            std::memmove(s_hubCdcRxAccum, s_hubCdcRxAccum + need, rest);
        }
        s_hubCdcRxAccumLen = rest;
    }
}

} // namespace

// This is used by the printf feature. Hence the above statics, and below seemingly random ptr (it's externed)
// TODO: less spaghetti code
StreamSink *usb_stream_output_ptr = &usb_stream_output;

BidirectionalPacketBasedChannel usb_channel(usb_packet_output_native);


struct USBInterface
{
    uint8_t *rx_buf = nullptr;
    uint32_t rx_len = 0;
    bool data_pending = false;
    uint8_t out_ep;
    uint8_t in_ep;
    USBSender &usb_sender;
};

// Note: statics make this less modular.
// Note: we use a single rx semaphore and loop over data_pending to allow a single pump loop thread
static USBInterface CDC_interface = {
    .rx_buf = nullptr,
    .rx_len = 0,
    .data_pending = false,
    .out_ep = CDC_OUT_EP,
    .in_ep = CDC_IN_EP,
    .usb_sender = usb_packet_output_cdc,
};
static USBInterface Ref_interface = {
    .rx_buf = nullptr,
    .rx_len = 0,
    .data_pending = false,
    .out_ep = REF_OUT_EP,
    .in_ep = REF_IN_EP,
    .usb_sender = usb_packet_output_native,
};

static void UsbServerTask(void *ctx)
{
    (void) ctx;

    for (;;)
    {
        // const uint32_t usb_check_timeout = 1; // ms
        osStatus sem_stat = osSemaphoreAcquire(sem_usb_rx, osWaitForever);
        if (sem_stat == osOK)
        {
            usb_stats_.rx_cnt++;

            // CDC Interface: new binary protocol or legacy ASCII
            if (CDC_interface.data_pending)
            {
                CDC_interface.data_pending = false;

                HubCdcAppendAndDispatch(CDC_interface.rx_buf, CDC_interface.rx_len);
                USBD_CDC_ReceivePacket(&hUsbDeviceFS, CDC_interface.out_ep);
            }

            // Native Interface
            if (Ref_interface.data_pending)
            {
                Ref_interface.data_pending = false;
                usb_channel.process_packet(Ref_interface.rx_buf, Ref_interface.rx_len);
                USBD_CDC_ReceivePacket(&hUsbDeviceFS, Ref_interface.out_ep);  // Allow next packet
            }
        }
    }
}

// Called from CDC_Receive_FS callback function, this allows the communication
// thread to handle the incoming data
void usb_rx_process_packet(uint8_t *buf, uint32_t len, uint8_t endpoint_pair)
{
    USBInterface *usb_iface;
    if (endpoint_pair == CDC_interface.out_ep)
    {
        usb_iface = &CDC_interface;
    } else if (endpoint_pair == Ref_interface.out_ep)
    {
        usb_iface = &Ref_interface;
    } else
    {
        return;
    }

    // We don't allow the next USB packet until the previous one has been processed completely.
    // Therefore it's safe to write to these vars directly since we know previous processing is complete.
    usb_iface->rx_buf = buf;
    usb_iface->rx_len = len;
    usb_iface->data_pending = true;
    osSemaphoreRelease(sem_usb_rx);
}


const osThreadAttr_t usbServerTask_attributes = {
    .name = "UsbServerTask",
    .stack_size = 512 * 8,
    .priority = (osPriority_t) osPriorityAboveNormal,
};

void StartUsbServer()
{
    // Start USB communication thread
    usbServerTaskHandle = osThreadNew(UsbServerTask, nullptr, &usbServerTask_attributes);
}
