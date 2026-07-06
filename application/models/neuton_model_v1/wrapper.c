/* Bridges the Nordic nRF Edge AI runtime (nrf_edgeai_init / feed_inputs / run_inference)
 * to the simplified nrf_edgeai_user_model_run() / nrf_edgeai_user_model_get_output()
 * interface that main.c calls.
 */
#include <stdbool.h>

#include <nrf_edgeai/nrf_edgeai.h>

#include "nrf_edgeai_user_model.h"
#include "nrf_edgeai_user_api.h"

/* Placeholder cutoff for anomaly-task models: score above this is reported as class 1
 * (anomaly), otherwise class 0 (normal). Tune against validation data. */
#define ANOMALY_SCORE_THRESHOLD 0.5f

static nrf_edgeai_t *p_edgeai;
static nrf_edgeai_user_outputs_t last_output;
static bool initialized;

int nrf_edgeai_user_model_run(nrf_edgeai_user_inputs_t *p_input, void *reserved)
{
    (void)reserved;

    if (!initialized) {
        p_edgeai = nrf_edgeai_user_model();
        if (nrf_edgeai_init(p_edgeai) != NRF_EDGEAI_ERR_SUCCESS) {
            return -1;
        }
        initialized = true;
    }

    uint32_t expected_inputs_num = nrf_edgeai_uniq_inputs_num(p_edgeai) *
                                    nrf_edgeai_input_window_size(p_edgeai);
    if (expected_inputs_num != NRF_EDGEAI_USER_INPUTS_NUM) {
        /* nrf_edgeai_user_inputs_t.input[] doesn't hold as many features as this
         * model expects; feeding it would over-read the caller's buffer. */
        return -1;
    }

    if (nrf_edgeai_feed_inputs(p_edgeai, p_input->input, NRF_EDGEAI_USER_INPUTS_NUM) !=
        NRF_EDGEAI_ERR_SUCCESS) {
        return -1;
    }

    if (nrf_edgeai_run_inference(p_edgeai) != NRF_EDGEAI_ERR_SUCCESS) {
        return -1;
    }

    if (nrf_edgeai_model_task(p_edgeai) == NRF_EDGEAI_TASK_ANOMALY_DETECTION) {
        last_output.class = (p_edgeai->decoded_output.anomaly.score > ANOMALY_SCORE_THRESHOLD)
                                 ? 1
                                 : 0;
    } else {
        last_output.class = (uint8_t)p_edgeai->decoded_output.classif.predicted_class;
    }

    return 0;
}

nrf_edgeai_user_outputs_t *nrf_edgeai_user_model_get_output(void *reserved)
{
    (void)reserved;
    return &last_output;
}
