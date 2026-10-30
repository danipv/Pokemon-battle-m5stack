#include "ir_link.h"
#include "ir_frame.h"
#include <Arduino.h>
#include <driver/rmt.h>

bool irHardwareBegin() {
    rmt_config_t cfg=RMT_DEFAULT_CONFIG_TX(GPIO_NUM_5,RMT_CHANNEL_0);
    cfg.clk_div=80; // 80 MHz APB / 80 = one microsecond per duration tick
    cfg.tx_config.carrier_en=true;
    cfg.tx_config.carrier_freq_hz=38000;
    cfg.tx_config.carrier_duty_percent=50;
    cfg.tx_config.carrier_level=RMT_CARRIER_LEVEL_HIGH;
    cfg.tx_config.idle_output_en=true;
    cfg.tx_config.idle_level=RMT_IDLE_LEVEL_LOW;
    esp_err_t error=rmt_config(&cfg);
    if (error==ESP_OK) error=rmt_driver_install(RMT_CHANNEL_0,0,0);
    Serial.printf("[ir] RMT TX GPIO5 38kHz duty=50%% init=%s\n",esp_err_to_name(error));
    return error==ESP_OK;
}

bool irHardwareSend(uint32_t raw) {
    irlink::Pulse pulses[irlink::FRAME_ITEMS];
    irlink::necFrame(raw,pulses);
    rmt_item32_t items[irlink::FRAME_ITEMS]={};
    for (unsigned i=0;i<irlink::FRAME_ITEMS;++i) {
        items[i].level0=1; items[i].duration0=pulses[i].mark;
        items[i].level1=0; items[i].duration1=pulses[i].space;
    }
    const esp_err_t error=rmt_write_items(RMT_CHANNEL_0,items,irlink::FRAME_ITEMS,true);
    if (error!=ESP_OK) Serial.printf("[ir] TX error: %s\n",esp_err_to_name(error));
    return error==ESP_OK;
}
