#include "LilygoT5S3.h"
#include <Renderer/EpdiyRenderer.h>
#include <regular_font.h>
#include <bold_font.h>
#include <italic_font.h>
#include <bold_italic_font.h>
#include <hourglass.h>
#include "controls/ButtonControls.h"
#include "controls/PaperS3TouchControls.h"
#include <esp_sleep.h>

// Simple no-op button controls for boards without navigation buttons
class NoButtonControls : public ButtonControls
{
public:
  bool did_wake_from_deep_sleep() override
  {
    // On Paper S3, we use deep sleep as a low-power "screen off" and
    // want to resume into the previous reading session rather than
    // treating every wake as a cold boot. Consider any non-undefined
    // wakeup cause as a deep-sleep resume.
    return esp_sleep_get_wakeup_cause() != ESP_SLEEP_WAKEUP_UNDEFINED;
  }
  UIAction get_deep_sleep_action() override { return UIAction::NONE; }
  void setup_deep_sleep() override {}
};

void LilygoT5S3::power_up()
{
  // Need to power on the EDP to get power to the SD Card
  epd_poweron();
}

void LilygoT5S3::prepare_to_sleep()
{
  // No special handling yet; deep sleep is managed in main.cpp
}

Renderer *LilygoT5S3::get_renderer()
{
  return new LilygoT5S3Renderer();
}

ButtonControls *LilygoT5S3::get_button_controls(QueueHandle_t ui_queue)
{
  (void)ui_queue;
  // PaperS3 has no dedicated navigation buttons; all navigation will
  // be via touch, so return a no-op ButtonControls implementation.
  return new NoButtonControls();
}

TouchControls *LilygoT5S3::get_touch_controls(Renderer *renderer, QueueHandle_t ui_queue)
{
  (void)ui_queue;
#if defined(BOARD_TYPE_LILYGO_T5_47_S3)
  return new LilygoT5S3();
#else
  return new PaperS3();
#endif
}
