/* 2026-06-27T02:13:38.394152 */
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
#define EDGEAI_LAB_SOLUTION_ID_STR      "94036"
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
 0.0000000, -508.0000000, 0.0600000, 33.6154709, -592.0000000, 0.0509902,
 -999.0000000, 0.0535063, 0.0532749, 0.0529821, 0.0526266, 0.0522083,
 0.0517280, 0.0511876, 0.0505903, 0.0499407, 0.0492448, 0.0485100,
 0.0477452, 0.0469609, 0.0461691, 0.0453831, 0.0446180, 0.0438900,
 0.0432166, 0.0426165, -0.1117114, -0.3109217, -0.4582522, -0.5571058,
 -0.6111744, -0.6243740, -0.6007808, -0.5445682, -0.4599468, -0.3511075,
 -0.2221678, -0.0771230, 0.0507321, 0.0532154, 0.0560219, 0.0591521,
 0.0626027, 0.0663662, 0.0704312, 0.0747821, 0.0793989, 0.0842576,
 0.0893301, 0.0945843, 0.0999848, 0.1054930, 0.1110675, 0.1166649,
 0.1222406, 0.1277490, 0.1331448, 0.1383836, 0.1434231, 0.1482238,
 0.1527497, 0.1569698, 0.1608584, 0.1643961, 0.1675702, 0.1703752,
 0.1728130 };

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
#define MODEL_TASK                 0
#define MODEL_OUTPUTS_NUM          3

#define MODEL_USES_AS_INPUT_INPUT_FEATURES 1
#define MODEL_USES_AS_INPUT_DSP_FEATURES 0
#define MODEL_USES_AS_INPUT_MASK ((MODEL_USES_AS_INPUT_INPUT_FEATURES << 0) | (MODEL_USES_AS_INPUT_DSP_FEATURES << 1)) 

#if MODEL_TYPE == __NRF_EDGEAI_MODEL_AXON 
#include <drivers/axon/nrf_axon_nn_infer.h>  
#include <axon/nrf_axon_platform.h> 
#include "nrf_edgeai_user_model_axon.h" 
#define P_MODEL_INSTANCE &model_axon_user_instance_94036
#else  // MODEL_TYPE == __NRF_EDGEAI_MODEL_NEUTON
#define P_MODEL_INSTANCE &model_neuton_user_instance_ 
#endif


#define NN_DECODED_OUTPUT_INIT                 \
.classif = {                                   \
   .predicted_class = 0,                       \
   .num_classes = MODEL_OUTPUTS_NUM,           \
}

//////////////////////////////////////////////////////////////////////////////
#define MODEL_NEURONS_NUM          19
#define MODEL_WEIGHTS_NUM          139
#define MODEL_PARAMS_TYPE          f32
#define MODEL_REORDERING           0

static const nrf_user_weight_t MODEL_WEIGHTS[] = {
 1.0000000, -0.9493128, 0.0788985, 0.7409320, 0.0287035, 1.0000000,
 1.0000000, -0.8750000, 1.0000000, -0.7202026, -0.1271182, -1.0000000,
 -0.2780826, 0.1317481, 0.5276408, -1.0000000, -0.9033361, 0.8378379,
 -0.4487900, -0.2633902, 0.4560702, -0.0572917, -0.1194863, 0.5000000,
 0.9499125, -0.0927465, 0.3018680, -0.8990655, -0.0464195, -0.6045343,
 -0.9995459, 0.9793987, 1.0000000, 0.6965382, 0.1697264, 0.6126466,
 -0.5346462, -0.3711162, 0.2207452, -0.1976865, -0.6032200, -0.8753946,
 -0.9155118, 0.4763676, -1.0000000, -1.0000000, 0.5068995, 0.8484636,
 0.3628867, -0.3623287, 0.4433747, 0.9956043, -0.9989211, -0.1676619,
 -0.3711400, 0.3667750, 1.0000000, -0.7537852, -0.7715825, -0.1011370,
 -0.7359701, -1.0000000, 0.9776310, 1.0000000, -1.0000000, -0.6138495,
 0.5210267, 0.0084778, 0.2481745, 0.9999611, 0.1443404, 0.5632030,
 0.7376167, -0.6213678, -0.2399173, -0.7858857, -1.0000000, 0.8205571,
 -1.0000000, -1.0000000, -1.0000000, 0.7923416, -0.9958972, -0.7669587,
 0.7491360, -0.1171451, -0.8137444, -0.7614155, 0.5966176, -0.8309512,
 -0.0495426, 0.6436478, 0.0012742, 0.0245279, 0.0394574, -1.0000000,
 0.9295344, 0.0006730, -0.9752819, -0.6093750, -0.0766657, -0.5489700,
 0.7322684, 0.2102808, 0.9066575, 0.9498683, -0.8592117, -0.2968965,
 -0.3886330, -0.7806460, -0.9843516, 1.0000000, -0.7709359, 0.9218750,
 1.0000000, 0.3400756, -0.4931939, 0.0222384, 0.0995431, -0.1313494,
 -1.0000000, 0.1698630, -0.2369360, -0.0431966, 0.2169352, 0.0595318,
 0.1536615, 0.7290826, -1.0000000, -0.9628359, -0.0014646, -0.2611843,
 1.0000000, 0.2852020, -1.0000000, 1.0000000, -1.0000000, 1.0000000,
 0.0946531 };

static const uint16_t MODEL_NEURONS_LINKS[] = {
 0, 1, 3, 5, 6, 10, 11, 46, 67, 0, 1, 2, 3, 4, 6, 7, 43, 67, 0, 1, 1, 2, 8,
 39, 67, 0, 1, 2, 1, 4, 6, 67, 3, 1, 4, 39, 67, 0, 2, 3, 4, 9, 10, 67, 2,
 5, 67, 0, 1, 2, 1, 2, 3, 4, 8, 42, 43, 45, 67, 1, 2, 1, 2, 8, 43, 45, 67,
 0, 2, 4, 7, 1, 48, 67, 1, 2, 3, 5, 7, 9, 1, 3, 4, 48, 67, 1, 2, 3, 5, 3,
 4, 67, 2, 11, 2, 3, 43, 67, 2, 12, 1, 6, 67, 2, 12, 1, 4, 67, 0, 3, 9, 10,
 11, 13, 14, 67, 0, 1, 2, 11, 12, 2, 3, 43, 67, 1, 11, 2, 3, 43, 67, 1, 4,
 7, 8, 12, 16, 17, 67 };

static const uint16_t MODEL_NEURON_INTERNAL_LINKS_NUM[] = {
 0, 9, 20, 28, 33, 40, 46, 50, 61, 71, 80, 89, 94, 100, 105, 115, 121, 127,
 138 };

static const uint16_t MODEL_NEURON_EXTERNAL_LINKS_NUM[] = {
 9, 18, 25, 32, 37, 44, 47, 59, 67, 74, 85, 92, 98, 103, 108, 116, 125,
 131, 139 };

static const nrf_user_coeff_t MODEL_NEURON_ACTIVATION_WEIGHTS[] = {
 40.0000000, 40.0000000, 40.0000000, 36.9683113, 17.7939224, 35.0855598,
 35.0855598, 32.1851196, 32.1851196, 30.8134232, 33.0553513, 33.0553513,
 32.1851196, 33.0553513, 40.0000000, 40.0000000, 32.1851196, 32.1851196,
 32.1851196 };

static const uint8_t MODEL_NEURON_ACTIVATION_TYPE_MASK[] = {
 0xbf, 0x7f, 0x3 };

static const uint16_t MODEL_OUTPUT_NEURONS_INDICES[] = {
 15, 18, 6 };

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

nrf_edgeai_t* nrf_edgeai_user_model_94036(void)
{
    return &nrf_edgeai_;
}

//////////////////////////////////////////////////////////////////////////////

uint32_t nrf_edgeai_user_model_neuton_size_94036(void)
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


