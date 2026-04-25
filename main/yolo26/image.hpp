#include <memory>
#include "dl_image.hpp"

class Image{
private:
    uint16_t m_width;
    uint16_t m_height;
    dl::image::pix_type_t m_pix_type;
    std::shared_ptr<uint8_t[]> m_data;

    void datach();

public:
    Image(uint16_t width,
          uint16_t height,
          dl::image::pix_type_t pix_type = dl::image::DL_IMAGE_PIX_TYPE_RGB888);
    Image(const dl::image::img_t &img);
    Image(const Image &) = default;
    Image(Image &&) = default;

    ~Image() = default;

    Image &operator=(const Image &) = default;
    Image &operator=(Image &&) = default;

    uint16_t &width();
    uint16_t &height();
    dl::image::pix_type_t &pix_type();
    // std::shared_ptr<void> &data();
    // void *raw_data();

    const dl::image::img_t to_img_t_const();
    dl::image::img_t to_img_t();

    void reset();
};