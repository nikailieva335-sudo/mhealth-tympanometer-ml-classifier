/* 2026-06-28T22:32:08.696425 */
/*
* Copyright (c) 2026 Nordic Semiconductor ASA
* SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
*/
#include "nrf_edgeai_user_model.h"
#include "nrf_edgeai_user_types.h"
#include <nrf_edgeai/nrf_edgeai_platform.h>
#include <nrf_edgeai/rt/private/nrf_edgeai_interfaces.h>
#include <assert.h>

//////////////////////////////////////////////////////////////////////////////
/* Nordic EdgeAI Lab Solution ID and Runtime Version */
#define EDGEAI_LAB_SOLUTION_ID_STR      "94044"
#define EDGEAI_RUNTIME_VERSION_COMBINED 0x00000202

//////////////////////////////////////////////////////////////////////////////
#define INPUT_TYPE                         f32

/** User input features type */
#define INPUT_FEATURE_DATA_TYPE            NRF_EDGEAI_INPUT_F32

/** Number of unique features in the original input sample */
#define INPUT_UNIQ_FEATURES_NUM            67

/** Number of unique features actually used by NN from the original input sample */
#define INPUT_UNIQ_FEATURES_USED_NUM       67

/** Number of input feature samples that should be collected in the input window
 *  feature_sample = 1 * INPUT_UNIQ_FEATURES_NUM
 */
#define INPUT_WINDOW_SIZE                  1

/** Number of input feature samples on that the input window is shifted */
#define INPUT_WINDOW_SHIFT                 0

/** Number of subwindows in input feature window,
* the SUBWINDOW_SIZE = INPUT_WINDOW_SIZE / INPUT_SUBWINDOW_NUM
* if the window size is not divisible by the number of subwindows without a remainder,
* the remainder is added to the last subwindow size */
#define INPUT_SUBWINDOW_NUM                 0

#define INPUT_UNIQUE_SCALES_NUM (sizeof(INPUT_FEATURES_SCALE_MIN) / sizeof(INPUT_FEATURES_SCALE_MIN[0])) 

/** Defines input(also used for LAG) features MIN scaling factor
 */
static const nrf_user_input_t INPUT_FEATURES_SCALE_MIN[] = {
 0.0000000, -508.0000000, 0.1500000, 52.6307907, -592.0000000, 0.1220656,
 -999.0000000, 0.0880044, 0.0931411, 0.0979217, 0.1037207, 0.1106365,
 0.1187640, 0.1281926, 0.1390053, 0.1512761, 0.1650691, 0.1804365,
 0.1974171, 0.2160349, 0.2362981, 0.2581977, 0.2727733, 0.2729703,
 0.2726480, 0.1424278, -0.1117114, -0.3109217, -0.4582522, -0.5571058,
 -0.6111744, -0.6243740, -0.6007808, -0.5445682, -0.4599468, -0.3511075,
 -0.2221678, -0.0771230, 0.0801984, 0.2093271, 0.2028696, 0.1964429,
 0.1901020, 0.1838984, 0.1778787, 0.1720848, 0.1665529, 0.1613134,
 0.1563910, 0.1518041, 0.1475655, 0.1436819, 0.1401549, 0.1369809,
 0.1341517, 0.1316550, 0.1294751, 0.1275930, 0.1259877, 0.1246363,
 0.1235146, 0.1225981, 0.1218621, 0.1212823, 0.1208356, 0.1204998,
 0.1202547 };

/** Defines input(also used for LAG) features MAX scaling factor
 */
static const nrf_user_input_t INPUT_FEATURES_SCALE_MAX[] = {
 1.0000000, 141.0000000, 9.4600000, 3162.1994629, 400.0000000, 14.2693405,
 0.0000000, 14.2068014, 13.1056395, 12.0127144, 10.9351940, 9.8800211,
 9.0385447, 8.9052229, 8.7880945, 8.7915201, 8.7948236, 8.7979918,
 8.8010139, 8.8038769, 8.8065720, 8.8090897, 8.8114214, 8.8135605,
 8.8155041, 8.8172464, 8.8187876, 8.8201275, 8.8212671, 8.8222113,
 8.8229647, 8.8235350, 8.8239317, 8.8241644, 8.8242445, 8.8241863,
 8.8240032, 8.8237104, 8.8233232, 8.8228569, 8.8223286, 8.8391562,
 8.8987551, 8.9563780, 9.0116787, 9.0643682, 9.1142225, 9.1610699,
 9.2047958, 9.2453327, 9.2826624, 9.3168049, 9.3478203, 9.3757973,
 9.4008560, 9.4231358, 9.4427938, 9.4600039, 9.4749451, 9.4878044,
 9.4987707, 9.5080309, 9.5157690, 9.5221624, 9.5273790, 9.5315781,
 9.5349083 };

/** Defines which unique features from the input data will be used/collected,
 *  one bit for one unique feature, starting from LSB
 */
#define INPUT_FEATURES_USAGE_MASK NULL

/** Defines which unique input features is used for LAG features processing,
 *  one bit for one unique feature, starting from LSB
 */
#define INPUT_FEATURES_USED_FOR_LAGS_MASK NULL

//////////////////////////////////////////////////////////////////////////////
#define MODEL_TYPE                 __NRF_EDGEAI_MODEL_NEUTON
#define MODEL_TASK                 3
#define MODEL_OUTPUTS_NUM          10

#define MODEL_USES_AS_INPUT_INPUT_FEATURES 1
#define MODEL_USES_AS_INPUT_DSP_FEATURES 0
#define MODEL_USES_AS_INPUT_MASK ((MODEL_USES_AS_INPUT_INPUT_FEATURES << 0) | (MODEL_USES_AS_INPUT_DSP_FEATURES << 1)) 

#if MODEL_TYPE == __NRF_EDGEAI_MODEL_AXON 
#include <drivers/axon/nrf_axon_nn_infer.h>  
#include <axon/nrf_axon_platform.h> 
#include "nrf_edgeai_user_model_axon.h" 
#define P_MODEL_INSTANCE &model_axon_user_instance_94044
#else  // MODEL_TYPE == __NRF_EDGEAI_MODEL_NEUTON
#define P_MODEL_INSTANCE &model_neuton_user_instance_ 
#endif


static const nrf_user_output_t MODEL_OUTPUT_SCALE_MIN[] = {
 0.9334819, 0.9258798, 0.8967381, 0.9208034, 0.3437916, 0.8283833,
 0.9482414, 0.4139533, 0.1192166, 0.9190145 };

static const nrf_user_output_t MODEL_OUTPUT_SCALE_MAX[] = {
 0.9413613, 0.9318014, 0.9045710, 0.9340014, 0.3562226, 0.8329785,
 0.9534840, 0.4232562, 0.1321854, 0.9212583 };

static const nrf_user_output_t MODEL_AVERAGE_EMBEDDING[] = {
 0.9353244, 0.9287623, 0.8988488, 0.9242805, 0.3529657, 0.8315067,
 0.9495708, 0.4166751, 0.1281673, 0.9203454 };

#define NN_DECODED_OUTPUT_INIT                                 \
.anomaly = {                                                   \
   .score = 0.f,                                               \
   .meta = { .p_scale_min         = MODEL_OUTPUT_SCALE_MIN,    \
             .p_scale_max         = MODEL_OUTPUT_SCALE_MAX,    \
             .p_average_embedding = MODEL_AVERAGE_EMBEDDING }, \
}

//////////////////////////////////////////////////////////////////////////////
#define MODEL_NEURONS_NUM          20
#define MODEL_WEIGHTS_NUM          74
#define MODEL_PARAMS_TYPE          f32
#define MODEL_REORDERING           0

static const nrf_user_weight_t MODEL_WEIGHTS[] = {
 0.2437826, 0.9532421, -1.0000000, 0.5000000, 1.0000000, -0.5000000,
 -0.3037916, 0.5953975, -0.1245693, 0.4672373, 0.1580726, 0.4670932,
 0.1180255, 0.2374907, -0.1993381, 0.3899050, -0.0387161, 0.8819698,
 0.1042381, -0.4142489, 0.2252867, 0.4561541, 0.5000000, -0.5000000,
 -0.0888214, 0.7586985, -0.2263985, 0.4233622, 0.4077111, 0.2194167,
 0.9931304, -0.4747808, 1.0000000, -0.1732607, 0.3368517, -0.7554941,
 0.3585545, -0.7279818, 0.0977440, 0.6456750, -0.7086593, -0.2068579,
 0.5000000, -0.5000000, 0.0738747, 0.1886992, 0.0148831, 0.7262318,
 0.3699774, 0.3091036, 0.3611169, -0.2317841, 0.5000000, 0.6015121,
 -0.0437471, 0.6090848, -0.1606005, 0.0839087, 0.6776891, 0.2982609,
 -0.6543807, 0.3440124, -0.6997187, 0.6034042, 0.2646785, -0.6825687,
 0.1883551, -0.0812050, 0.8730284, 0.6939546, -0.8335121, 0.7595277,
 -0.7760696, 0.2501617 };

static const uint16_t MODEL_NEURONS_LINKS[] = {
 1, 30, 44, 46, 50, 53, 67, 0, 67, 0, 1, 2, 6, 12, 67, 2, 67, 2, 4, 7, 42,
 50, 52, 54, 67, 4, 67, 4, 49, 67, 6, 67, 0, 6, 67, 8, 67, 6, 8, 1, 3, 6,
 49, 53, 67, 10, 67, 4, 43, 67, 12, 67, 2, 32, 67, 14, 67, 12, 14, 67, 16,
 67, 6, 10, 12, 1, 3, 4, 7, 26, 46, 67, 18, 67 };

static const uint16_t MODEL_NEURON_INTERNAL_LINKS_NUM[] = {
 0, 8, 10, 16, 18, 26, 28, 31, 33, 36, 39, 46, 48, 51, 53, 56, 59, 61, 65,
 73 };

static const uint16_t MODEL_NEURON_EXTERNAL_LINKS_NUM[] = {
 7, 9, 15, 17, 25, 27, 30, 32, 35, 37, 45, 47, 50, 52, 55, 57, 60, 62, 72,
 74 };

static const nrf_user_coeff_t MODEL_NEURON_ACTIVATION_WEIGHTS[] = {
 20.0000000, 14.0817471, 20.0000000, 15.0249939, 20.0000000, 7.5624990,
 20.0000000, 6.9598293, 20.0000000, 8.1843748, 20.0000000, 14.4031258,
 20.0000000, 15.7344704, 20.0000000, 10.0499992, 20.0000000, 11.2937489,
 20.0000000, 6.8039818 };

static const uint8_t MODEL_NEURON_ACTIVATION_TYPE_MASK[] = {
 0x55, 0x55, 0x5 };

static const uint16_t MODEL_OUTPUT_NEURONS_INDICES[] = {
 1, 3, 5, 7, 9, 11, 13, 15, 17, 19 };

/** Model neurons activations buffer */ 
static nrf_user_neuron_t model_neurons_[MODEL_NEURONS_NUM];

/** Neuton model instance */ 
static const nrf_edgeai_model_neuton_t model_neuton_user_instance_ = { 
   .meta.p_neuron_internal_links_num = MODEL_NEURON_INTERNAL_LINKS_NUM,
   .meta.p_neuron_external_links_num = MODEL_NEURON_EXTERNAL_LINKS_NUM,
   .meta.p_output_neurons_indices    = MODEL_OUTPUT_NEURONS_INDICES,
   .meta.p_neuron_links              = MODEL_NEURONS_LINKS,
   .meta.p_neuron_act_type_mask      = MODEL_NEURON_ACTIVATION_TYPE_MASK,
   .meta.outputs_num                 = MODEL_OUTPUTS_NUM,
   .meta.neurons_num                 = MODEL_NEURONS_NUM,
   .meta.weights_num                 = MODEL_WEIGHTS_NUM,
   /// 
   .params.MODEL_PARAMS_TYPE = {
       .p_weights      = MODEL_WEIGHTS,
       .p_act_weights  = MODEL_NEURON_ACTIVATION_WEIGHTS,
       .p_neurons      = model_neurons_,
   },
};

//////////////////////////////////////////////////////////////////////////////
#define INPUT_WINDOW_MEMORY    NULL 
#define P_INPUT_WINDOW_CTX    NULL  

//////////////////////////////////////////////////////////////////////////////
/** The maximum number of extracted features that user used for all unique input features */
#define EXTRACTED_FEATURES_NUM 0 
#define P_DSP_PIPELINE         NULL 

//////////////////////////////////////////////////////////////////////////////
#define NN_INPUT_INIT_INTERFACE        nrf_edgeai_input_init_no_window 
#define NN_INPUT_FEED_INTERFACE        nrf_edgeai_input_feed_no_window 
#define NN_PROCESS_FEATURES_INTERFACE  nrf_edgeai_process_features_scale_vector_f32_f32 
#define NN_INIT_INFERENCE_INTERFACE    nrf_edgeai_init_inference_neuton 
#define NN_RUN_INFERENCE_INTERFACE     nrf_edgeai_run_inference_neuton_f32 
#define NN_PROPAGATE_OUTPUTS_INTERFACE nrf_edgeai_output_propagate_neuton_f32 
#define NN_DECODE_OUTPUTS_INTERFACE    nrf_edgeai_output_decode_anomaly_f32 

//////////////////////////////////////////////////////////////////////////////

static nrf_user_output_t model_outputs_[MODEL_OUTPUTS_NUM];

//////////////////////////////////////////////////////////////////////////////

static nrf_edgeai_t nrf_edgeai_ = {
    ///
    .metadata.p_solution_id     = EDGEAI_LAB_SOLUTION_ID_STR,
    .metadata.version.combined  = EDGEAI_RUNTIME_VERSION_COMBINED,
    ///   
    .input.p_used_for_lags_mask = INPUT_FEATURES_USED_FOR_LAGS_MASK,
    .input.p_usage_mask         = INPUT_FEATURES_USAGE_MASK,
    .input.type                 = INPUT_FEATURE_DATA_TYPE,
    .input.unique_num           = INPUT_UNIQ_FEATURES_NUM,
    .input.unique_num_used      = INPUT_UNIQ_FEATURES_USED_NUM,
    .input.unique_scales_num    = INPUT_UNIQUE_SCALES_NUM,
    .input.window_size          = INPUT_WINDOW_SIZE,
    .input.window_shift         = INPUT_WINDOW_SHIFT,
    .input.subwindow_num        = INPUT_SUBWINDOW_NUM,
    .input.window_memory.p_void = INPUT_WINDOW_MEMORY,
    .input.p_window_ctx         = P_INPUT_WINDOW_CTX,

    .input.scale.INPUT_TYPE = {
        .p_min = INPUT_FEATURES_SCALE_MIN,
        .p_max = INPUT_FEATURES_SCALE_MAX,
    }, 
    ///
    .p_dsp = P_DSP_PIPELINE,
    ///
    .model.type                 = (nrf_edgeai_model_type_t)MODEL_TYPE,
    .model.task                 = (nrf_edgeai_model_task_t)MODEL_TASK,
    .model.instance.p_void      = P_MODEL_INSTANCE,
    .model.output.memory.p_void = model_outputs_,
    .model.output.num           = MODEL_OUTPUTS_NUM,
    .model.uses_as_input.all    = MODEL_USES_AS_INPUT_MASK,
    ///
    .interfaces.input_init          = NN_INPUT_INIT_INTERFACE,
    .interfaces.feed_inputs         = NN_INPUT_FEED_INTERFACE,
    .interfaces.process_features    = NN_PROCESS_FEATURES_INTERFACE,
    .interfaces.init_inference      = NN_INIT_INFERENCE_INTERFACE,
    .interfaces.run_inference       = NN_RUN_INFERENCE_INTERFACE,
    .interfaces.propagate_outputs   = NN_PROPAGATE_OUTPUTS_INTERFACE,
    .interfaces.decode_outputs      = NN_DECODE_OUTPUTS_INTERFACE,
    ///
    .decoded_output = { NN_DECODED_OUTPUT_INIT },
};

//////////////////////////////////////////////////////////////////////////////

nrf_edgeai_t* nrf_edgeai_user_model_94044(void)
{
    return &nrf_edgeai_;
}

//////////////////////////////////////////////////////////////////////////////

uint32_t nrf_edgeai_user_model_neuton_size_94044(void)
{
    uint32_t model_meta_size = 0;
#if MODEL_TYPE == __NRF_EDGEAI_MODEL_NEUTON
    model_meta_size +=
        (sizeof(MODEL_WEIGHTS) + sizeof(MODEL_NEURONS_LINKS) +
         sizeof(MODEL_NEURON_EXTERNAL_LINKS_NUM) + sizeof(MODEL_NEURON_INTERNAL_LINKS_NUM) +
         sizeof(MODEL_NEURON_ACTIVATION_WEIGHTS) + sizeof(MODEL_NEURON_ACTIVATION_TYPE_MASK) +
         sizeof(MODEL_OUTPUT_NEURONS_INDICES));
#endif

#if MODEL_TASK == __NRF_EDGEAI_TASK_ANOMALY_DETECTION
    model_meta_size += sizeof(MODEL_AVERAGE_EMBEDDING) + sizeof(MODEL_OUTPUT_SCALE_MIN) +
                       sizeof(MODEL_OUTPUT_SCALE_MAX);
#endif

#if MODEL_TASK == __NRF_EDGEAI_TASK_REGRESSION
    model_meta_size += sizeof(MODEL_OUTPUT_SCALE_MIN) + sizeof(MODEL_OUTPUT_SCALE_MAX);
#endif

    return model_meta_size;
}


