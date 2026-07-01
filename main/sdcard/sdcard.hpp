#include "esp_err.h"
#include "esp_ldo_regulator.h"
#include "sd_protocol_types.h"

namespace sdcard
{
    class SDcard
    {
    public:
        static SDcard &instance()
        {
            static SDcard m_instance;
            return m_instance;
        }
        esp_err_t init();
        void deinit();

    private:
        sdmmc_card_t *m_card;
        esp_ldo_channel_handle_t m_ldo_chan_handle;

        SDcard() : m_card{}, m_ldo_chan_handle{} {}
        ~SDcard() = default;
    };
}