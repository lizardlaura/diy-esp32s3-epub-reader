#include <esp_log.h>
#include "Board.h"
#include <SDCard.h>
#include "battery/ADCBattery.h"

#if defined(BOARD_TYPE_LILYGO_T5_47_S3)
#include "LilygoT5S3.h"
#else
#include "PaperS3.h"
#endif
 
Board *Board::factory()
{
#if defined(BOARD_TYPE_LILYGO_T5_47_S3)
  return new LilygoT5S3();
#else
  return new PaperS3();
#endif
}
 


void Board::start_filesystem()
{
  ESP_LOGI("main", "Using SDCard");
  // initialise the SDCard
  sdcard = new SDCard("/fs", SD_CARD_PIN_NUM_MISO, SD_CARD_PIN_NUM_MOSI, SD_CARD_PIN_NUM_CLK, SD_CARD_PIN_NUM_CS);
}

void Board::stop_filesystem()
{
  delete sdcard;
}

Battery *Board::get_battery()
{
#ifdef BATTERY_ADC_CHANNEL
  return new ADCBattery(BATTERY_ADC_CHANNEL);
#else
  return nullptr;
#endif
}

TouchControls *Board::get_touch_controls(Renderer *renderer, QueueHandle_t ui_queue)
{
  return new TouchControls();
}
