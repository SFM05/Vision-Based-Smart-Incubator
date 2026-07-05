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
        static MQTT &instance()
        {
            static MQTT m_instance;
            return m_instance;
        }

        void set_mqtt_cfg(const char *uri, const char *username, const char *password)
        {
            m_mqtt_cfg.broker.address.uri = uri;
            m_mqtt_cfg.credentials.username = username;
            m_mqtt_cfg.credentials.authentication.password = password;
            m_mqtt_cfg.broker.verification.crt_bundle_attach = esp_crt_bundle_attach;
        }
        esp_err_t mqtt_init(esp_event_handler_t handler, void *handler_args);

        const esp_mqtt_client_handle_t &mqtt_handle() const { return m_mqtt_handle; }

    private:
        esp_mqtt_client_config_t m_mqtt_cfg;
        esp_mqtt_client_handle_t m_mqtt_handle;

        MQTT() : m_mqtt_cfg{}, m_mqtt_handle{} {}
        ~MQTT() = default;
    };
}