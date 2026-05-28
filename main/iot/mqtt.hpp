#pragma once

#include "esp_crt_bundle.h"
#include "esp_err.h"
#include "esp_event_base.h"
#include "mqtt_client.h"

namespace MQTT
{
    class MQTT
    {
    public:
        QueueHandle_t m_upload_queue;

        static MQTT &get_instance()
        {
            static MQTT instance;
            return instance;
        }

        void set_mqtt_cfg(const char *uri, const char *username, const char *password)
        {
            m_mqtt_cfg.broker.address.uri = uri;
            m_mqtt_cfg.credentials.username = username;
            m_mqtt_cfg.credentials.authentication.password = password;
            m_mqtt_cfg.broker.verification.crt_bundle_attach = esp_crt_bundle_attach;
        }
        esp_err_t mqtt_init();

        const esp_mqtt_client_handle_t &mqtt_handle() const { return m_mqtt_handle; }

        static void mqtt_event_handler(void *handler_args,
                                       esp_event_base_t base,
                                       int32_t event_id,
                                       void *event_data);

    private:
        esp_mqtt_client_config_t m_mqtt_cfg;
        esp_mqtt_client_handle_t m_mqtt_handle;

        MQTT() : m_mqtt_cfg{}, m_mqtt_handle{}
        {
            m_upload_queue = xQueueCreate(
                5,                // 队列深度：最多缓存 5 条待处理 URL
                sizeof(char[512]) // 每条消息大小：512 字节的字符串
            );
        }
        ~MQTT() = default;
    };
}