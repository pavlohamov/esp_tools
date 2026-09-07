/*
 * i2cSpave.cpp
 *
 *  Created on: 11 Aug 2026
 *      Author: pavloha
 */




#if 0

#include "driver/i2c_slave.h"

struct slave_event {
	bool request;
    uint32_t length;
    uint8_t *buffer;
};

static bool i2c_slave_request_cb(i2c_slave_dev_handle_t i2c_slave, const i2c_slave_request_event_data_t *evt_data, void *arg) {
	QueueHandle_t sq = (QueueHandle_t)arg;
	slave_event se = {
		.request = true,
	};
    BaseType_t xTaskWoken = 0;
    xQueueSendFromISR(sq, &se, &xTaskWoken);
    return xTaskWoken;
}

static bool i2c_slave_receive_cb(i2c_slave_dev_handle_t i2c_slave, const i2c_slave_rx_done_event_data_t *evt_data, void *arg) {
	QueueHandle_t sq = (QueueHandle_t)arg;
	slave_event se = {
		.request = false,
		.length = evt_data->length,
	};
	se.buffer = (uint8_t*)malloc(evt_data->length);
	memcpy(se.buffer, evt_data->buffer, evt_data->length);
    BaseType_t xTaskWoken = 0;
    xQueueSendFromISR(sq, &se, &xTaskWoken);
    return xTaskWoken;
}
i2c_slave_config_t i2c_slv_config = {
    .i2c_port = 1,
    .sda_io_num = GPIO_NUM_8,
    .scl_io_num = GPIO_NUM_9,
    .clk_source = I2C_CLK_SRC_DEFAULT,
    .send_buf_depth = 100,
    .receive_buf_depth = 100,
    .slave_addr = 0x66,
};

i2c_slave_dev_handle_t slave;
ESP_ERROR_CHECK(i2c_new_slave_device(&i2c_slv_config, &slave));

QueueHandle_t sq = xQueueCreate(16, sizeof(slave_event));

i2c_slave_event_callbacks_t cbs = {
	.on_request = i2c_slave_request_cb,
    .on_receive = i2c_slave_receive_cb,
};
ESP_ERROR_CHECK(i2c_slave_register_event_callbacks(slave, &cbs, sq));

slave_event evt;
while (xQueueReceive(sq, &evt, 1000)) {
	if (evt.request) {
		ESP_LOGE(TAG, "rq");
	} else {
		ESP_LOGE(TAG, "data %d", evt.length);
		ESP_LOG_BUFFER_HEXDUMP(TAG, evt.buffer, evt.length, ESP_LOG_WARN);
	}
	if (evt.buffer)
		free(evt.buffer);
}


#endif
