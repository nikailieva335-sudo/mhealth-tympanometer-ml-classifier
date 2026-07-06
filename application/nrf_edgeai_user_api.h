#ifndef _NRF_EDGEAI_USER_API_H_
#define _NRF_EDGEAI_USER_API_H_

#include <stdint.h>

#define NRF_EDGEAI_USER_INPUTS_NUM 67

typedef struct {
    float input[NRF_EDGEAI_USER_INPUTS_NUM];
} nrf_edgeai_user_inputs_t;

typedef struct {
    uint8_t class;
} nrf_edgeai_user_outputs_t;

int nrf_edgeai_user_model_run(nrf_edgeai_user_inputs_t *p_input, void *reserved);
nrf_edgeai_user_outputs_t *nrf_edgeai_user_model_get_output(void *reserved);

#endif