#include "transport/ble_nus.hpp"

#include <atomic>
#include <cstdio>
#include <cstring>

#include "esp_log.h"
#include "freertos/queue.h"
#include "host/ble_hs.h"
#include "host/util/util.h"
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "services/gap/ble_svc_gap.h"
#include "services/gatt/ble_svc_gatt.h"

namespace codex_island::transport::ble_nus {
namespace {

constexpr char kTag[] = "ble_nus";
constexpr std::size_t kPacketBufferSize = 520;

const ble_uuid128_t kServiceUuid = BLE_UUID128_INIT(
    0x9e, 0xca, 0xdc, 0x24, 0x0e, 0xe5, 0xa9, 0xe0,
    0x93, 0xf3, 0xa3, 0xb5, 0x01, 0x00, 0x40, 0x6e);
const ble_uuid128_t kRxUuid = BLE_UUID128_INIT(
    0x9e, 0xca, 0xdc, 0x24, 0x0e, 0xe5, 0xa9, 0xe0,
    0x93, 0xf3, 0xa3, 0xb5, 0x02, 0x00, 0x40, 0x6e);
const ble_uuid128_t kTxUuid = BLE_UUID128_INIT(
    0x9e, 0xca, 0xdc, 0x24, 0x0e, 0xe5, 0xa9, 0xe0,
    0x93, 0xf3, 0xa3, 0xb5, 0x03, 0x00, 0x40, 0x6e);

QueueHandle_t g_line_queue = nullptr;
QueueHandle_t g_event_queue = nullptr;
Line g_assembling{};
std::size_t g_assembling_length = 0;
bool g_dropping_overflow = false;
uint16_t g_tx_handle = 0;
uint16_t g_connection_handle = BLE_HS_CONN_HANDLE_NONE;
uint8_t g_address_type = 0;
char g_device_name[32] = "Codex Island";
std::atomic<bool> g_connected{false};
std::atomic<bool> g_subscribed{false};

void start_advertising();

void queue_event(LinkEvent event) {
    if (g_event_queue != nullptr) {
        (void)xQueueSend(g_event_queue, &event, 0);
    }
}

void publish_assembled_line() {
    if (g_assembling_length == 0 || g_line_queue == nullptr) {
        g_assembling_length = 0;
        return;
    }
    g_assembling.data[g_assembling_length] = '\0';
    if (xQueueSend(g_line_queue, &g_assembling, 0) != pdTRUE) {
        ESP_LOGW(kTag, "RX line queue full; dropping one JSON line");
    }
    g_assembling_length = 0;
}

void feed_bytes(const uint8_t *data, std::size_t length) {
    for (std::size_t i = 0; i < length; ++i) {
        const char byte = static_cast<char>(data[i]);
        if (byte == '\n') {
            if (g_dropping_overflow) {
                ESP_LOGE(kTag, "RX JSON line exceeded %u bytes and was dropped",
                         static_cast<unsigned>(kMaxLineLength));
                g_dropping_overflow = false;
                g_assembling_length = 0;
            } else {
                if (g_assembling_length > 0 &&
                    g_assembling.data[g_assembling_length - 1] == '\r') {
                    --g_assembling_length;
                }
                publish_assembled_line();
            }
            continue;
        }
        if (g_dropping_overflow) {
            continue;
        }
        if (g_assembling_length < kMaxLineLength) {
            g_assembling.data[g_assembling_length++] = byte;
        } else {
            g_assembling_length = 0;
            g_dropping_overflow = true;
        }
    }
}

int rx_access(uint16_t, uint16_t, ble_gatt_access_ctxt *context, void *) {
    if (context->op != BLE_GATT_ACCESS_OP_WRITE_CHR) {
        return BLE_ATT_ERR_UNLIKELY;
    }
    const uint16_t packet_length = OS_MBUF_PKTLEN(context->om);
    if (packet_length > kPacketBufferSize) {
        return BLE_ATT_ERR_INVALID_ATTR_VALUE_LEN;
    }
    uint8_t packet[kPacketBufferSize]{};
    if (ble_hs_mbuf_to_flat(context->om, packet, packet_length, nullptr) != 0) {
        return BLE_ATT_ERR_UNLIKELY;
    }
    feed_bytes(packet, packet_length);
    return 0;
}

int tx_access(uint16_t, uint16_t, ble_gatt_access_ctxt *, void *) {
    return 0;
}

const ble_gatt_chr_def kCharacteristics[] = {
    {
        .uuid = &kRxUuid.u,
        .access_cb = rx_access,
        .arg = nullptr,
        .descriptors = nullptr,
        .flags = BLE_GATT_CHR_F_WRITE | BLE_GATT_CHR_F_WRITE_NO_RSP,
        .min_key_size = 0,
        .val_handle = nullptr,
        .cpfd = nullptr,
    },
    {
        .uuid = &kTxUuid.u,
        .access_cb = tx_access,
        .arg = nullptr,
        .descriptors = nullptr,
        .flags = BLE_GATT_CHR_F_NOTIFY,
        .min_key_size = 0,
        .val_handle = &g_tx_handle,
        .cpfd = nullptr,
    },
    {},
};

const ble_gatt_svc_def kServices[] = {
    {
        .type = BLE_GATT_SVC_TYPE_PRIMARY,
        .uuid = &kServiceUuid.u,
        .includes = nullptr,
        .characteristics = kCharacteristics,
    },
    {},
};

int gap_event(ble_gap_event *event, void *) {
    switch (event->type) {
        case BLE_GAP_EVENT_CONNECT:
            if (event->connect.status == 0) {
                g_connection_handle = event->connect.conn_handle;
                g_connected.store(true);
                g_subscribed.store(false);
                queue_event(LinkEvent::kConnected);
                ESP_LOGI(kTag, "central connected; handle=%u", g_connection_handle);
            } else {
                ESP_LOGW(kTag, "connection failed; status=%d", event->connect.status);
                start_advertising();
            }
            break;
        case BLE_GAP_EVENT_DISCONNECT:
            ESP_LOGI(kTag, "central disconnected; reason=%d", event->disconnect.reason);
            g_connection_handle = BLE_HS_CONN_HANDLE_NONE;
            g_connected.store(false);
            g_subscribed.store(false);
            queue_event(LinkEvent::kDisconnected);
            start_advertising();
            break;
        case BLE_GAP_EVENT_SUBSCRIBE:
            if (event->subscribe.attr_handle == g_tx_handle &&
                event->subscribe.cur_notify != 0) {
                g_subscribed.store(true);
                queue_event(LinkEvent::kSubscribed);
                ESP_LOGI(kTag, "central subscribed to NUS TX notifications");
            }
            break;
        case BLE_GAP_EVENT_ADV_COMPLETE:
            start_advertising();
            break;
        case BLE_GAP_EVENT_MTU:
            ESP_LOGI(kTag, "negotiated ATT MTU=%u", event->mtu.value);
            break;
        default:
            break;
    }
    return 0;
}

void start_advertising() {
    ble_hs_adv_fields advertisement{};
    advertisement.flags = BLE_HS_ADV_F_DISC_GEN | BLE_HS_ADV_F_BREDR_UNSUP;
    advertisement.name = reinterpret_cast<uint8_t *>(g_device_name);
    advertisement.name_len = std::strlen(g_device_name);
    advertisement.name_is_complete = 1;
    int result = ble_gap_adv_set_fields(&advertisement);
    if (result != 0) {
        ESP_LOGE(kTag, "could not configure advertisement: %d", result);
        return;
    }

    ble_hs_adv_fields scan_response{};
    scan_response.uuids128 = const_cast<ble_uuid128_t *>(&kServiceUuid);
    scan_response.num_uuids128 = 1;
    scan_response.uuids128_is_complete = 1;
    result = ble_gap_adv_rsp_set_fields(&scan_response);
    if (result != 0) {
        ESP_LOGE(kTag, "could not configure scan response: %d", result);
        return;
    }

    ble_gap_adv_params parameters{};
    parameters.conn_mode = BLE_GAP_CONN_MODE_UND;
    parameters.disc_mode = BLE_GAP_DISC_MODE_GEN;
    result = ble_gap_adv_start(g_address_type, nullptr, BLE_HS_FOREVER,
                               &parameters, gap_event, nullptr);
    if (result != 0 && result != BLE_HS_EALREADY) {
        ESP_LOGE(kTag, "could not start advertising: %d", result);
    }
}

void on_sync() {
    const int result = ble_hs_id_infer_auto(0, &g_address_type);
    if (result != 0) {
        ESP_LOGE(kTag, "could not infer BLE address type: %d", result);
        return;
    }
    start_advertising();
    ESP_LOGI(kTag, "advertising as '%s'", g_device_name);
}

void on_reset(int reason) {
    ESP_LOGE(kTag, "NimBLE host reset; reason=%d", reason);
}

void host_task(void *) {
    nimble_port_run();
    nimble_port_freertos_deinit();
}

}  // namespace

esp_err_t start(const char *device_name) {
    static bool started = false;
    if (started) {
        return ESP_OK;
    }
    started = true;

    if (device_name != nullptr && device_name[0] != '\0') {
        std::snprintf(g_device_name, sizeof(g_device_name), "%s", device_name);
    }
    g_line_queue = xQueueCreate(3, sizeof(Line));
    g_event_queue = xQueueCreate(6, sizeof(LinkEvent));
    if (g_line_queue == nullptr || g_event_queue == nullptr) {
        return ESP_ERR_NO_MEM;
    }

    esp_err_t result = nimble_port_init();
    if (result != ESP_OK) {
        ESP_LOGE(kTag, "nimble_port_init failed: %s", esp_err_to_name(result));
        return result;
    }

    ble_svc_gap_init();
    ble_svc_gatt_init();
    int host_result = ble_gatts_count_cfg(kServices);
    if (host_result == 0) {
        host_result = ble_gatts_add_svcs(kServices);
    }
    if (host_result != 0) {
        ESP_LOGE(kTag, "could not register NUS GATT service: %d", host_result);
        return ESP_FAIL;
    }
    host_result = ble_svc_gap_device_name_set(g_device_name);
    if (host_result != 0) {
        ESP_LOGE(kTag, "could not set GAP name: %d", host_result);
        return ESP_FAIL;
    }

    ble_hs_cfg.sync_cb = on_sync;
    ble_hs_cfg.reset_cb = on_reset;
    nimble_port_freertos_init(host_task);
    return ESP_OK;
}

bool poll_line(Line &line, TickType_t wait_ticks) {
    return g_line_queue != nullptr &&
           xQueueReceive(g_line_queue, &line, wait_ticks) == pdTRUE;
}

bool poll_link_event(LinkEvent &event) {
    return g_event_queue != nullptr &&
           xQueueReceive(g_event_queue, &event, 0) == pdTRUE;
}

bool notify_json_line(const char *json) {
    if (!g_connected.load() || !g_subscribed.load() || json == nullptr) {
        return false;
    }
    char line[256]{};
    const std::size_t length = std::strlen(json);
    if (length == 0 || length + 1 >= sizeof(line)) {
        return false;
    }
    std::memcpy(line, json, length);
    line[length] = '\n';
    os_mbuf *message = ble_hs_mbuf_from_flat(line, length + 1);
    if (message == nullptr) {
        return false;
    }
    return ble_gatts_notify_custom(g_connection_handle, g_tx_handle, message) == 0;
}

bool request_refresh() {
    return notify_json_line("{\"v\":1,\"k\":\"refresh\",\"what\":\"all\"}");
}

bool is_connected() {
    return g_connected.load();
}

}  // namespace codex_island::transport::ble_nus
