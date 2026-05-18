#include "mqtt.hpp"
#include "esp_err.h"
#include "esp_log.h"
#include "mqtt_client.h"

static const char *TAG = "MQTT";

namespace MQTT
{
    esp_err_t MQTT::mqtt_init()
    {

        m_mqtt_handle = esp_mqtt_client_init(&m_mqtt_cfg);
        if (m_mqtt_handle == nullptr) [[unlikely]]
        {
            ESP_LOGE(TAG, "MQTT client create failed.");
            return ESP_FAIL;
        }

        esp_err_t err = esp_mqtt_client_register_event(m_mqtt_handle,
                                                       esp_mqtt_event_id_t::MQTT_EVENT_ANY,
                                                       mqtt_event_handler,
                                                       nullptr);
        if (err != ESP_OK) [[unlikely]]
        {
            ESP_LOGE(TAG, "Event register failed.");
            return err;
        }

        err = esp_mqtt_client_start(m_mqtt_handle);
        if (err != ESP_OK) [[unlikely]]
        {
            ESP_LOGE(TAG, "Start MQTT client failed.");
            return err;
        }

        err = esp_event_loop_create_default();
        if (err == ESP_ERR_INVALID_STATE) [[unlikely]]
        {
            ESP_LOGW(TAG, "Event loop has been created before.");
            return ESP_ERR_INVALID_STATE;
        }
        if (err != ESP_OK) [[unlikely]]
        {
            ESP_LOGE(TAG, "Event loop create failed.");
            return err;
        }

        return ESP_OK;
    }

    void MQTT::mqtt_event_handler(void *handler_args,
                                  esp_event_base_t base,
                                  int32_t event_id,
                                  void *event_data)
    {
        ESP_LOGD(TAG, "Event dispatched from event loop base=%s, event_id=%" PRIi32, base, event_id);
        esp_mqtt_event_handle_t event = static_cast<esp_mqtt_event_handle_t>(event_data);
        esp_mqtt_client_handle_t client = event->client;
        int msg_id;

        switch (static_cast<esp_mqtt_event_id_t>(event_id))
        {
        case MQTT_EVENT_CONNECTED:
            ESP_LOGI(TAG, "MQTT_EVENT_CONNECTED");
            msg_id = esp_mqtt_client_subscribe(client, "topic/qos0", 0);
            ESP_LOGI(TAG, "sent subscribe successful, msg_id=%d", msg_id);

            msg_id = esp_mqtt_client_subscribe(client, "topic/qos1", 1);
            ESP_LOGI(TAG, "sent subscribe successful, msg_id=%d", msg_id);

            msg_id = esp_mqtt_client_unsubscribe(client, "topic/qos1");
            ESP_LOGI(TAG, "sent unsubscribe successful, msg_id=%d", msg_id);
            break;
        case MQTT_EVENT_DISCONNECTED:
            ESP_LOGI(TAG, "MQTT_EVENT_DISCONNECTED");
            break;

        case MQTT_EVENT_SUBSCRIBED:
            ESP_LOGI(TAG, "MQTT_EVENT_SUBSCRIBED, msg_id=%d, return code=0x%02x ", event->msg_id, (uint8_t)*event->data);
            msg_id = esp_mqtt_client_publish(client, "topic/qos0", "data", 0, 0, 0);
            ESP_LOGI(TAG, "sent publish successful, msg_id=%d", msg_id);
            break;
        case MQTT_EVENT_UNSUBSCRIBED:
            ESP_LOGI(TAG, "MQTT_EVENT_UNSUBSCRIBED, msg_id=%d", event->msg_id);
            break;
        case MQTT_EVENT_PUBLISHED:
            ESP_LOGI(TAG, "MQTT_EVENT_PUBLISHED, msg_id=%d", event->msg_id);
            break;
        case MQTT_EVENT_DATA:
            ESP_LOGI(TAG, "MQTT_EVENT_DATA");
            printf("TOPIC=%.*s\r\n", event->topic_len, event->topic);
            printf("DATA=%.*s\r\n", event->data_len, event->data);
            break;
        case MQTT_EVENT_ERROR:
            ESP_LOGI(TAG, "MQTT_EVENT_ERROR");
            if (event->error_handle->error_type == MQTT_ERROR_TYPE_TCP_TRANSPORT)
            {
                ESP_LOGI(TAG, "Last error code reported from esp-tls: 0x%x", event->error_handle->esp_tls_last_esp_err);
                ESP_LOGI(TAG, "Last tls stack error number: 0x%x", event->error_handle->esp_tls_stack_err);
                ESP_LOGI(TAG, "Last captured errno : %d (%s)", event->error_handle->esp_transport_sock_errno,
                         strerror(event->error_handle->esp_transport_sock_errno));
            }
            else if (event->error_handle->error_type == MQTT_ERROR_TYPE_CONNECTION_REFUSED)
            {
                ESP_LOGI(TAG, "Connection refused error: 0x%x", event->error_handle->connect_return_code);
            }
            else
            {
                ESP_LOGW(TAG, "Unknown error type: 0x%x", event->error_handle->error_type);
            }
            break;
        default:
            ESP_LOGI(TAG, "Other event id:%d", event->event_id);
            break;
        }
    }
}