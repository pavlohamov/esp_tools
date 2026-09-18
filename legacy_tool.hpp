/*
 * legacy_tool.hpp
 *
 *  Created on: 7 Sept 2026
 *      Author: pavloha
 */

#pragma once

#include <stdint.h>


#ifdef __cplusplus
extern "C" {
#endif


int WiFi_init(void);
int WiFi_connected(void);


int SNTP_request(void);
int SNTP_request_sync(uint32_t tout_s);


#ifdef __cplusplus
}
#endif
