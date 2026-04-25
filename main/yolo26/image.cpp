#include <memory>
#include "dl_image.hpp"
#include "image.hpp"

auto deleter = [](uint8_t *p)
{ heap_caps_free(static_cast<void *>(p)); };

Image::Image(uint16_t width,
             uint16_t height,
             dl::image::pix_type_t pix_type) : m_width{width}, m_height{height}, m_pix_type{pix_type}
{
    size_t size = width * height * dl::image::get_pix_byte_size(pix_type);
    uint8_t *data = static_cast<uint8_t *>(heap_caps_malloc(size, MALLOC_CAP_DEFAULT));
    m_data = std::shared_ptr<uint8_t[]>(data, deleter);
}

Image::Image(const dl::image::img_t &img) : m_width{img.width}, m_height{img.height}, m_pix_type{img.pix_type}
{
    m_data = std::shared_ptr<uint8_t[]>(static_cast<uint8_t *>(img.data), deleter);
}

void Image::datach()
{
    if (!m_data.unique())
    {
        size_t size = m_width * m_height * dl::image::get_pix_byte_size(m_pix_type);
        uint8_t *new_data = static_cast<uint8_t *>(heap_caps_malloc(size, MALLOC_CAP_DEFAULT));
        std::copy(m_data.get(), m_data.get() + size, new_data);
        m_data = std::shared_ptr<uint8_t[]>(new_data, deleter);
    }
}

uint16_t &Image::width(){
    return m_width;
}
uint16_t &Image::height(){
    return m_height;
}
dl::image::pix_type_t &Image::pix_type(){
    return m_pix_type;
}

const dl::image::img_t Image::to_img_t_const()
{
    dl::image::img_t img{
        .data = m_data.get(),
        .width = m_width,
        .height = m_height,
        .pix_type = m_pix_type};
    return img;
}

dl::image::img_t Image::to_img_t()
{
    datach();
    dl::image::img_t img{
        .data = m_data.get(),
        .width = m_width,
        .height = m_height,
        .pix_type = m_pix_type};
    return img;
}

void Image::reset()
{
    m_data.reset();
}
