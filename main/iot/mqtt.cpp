#include "mqtt.hpp"
#include "esp_err.h"
#include "esp_log.h"
#include "mqtt_client.h"

static const char *TAG = "MQTT";

namespace MQTT
{
    esp_err_t MQTT::mqtt_init(esp_event_handler_t handler, void *handler_args)
    {
        esp_err_t err = esp_event_loop_create_default();
        if (err == ESP_ERR_INVALID_STATE)
        {
            ESP_LOGW(TAG, "Event loop already exists, continuing.");
        }
        else if (err != ESP_OK) [[unlikely]]
        {
            ESP_LOGE(TAG, "Event loop create failed.");
            return err;
        }

        m_mqtt_handle = esp_mqtt_client_init(&m_mqtt_cfg);
        if (m_mqtt_handle == nullptr) [[unlikely]]
        {
            ESP_LOGE(TAG, "MQTT client create failed.");
            return ESP_FAIL;
        }

        err = esp_mqtt_client_register_event(m_mqtt_handle,
                                             esp_mqtt_event_id_t::MQTT_EVENT_ANY,
                                             handler,
                                             handler_args);
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

        return ESP_OK;
    }
}