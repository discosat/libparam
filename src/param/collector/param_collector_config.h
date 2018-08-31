/*
 * param_collector_config.h
 *
 *  Created on: Aug 30, 2018
 *      Author: johan
 */

#ifndef LIB_PARAM_SRC_PARAM_COLLECTOR_PARAM_COLLECTOR_CONFIG_H_
#define LIB_PARAM_SRC_PARAM_COLLECTOR_PARAM_COLLECTOR_CONFIG_H_

struct param_collector_config_s {
	uint8_t node;
	uint32_t interval;
	uint32_t mask;
};

extern struct param_collector_config_s param_collector_config[];

void param_collector_init(void);



#endif /* LIB_PARAM_SRC_PARAM_COLLECTOR_PARAM_COLLECTOR_CONFIG_H_ */
