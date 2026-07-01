#include "esp_err.h"
#include "esp_ldo_regulator.h"
#include "sd_protocol_types.h"
#include "soc/gpio_num.h"

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
        esp_err_t init(gpio_num_t clk,
                       gpio_num_t cmd,
                       gpio_num_t d0,
                       gpio_num_t d1,
                       gpio_num_t d2,
                       gpio_num_t d3);
        void deinit();

    private:
        sdmmc_card_t *m_card;
        esp_ldo_channel_handle_t m_ldo_chan_handle;

        SDcard() : m_card{}, m_ldo_chan_handle{} {}
        ~SDcard() { deinit(); }
    };
}