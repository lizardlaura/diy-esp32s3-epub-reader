#include "LilygoT5S3.h"
#include <Renderer/LilygoT5S3Renderer.h>
#include "controls/ButtonControls.h"
#include "controls/TouchControls.h"
#include "src/epd_driver.h"
#include <esp_sleep.h>

// The T5-S3 has one usable button (GPIO21) plus BOOT on GPIO0, which is
// shared with the config shift register strobe. GPIOButtonControls needs
// three pins, so use a no-op implementation until extra buttons are wired
// to the free pins (45, 10, 48, 39).
class NoButtonControls : public ButtonControls
{
public:
  bool did_wake_from_deep_sleep() override
  {
    return esp_sleep_get_wakeup_cause() != ESP_SLEEP_WAKEUP_UNDEFINED;
  }
  UIAction get_deep_sleep_action() override { return UIAction::NONE; }
  void setup_deep_sleep() override {}
};

void LilygoT5S3::power_up()
{
  // Must power on the EPD to get power to the SD card - the SD slot sits
  // downstream of the display power rail, so start_filesystem() will fail
  // to mount if this hasn't run first.
  epd_poweron();
}

void LilygoT5S3::prepare_to_sleep()
{
  epd_poweroff();
}

Renderer *LilygoT5S3::get_renderer()
{
  return new LilygoT5S3Renderer();
}

ButtonControls *LilygoT5S3::get_button_controls(QueueHandle_t ui_queue)
{
  (void)ui_queue;
  return new NoButtonControls();
}

TouchControls *LilygoT5S3::get_touch_controls(Renderer *renderer, QueueHandle_t ui_queue)
{
  (void)renderer;
  (void)ui_queue;
  // GT911 not wired up yet - dummy implementation for now.
  return new TouchControls();
}