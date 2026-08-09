#pragma once

/**
 * LilygoT5S3Renderer - Renderer for the diy-esp32s3-epub-reader on the
 * LilyGo T5 4.7" ESP32-S3 (v2.3), using LilyGo's own EPD driver.
 *
 * WHY THIS EXISTS
 * ---------------
 * epdiy cannot drive this board on the S3: it selects RENDER_METHOD_LCD for
 * ESP32-S3 and binds LEH to the LCD peripheral's hardware HSYNC signal, but on
 * this board latch-enable and STV live inside the 74HCT4094 shift register,
 * not on GPIOs. So we keep LilyGo's driver (which already works) and do the
 * portrait rotation here instead of via epd_set_rotation().
 *
 * The reader's FreeType rasteriser emits every glyph pixel through
 * Renderer::draw_pixel(), so rotating that one function rotates all text.
 *
 * LOGICAL COORDINATE SPACE
 * ------------------------
 * The reader already assumes a portrait page. Here that is 540 x 960:
 *   get_page_width()  -> EPD_HEIGHT - horizontal margins
 *   get_page_height() -> EPD_WIDTH  - vertical margins
 * The panel underneath stays 960 x 540; nothing above this class knows that.
 */

#include <esp_log.h>
#include <esp_heap_caps.h>
#include <string.h>
#include <math.h>

#include "epd_driver.h"          // LilyGo library, NOT epdiy
#include "Renderer.h"

#ifdef USE_FREETYPE
#include "FreeTypeFont.h"
#endif

#define GAMMA_VALUE (1.0f / 0.8f)

/* Flip to ROTATE_CCW if the page comes out upside down. Rotating the
   physical device is not equivalent - the panel's long edge is the
   scan direction, so pick the one that matches your enclosure. */
#define ROTATE_CW

class LilygoT5S3Renderer : public Renderer
{
protected:
    uint8_t *m_frame_buffer = nullptr;
    uint8_t gamma_curve[256] = {0};
    bool needs_gray_flush = false;

#ifdef USE_FREETYPE
    FreeTypeFont *m_freetype_font = nullptr;
    bool m_freetype_enabled = false;
#endif

    /* ---- portrait -> landscape ---------------------------------------- */
    static inline void rotate(int px, int py, int &lx, int &ly)
    {
#ifdef ROTATE_CW
        lx = (EPD_WIDTH - 1) - py;
        ly = px;
#else
        lx = py;
        ly = (EPD_HEIGHT - 1) - px;
#endif
    }

    /* Portrait rect -> landscape Rect_t, with x/width snapped to 4px so the
       partial-refresh row copy stays byte aligned. */
    static Rect_t rotate_rect(int x, int y, int w, int h)
    {
        Rect_t r;
#ifdef ROTATE_CW
        r.x = EPD_WIDTH - y - h;
        r.y = x;
#else
        r.x = y;
        r.y = EPD_HEIGHT - x - w;
#endif
        r.width = h;
        r.height = w;

        int x0 = r.x & ~3;
        int x1 = (r.x + r.width + 3) & ~3;
        if (x0 < 0) x0 = 0;
        if (x1 > EPD_WIDTH) x1 = EPD_WIDTH;
        r.x = x0;
        r.width = x1 - x0;

        if (r.y < 0) { r.height += r.y; r.y = 0; }
        if (r.y + r.height > EPD_HEIGHT) r.height = EPD_HEIGHT - r.y;
        return r;
    }

public:
    LilygoT5S3Renderer()
    {
        for (int g = 0; g < 256; g++)
            gamma_curve[g] = (uint8_t)round(255 * pow(g / 255.0, GAMMA_VALUE));

        m_frame_buffer = (uint8_t *)heap_caps_malloc(
            EPD_WIDTH * EPD_HEIGHT / 2, MALLOC_CAP_SPIRAM);
        if (!m_frame_buffer) {
            ESP_LOGE("EPD", "framebuffer alloc failed");
            return;
        }
        memset(m_frame_buffer, 0xFF, EPD_WIDTH * EPD_HEIGHT / 2);

        epd_init();
    }

    virtual ~LilygoT5S3Renderer()
    {
        if (m_frame_buffer) heap_caps_free(m_frame_buffer);
    }

    /* ---- the one function that matters --------------------------------- */

    virtual void draw_pixel(int x, int y, uint8_t color) override
    {
        uint8_t c = gamma_curve[color];
        needs_gray(c);
        int lx, ly;
        rotate(x + margin_left, y + margin_top, lx, ly);
        epd_draw_pixel(lx, ly, c, m_frame_buffer);
    }

    /* ---- geometry ------------------------------------------------------ */

    virtual void fill_rect(int x, int y, int width, int height, uint8_t color = 0) override
    {
        needs_gray(color);
        Rect_t r = rotate_rect(x + margin_left, y + margin_top, width, height);
        epd_fill_rect(r.x, r.y, r.width, r.height, color, m_frame_buffer);
    }

    virtual void draw_rect(int x, int y, int width, int height, uint8_t color = 0) override
    {
        needs_gray(color);
        Rect_t r = rotate_rect(x + margin_left, y + margin_top, width, height);
        epd_draw_rect(r.x, r.y, r.width, r.height, color, m_frame_buffer);
    }

    virtual void draw_circle(int x, int y, int r, uint8_t color = 0) override
    {
        needs_gray(color);
        int lx, ly;
        rotate(x + margin_left, y + margin_top, lx, ly);
        epd_draw_circle(lx, ly, r, color, m_frame_buffer);
    }

    virtual void fill_circle(int x, int y, int r, uint8_t color = 0) override
    {
        needs_gray(color);
        int lx, ly;
        rotate(x + margin_left, y + margin_top, lx, ly);
        epd_fill_circle(lx, ly, r, color, m_frame_buffer);
    }

    virtual void draw_triangle(int x0, int y0, int x1, int y1,
                               int x2, int y2, uint8_t color) override
    {
        needs_gray(color);
        int a0, b0, a1, b1, a2, b2;
        rotate(x0 + margin_left, y0 + margin_top, a0, b0);
        rotate(x1 + margin_left, y1 + margin_top, a1, b1);
        rotate(x2 + margin_left, y2 + margin_top, a2, b2);
        epd_draw_triangle(a0, b0, a1, b1, a2, b2, color, m_frame_buffer);
    }

    virtual void fill_triangle(int x0, int y0, int x1, int y1,
                               int x2, int y2, uint8_t color) override
    {
        needs_gray(color);
        int a0, b0, a1, b1, a2, b2;
        rotate(x0 + margin_left, y0 + margin_top, a0, b0);
        rotate(x1 + margin_left, y1 + margin_top, a1, b1);
        rotate(x2 + margin_left, y2 + margin_top, a2, b2);
        epd_fill_triangle(a0, b0, a1, b1, a2, b2, color, m_frame_buffer);
    }

    /* ---- text (FreeType only) ------------------------------------------ */

    virtual void draw_text(int x, int y, const char *text,
                           bool bold = false, bool italic = false) override
    {
#ifdef USE_FREETYPE
        if (m_freetype_enabled && m_freetype_font && m_freetype_font->is_valid()) {
            /* goes through draw_pixel(), so it rotates for free */
            m_freetype_font->draw_text(this, x, y, text);
            return;
        }
#endif
        /* No bitmap fallback: LilyGo's write_string() writes directly into
           the framebuffer at landscape stride and would come out sideways. */
        ESP_LOGW("EPD", "draw_text called without FreeType - text skipped");
    }

    virtual int get_text_width(const char *text,
                               bool bold = false, bool italic = false) override
    {
#ifdef USE_FREETYPE
        if (m_freetype_enabled && m_freetype_font && m_freetype_font->is_valid())
            return m_freetype_font->get_text_width(text);
#endif
        return 0;
    }

    virtual int get_line_height() override
    {
#ifdef USE_FREETYPE
        if (m_freetype_enabled && m_freetype_font && m_freetype_font->is_valid())
            return m_freetype_font->get_line_height();
#endif
        return 24;
    }

    virtual int get_space_width() override
    {
#ifdef USE_FREETYPE
        if (m_freetype_enabled && m_freetype_font && m_freetype_font->is_valid())
            return m_freetype_font->get_text_width(" ");
#endif
        return 8;
    }

#ifdef USE_FREETYPE
    virtual void set_freetype_font_for_reading(FreeTypeFont *font) override
    {
        m_freetype_font = font;
    }
    virtual void set_freetype_enabled(bool enabled) override
    {
        m_freetype_enabled = enabled && m_freetype_font && m_freetype_font->is_valid();
    }
    virtual int get_reading_font_pixel_height() const override
    {
        return m_freetype_font ? m_freetype_font->get_pixel_height() : 0;
    }
    virtual bool set_reading_font_pixel_height(int pixel_height) override
    {
        return m_freetype_font ? m_freetype_font->set_pixel_height(pixel_height) : false;
    }
#endif

    /* ---- images -------------------------------------------------------- */

    /* img_buffer is 4bpp, stride = width/2 rounded up */
    virtual void show_img(int x, int y, int width, int height,
                          const uint8_t *img_buffer) override
    {
        int stride = width / 2 + width % 2;
        for (int sy = 0; sy < height; sy++) {
            const uint8_t *row = img_buffer + (size_t)sy * stride;
            for (int sx = 0; sx < width; sx++) {
                uint8_t b = row[sx / 2];
                uint8_t v = (sx % 2) ? (b >> 4) : (b & 0x0F);
                int lx, ly;
                rotate(x + margin_left + sx, y + margin_top + sy, lx, ly);
                epd_draw_pixel(lx, ly, (uint8_t)(v << 4), m_frame_buffer);
            }
        }
        needs_gray_flush = true;
    }

    virtual void show_busy() override
    {
        /* Simple centred marker; swap for your busy icon if you have one. */
        int cx = get_page_width() / 2;
        int cy = get_page_height() / 2;
        fill_circle(cx, cy, 20, 0);
        flush_area(cx - 30, cy - 30, 60, 60);
    }

    /* ---- grayscale tracking -------------------------------------------- */

    virtual void needs_gray(uint8_t color) override
    {
        if (color != 0 && color != 255) needs_gray_flush = true;
    }
    virtual bool has_gray() override { return needs_gray_flush; }

    /* ---- page geometry ------------------------------------------------- */

    virtual int get_page_width() override
    {
        return EPD_HEIGHT - (margin_left + margin_right);   /* 540 */
    }
    virtual int get_page_height() override
    {
        return EPD_WIDTH - (margin_top + margin_bottom);    /* 960 */
    }

    /* ---- output -------------------------------------------------------- */

    virtual void clear_screen() override
    {
        memset(m_frame_buffer, 0xFF, EPD_WIDTH * EPD_HEIGHT / 2);
        needs_gray_flush = false;
    }

    virtual void flush_display() override
    {
        epd_poweron();
        epd_clear();
        epd_draw_grayscale_image(epd_full_screen(), m_frame_buffer);
        epd_poweroff();
        needs_gray_flush = false;
    }

    /**
     * Partial refresh. epd_draw_grayscale_image() wants an AREA-SIZED buffer,
     * not a window into the full framebuffer (provide_out() advances the data
     * pointer by area.width/2 per row), so copy the rows out first.
     */
    virtual void flush_area(int x, int y, int width, int height) override
    {
        Rect_t r = rotate_rect(x + margin_left, y + margin_top, width, height);
        if (r.width <= 0 || r.height <= 0) return;

        int stride = r.width / 2;
        uint8_t *tmp = (uint8_t *)heap_caps_malloc((size_t)stride * r.height,
                                                   MALLOC_CAP_SPIRAM);
        if (!tmp) return;

        for (int row = 0; row < r.height; row++)
            memcpy(tmp + (size_t)row * stride,
                   m_frame_buffer + (size_t)(r.y + row) * (EPD_WIDTH / 2) + r.x / 2,
                   stride);

        epd_poweron();
        epd_clear_area(r);
        epd_draw_grayscale_image(r, tmp);
        epd_poweroff();

        heap_caps_free(tmp);
    }
};
