#pragma once

#include "esp_err.h"
#include "esp_event_base.h"
#include "mqtt_client.h"
#include "esp_crt_bundle.h"

namespace MQTT
{
    class MQTT
    {
    public:
        static MQTT &get_instance(){
            static MQTT instance;
            return instance;
        }

        void set_mqtt_cfg(const char *uri)
        {
            m_mqtt_cfg.broker.address.uri = uri;
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

        MQTT() : m_mqtt_cfg{}, m_mqtt_handle{} {}
        ~MQTT() = default;
    };
}