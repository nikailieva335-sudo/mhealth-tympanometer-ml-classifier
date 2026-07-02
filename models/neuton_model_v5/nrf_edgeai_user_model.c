/* 2026-06-29T00:50:55.916428 */
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
#define EDGEAI_LAB_SOLUTION_ID_STR      "94046"
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
 0.0000000, -508.0000000, 0.0600000, 6.7082038, -592.0000000, 0.0400000,
 -999.0000000, 0.0395264, 0.0395717, 0.0396202, 0.0396725, 0.0397296,
 0.0397923, 0.0398617, 0.0399387, 0.0400244, 0.0401201, 0.0402269,
 0.0403460, 0.0404784, 0.0406254, 0.0407880, 0.0409670, 0.0411632,
 0.0413774, 0.0416101, 0.0418613, 0.0417157, 0.0414564, 0.0413529,
 0.0414265, 0.0416984, 0.0421892, 0.0429187, 0.0439054, 0.0448867,
 0.0452848, 0.0456880, 0.0460936, 0.0464989, 0.0469012, 0.0472975,
 0.0476850, 0.0480608, 0.0484223, 0.0487670, 0.0490925, 0.0493967,
 0.0496778, 0.0499344, 0.0501654, 0.0503701, 0.0505482, 0.0506998,
 0.0508254, 0.0509261, 0.0510030, 0.0510579, 0.0510927, 0.0511097,
 0.0511112, 0.0510998, 0.0510780, 0.0510486, 0.0510138, 0.0509762,
 0.0509378 };

/** Defines input(also used for LAG) features MAX scaling factor
 */
static const nrf_user_input_t INPUT_FEATURES_SCALE_MAX[] = {
 1.0000000, 389.0000000, 28.0200005, 3162.1994629, 393.0000000, 28.0895844,
 0.0000000, 28.0747185, 28.0438004, 28.0134716, 27.9840374, 27.9557953,
 27.9290257, 27.9039917, 27.8809338, 27.8600693, 27.8415794, 27.8256207,
 27.8123112, 27.8017311, 27.7939301, 27.7889156, 27.7866650, 27.7871151,
 27.7901764, 27.7957230, 27.8036079, 27.8136520, 27.8256645, 27.8394279,
 27.8547134, 27.8712845, 27.8888912, 27.9072876, 27.9262218, 27.9454517,
 27.9647388, 27.9838600, 28.0026016, 28.0207691, 28.0381870, 28.0547047,
 28.0701866, 28.0845261, 28.0976429, 28.1094742, 28.1199894, 28.1291771,
 28.1370487, 28.1436348, 28.1489887, 28.1531773, 28.1562805, 28.1583958,
 28.1596222, 28.1600704, 28.1598530, 28.1590805, 28.1578674, 28.1563168,
 28.1545315, 28.1526051, 28.1506195, 28.1486454, 28.1467476, 28.1449718,
 28.1433563 };

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
#define MODEL_TASK                 1
#define MODEL_OUTPUTS_NUM          2

#define MODEL_USES_AS_INPUT_INPUT_FEATURES 1
#define MODEL_USES_AS_INPUT_DSP_FEATURES 0
#define MODEL_USES_AS_INPUT_MASK ((MODEL_USES_AS_INPUT_INPUT_FEATURES << 0) | (MODEL_USES_AS_INPUT_DSP_FEATURES << 1)) 

#if MODEL_TYPE == __NRF_EDGEAI_MODEL_AXON 
#include <drivers/axon/nrf_axon_nn_infer.h>  
#include <axon/nrf_axon_platform.h> 
#include "nrf_edgeai_user_model_axon.h" 
#define P_MODEL_INSTANCE &model_axon_user_instance_94046
#else  // MODEL_TYPE == __NRF_EDGEAI_MODEL_NEUTON
#define P_MODEL_INSTANCE &model_neuton_user_instance_ 
#endif


#define NN_DECODED_OUTPUT_INIT                 \
.classif = {                                   \
   .predicted_class = 0,                       \
   .num_classes = MODEL_OUTPUTS_NUM,           \
}

//////////////////////////////////////////////////////////////////////////////
#define MODEL_NEURONS_NUM          5
#define MODEL_WEIGHTS_NUM          45
#define MODEL_PARAMS_TYPE          f32
#define MODEL_REORDERING           0

static const nrf_user_weight_t MODEL_WEIGHTS[] = {
 1.0000000, -1.0000000, 0.5000000, 1.0000000, -0.5000000, -0.5000000,
 -1.0000000, -1.0000000, 1.0000000, 1.0000000, 0.5000000, -1.0000000,
 -0.5000000, -0.2613772, -0.1099578, -0.0534256, -1.0000000, -1.0000000,
 -0.3156116, 1.0000000, 0.8750000, -0.5000000, 1.0000000, 0.3449123,
 -0.8471680, 0.1673172, -0.9953297, -0.2483103, -1.0000000, -0.7373432,
 0.1116660, -0.2363271, 0.9999999, 1.0000000, 0.9994969, -0.5565921,
 1.0000000, -0.8282868, 0.4272047, -0.5779048, -0.5670527, -0.0085593,
 -1.0000000, -1.0000000, 0.0911706 };

static const uint16_t MODEL_NEURONS_LINKS[] = {
 0, 1, 7, 11, 30, 41, 44, 54, 56, 57, 67, 0, 2, 3, 5, 6, 7, 26, 31, 38, 39,
 44, 49, 67, 1, 67, 0, 1, 0, 5, 6, 7, 11, 12, 14, 25, 30, 31, 34, 36, 38,
 67, 0, 3, 67 };

static const uint16_t MODEL_NEURON_INTERNAL_LINKS_NUM[] = {
 0, 12, 25, 28, 44 };

static const uint16_t MODEL_NEURON_EXTERNAL_LINKS_NUM[] = {
 11, 24, 26, 42, 45 };

static const nrf_user_coeff_t MODEL_NEURON_ACTIVATION_WEIGHTS[] = {
 40.0000000, 40.0000000, 40.0000000, 32.5187531, 40.0000000 };

static const uint8_t MODEL_NEURON_ACTIVATION_TYPE_MASK[] = {
 0xb };

static const uint16_t MODEL_OUTPUT_NEURONS_INDICES[] = {
 4, 2 };

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
#define NN_DECODE_OUTPUTS_INTERFACE    nrf_edgeai_output_decode_classification_f32 

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

nrf_edgeai_t* nrf_edgeai_user_model_94046(void)
{
    return &nrf_edgeai_;
}

//////////////////////////////////////////////////////////////////////////////

uint32_t nrf_edgeai_user_model_neuton_size_94046(void)
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


