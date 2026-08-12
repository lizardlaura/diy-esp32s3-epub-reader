// Exclude this class if the board is EPDIY
#ifndef BOARD_TYPE_EPDIY
#include <esp_sleep.h>
#include <driver/rtc_io.h>
#include <driver/gpio.h>
#include <esp_log.h>
#include <esp_timer.h>
#include "GPIOButtonControls.h"
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

// ULP support is only available on some targets / IDF versions.
// Make usage conditional so builds succeed when esp32/ulp.h is absent.
#if __has_include(<esp32/ulp.h>)
#include <esp32/ulp.h>
#include "ulp_main.h"
#define GPIOBTN_HAS_ULP 1
#else
#define GPIOBTN_HAS_ULP 0
#endif

#if GPIOBTN_HAS_ULP
extern const uint8_t ulp_main_bin_start[] asm("_binary_ulp_main_bin_start");
extern const uint8_t ulp_main_bin_end[] asm("_binary_ulp_main_bin_end");
#endif

GPIOButtonControls::GPIOButtonControls(
    gpio_num_t gpio_up,
    gpio_num_t gpio_down,
    gpio_num_t gpio_select,
    int active_level,
    ActionCallback_t on_action)
    : gpio_up(gpio_up), gpio_down(gpio_down), gpio_select(gpio_select),
      active_level(active_level),
      on_action(on_action)
{
  gpio_install_isr_service(0);
  up = new GPIOButton(gpio_up, active_level, [this]()
                      { this->on_action(UIAction::UP); });
  down = new GPIOButton(gpio_down, active_level, [this]()
                        { this->on_action(UIAction::DOWN); });
  select = new GPIOButton(gpio_select, active_level, [this]()
                          { this->on_action(UIAction::SELECT); });
}

bool GPIOButtonControls::did_wake_from_deep_sleep()
{
  auto wake_cause = esp_sleep_get_wakeup_cause();
  // if our controls are active low then we must have been woken by the ULP
  // as that's the only mechanism available for active low buttons
  if (active_level == 0 && wake_cause == ESP_SLEEP_WAKEUP_ULP)
  {
    ESP_LOGI("Controls", "ULP Wakeup");
    return true;
  }
  if (wake_cause == ESP_SLEEP_WAKEUP_EXT1)
  {
    ESP_LOGI("Controls", "EXT1 Wakeup");
    return true;
  }
  return false;
}

UIAction GPIOButtonControls::get_deep_sleep_action()
{
  // If ULP support is available and buttons are active-low, prefer ULP status.
#if GPIOBTN_HAS_ULP
  if (active_level == 0)
  {
    uint16_t rtc_pin = ulp_gpio_status & UINT16_MAX;
    ESP_LOGI("Controls", "***** rtc_pin: %d", rtc_pin);
    if ((rtc_pin & (1 << rtc_io_number_get(gpio_up))))
    {
      ESP_LOGI("Controls", "***** UP %d, %d, %d", 1 << rtc_io_number_get(gpio_up), rtc_pin, (rtc_pin & (1 << rtc_io_number_get(gpio_up))));
      return UIAction::UP;
    }
    else if ((rtc_pin & (1 << rtc_io_number_get(gpio_down))))
    {
      ESP_LOGI("Controls", "***** DOWN %d, %d, %d", 1 << rtc_io_number_get(gpio_down), rtc_pin, (rtc_pin & (1 << rtc_io_number_get(gpio_down))));
      return UIAction::DOWN;
    }
    else if ((rtc_pin & (1 << rtc_io_number_get(gpio_select))))
    {
      ESP_LOGI("Controls", "***** SELECT %d, %d, %d", 1 << rtc_io_number_get(gpio_select), rtc_pin, (rtc_pin & (1 << rtc_io_number_get(gpio_select))));
      return UIAction::SELECT;
    }
  }
#endif

  // Fallback / active-high buttons use EXT1 wakeup status.
  
    uint64_t ext1_buttons = esp_sleep_get_ext1_wakeup_status();
    if (ext1_buttons & (1ULL << gpio_up))
    {
      return UIAction::UP;
    }
    else if (ext1_buttons & (1ULL << gpio_down))
    {
      return UIAction::DOWN;
    }
    else if (ext1_buttons & (1ULL << gpio_select))
    {
      return UIAction::SELECT;
    }
  
  return UIAction::NONE;
}

bool GPIOButtonControls::setup_deep_sleep()
{
#if GPIOBTN_HAS_ULP
  if (active_level == 0)
  {
    rtc_gpio_init(gpio_up);
    rtc_gpio_set_direction(gpio_up, RTC_GPIO_MODE_INPUT_ONLY);
    rtc_gpio_pullup_en(gpio_up);

    rtc_gpio_init(gpio_down);
    rtc_gpio_set_direction(gpio_down, RTC_GPIO_MODE_INPUT_ONLY);
    rtc_gpio_pullup_en(gpio_down);

    rtc_gpio_init(gpio_select);
    rtc_gpio_set_direction(gpio_select, RTC_GPIO_MODE_INPUT_ONLY);
    rtc_gpio_pullup_en(gpio_select);

    // need to use the ULP if we have buttons that are active low
    // see ulp/main.S for more details
    esp_err_t err = ulp_load_binary(0, ulp_main_bin_start, (ulp_main_bin_end - ulp_main_bin_start) / sizeof(uint32_t));
    ESP_ERROR_CHECK(err);

    ulp_up_mask = 1 << rtc_io_number_get(gpio_up);
    ulp_down_mask = 1 << rtc_io_number_get(gpio_down);
    ulp_select_mask = 1 << rtc_io_number_get(gpio_select);

    ulp_set_wakeup_period(0, 100 * 1000); // 100 ms
    err = ulp_run(&ulp_entry - RTC_SLOW_MEM);
    ESP_ERROR_CHECK(err);
    return;
  }
#endif

//   // For active-high buttons, or when ULP is unavailable, use EXT1 wakeup.
// rtc_gpio_init(gpio_up);
//   rtc_gpio_set_direction(gpio_up, RTC_GPIO_MODE_INPUT_ONLY);
//   rtc_gpio_init(gpio_down);
//   rtc_gpio_set_direction(gpio_down, RTC_GPIO_MODE_INPUT_ONLY);
//   rtc_gpio_init(gpio_select);
//   rtc_gpio_set_direction(gpio_select, RTC_GPIO_MODE_INPUT_ONLY);

//   if (active_level == 0)
//   {
//     rtc_gpio_pulldown_dis(gpio_up);
//     rtc_gpio_pullup_en(gpio_up);
//     rtc_gpio_pulldown_dis(gpio_down);
//     rtc_gpio_pullup_en(gpio_down);
//     rtc_gpio_pulldown_dis(gpio_select);
//     rtc_gpio_pullup_en(gpio_select);
//   }
//   else
//   {
//     rtc_gpio_pullup_dis(gpio_up);
//     rtc_gpio_pulldown_en(gpio_up);
//     rtc_gpio_pullup_dis(gpio_down);
//     rtc_gpio_pulldown_en(gpio_down);
//     rtc_gpio_pullup_dis(gpio_select);
//     rtc_gpio_pulldown_en(gpio_select);
//   }

//   esp_sleep_enable_ext1_wakeup(
//       (1ULL << gpio_up) | (1ULL << gpio_down) | (1ULL << gpio_select),
//       active_level == 0 ? ESP_EXT1_WAKEUP_ANY_LOW : ESP_EXT1_WAKEUP_ANY_HIGH);
// For active-high buttons, or when ULP is unavailable, use EXT1 wakeup.
  // NOTE: on ESP32-S3 only GPIO0..GPIO21 are RTC-capable. GPIO39 (select) is
  // not, so it is filtered out here and cannot wake the device.
  const gpio_num_t candidates[] = {gpio_up, gpio_down, gpio_select};
  uint64_t ext1_mask = 0;

  for (gpio_num_t pin : candidates)
  {
    if (!rtc_gpio_is_valid_gpio(pin))
    {
      ESP_LOGW("Controls", "GPIO%d is not RTC-capable, excluded from wake mask", pin);
      continue;
    }
    rtc_gpio_init(pin);
    rtc_gpio_set_direction(pin, RTC_GPIO_MODE_INPUT_ONLY);
    if (active_level == 0)
    {
      rtc_gpio_pulldown_dis(pin);
      rtc_gpio_pullup_en(pin);
    }
    else
    {
      rtc_gpio_pullup_dis(pin);
      rtc_gpio_pulldown_en(pin);
    }
    ext1_mask |= (1ULL << pin);
  }

  if (ext1_mask == 0)
  {
    ESP_LOGE("Controls", "no RTC-capable wake pins - refusing deep sleep");
    return false;
  }

  // Level-triggered wake: if a button is still held (or GPIO21's 100k/4.7uF
  // RC hasn't recovered) we would wake immediately. Wait for release.
  const int64_t deadline = esp_timer_get_time() + 3000000;
  while (esp_timer_get_time() < deadline)
  {
    bool all_released = true;
    for (gpio_num_t pin : candidates)
    {
      if (gpio_get_level(pin) == active_level)
      {
        all_released = false;
        break;
      }
    }
    if (all_released)
      break;
    vTaskDelay(pdMS_TO_TICKS(20));
  }

  esp_err_t err = esp_sleep_enable_ext1_wakeup(
      ext1_mask,
      active_level == 0 ? ESP_EXT1_WAKEUP_ANY_LOW : ESP_EXT1_WAKEUP_ANY_HIGH);
  if (err != ESP_OK)
  {
    ESP_LOGE("Controls", "ext1 arm failed: %s - refusing deep sleep", esp_err_to_name(err));
    return false;
  }
  return true;

}
#endif