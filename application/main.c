#include <zephyr/kernel.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Neuton models
#include "nrf_edgeai_user_model.h"
#include "nrf_edgeai_user_types.h"

// Data structure
typedef struct {
 uint32_t uin;
    int idx;
    int target;
    int no_peak;
    float tpp;
    float ecv;
    float sa;
    float est_tpp;
    float est_ecv;
    float tw;
    float samples[60];
} patient_t;

int main(void) {
    printk("\n╔════════════════════════════════════════╗\n");
    printk("║   ML Model Evaluation                  ║\n");
    printk("╚════════════════════════════════════════╝\n\n");

const char *csv_file = NULL;
    const char *model_name = NULL;

    #ifdef MODEL_V1
    csv_file = "analysis_neuton_ao.csv";
    model_name = "Model V1 (AO)";
    #endif

    #ifdef MODEL_V2
    csv_file = "analysis_neuton_ao_b.csv";
    model_name = "Model V2 (AO_B)";
    #endif

    #ifdef MODEL_V3
    csv_file = "analysis_neuton_ao_b_anomaly.csv";
    model_name = "Model V3 (Anomaly Detection)";
    #endif

    #ifdef MODEL_V4
    csv_file = "analysis_neuton_ao_equal.csv";
    model_name = "Model V4 (Types ABC Equalized)";
    #endif

    #ifdef MODEL_V5
    csv_file = "analysis_neuton_ao_equal_A_BC.csv";
    model_name = "Model V5 (A=B+C Equalized)";
    #endif

    if (csv_file == NULL) {
        printk("ERROR: No model selected\n");
        return -1;
    }

    printk("Testing: %s\n", model_name);
    printk("Loading: %s\n\n", csv_file);

FILE *f = fopen(csv_file, "r");
    if (!f) {
        printk("ERROR: Cannot open %s\n", csv_file);
        return -1;
    }

// Data parsing

char line[4096];
fgets(line, sizeof(line), f);

int total = 0;
int correct = 0;
int per_class[10] = {0};
int correct_per_class[10] = {0};

while (fgets(line, sizeof(line), f)) {
    patient_t p;

    char *token = strtok(line, ",");
    p.uin = atoi(token);

    char *token = strtok(line, ",");
    p.uin = atoi(token);

    token = strtok(NULL, ",");
    p.idx = atoi(token);

    token = strtok(NULL, ",");
    p.target = atoi(token);

    token = strtok(NULL, ",");
    p.no_peak = atoi(token);

    token = strtok(NULL, ",");
    p.tpp = atof(token);

    token = strtok(NULL, ",");
    p.ecv = atof(token);

    token = strtok(NULL, ",");
    p.sa = atof(token);

    token = strtok(NULL, ",");
    p.est_tpp = atof(token);

    token = strtok(NULL, ",");
    p.est_ecv = atof(token);

    token = strtok(NULL, ",");
    p.tw = atof(token);


    for (int i = 0; i < 60; i++) {
        token = strtok(NULL, ",");
        p.samples[i] = atof(token);
    }

// Double Check This

    nrf_edgeai_user_inputs_t model_input;
    float min_val = p.samples[0];
    float max_val = p.samples[0];
    float sum = 0.0f;

    for (int i = 0; i < 60; i++) {
        if (p.samples[i] < min_val) min_val = p.samples[i];
        if (p.samples[i] > max_val) max_val = p.samples[i];
        sum += p.samples[i];
    }

    float mean_val = sum / 60.0f;
    float range = max_val - min_val;

    // Feed features to model
    model_input.input[0] = min_val;
    model_input.input[1] = max_val;
    model_input.input[2] = mean_val;
    model_input.input[3] = range;
    model_input.input[4] = p.samples[0];
    model_input.input[5] = p.samples[59];
    model_input.input[6] = p.tpp;
    model_input.input[7] = p.ecv;
    model_input.input[8] = p.sa;
    model_input.input[9] = p.tw;

    // Run Neuton model
    int ret = nrf_edgeai_user_model_run(&model_input, NULL);
    if (ret != 0) {
        printk("ERROR: Model inference failed for UIN %u\n", p.uin);
        continue;
    }

    // Get prediction
    nrf_edgeai_user_outputs_t *output = nrf_edgeai_user_model_get_output(NULL);
    uint8_t predicted = output->class;
    uint8_t expert = (uint8_t)p.target;

    // Count results
    total++;
    per_class[expert]++;

    if (predicted == expert) {
        correct++;
        correct_per_class[expert]++;
    }

    // Progress indicator
    if ((total % 50) == 0) {
        printk("Processed %d patients...\n", total);
    }
}

    fclose(f);

    // ─── Print results ───
    printk("\n╔════════════════════════════════════════════════╗\n");
    printk("║         %s RESULTS\n", model_name);
    printk("╠════════════════════════════════════════════════╣\n");
    printk("║ Total patients: %d                             ║\n", total);
    printk("║ Correct: %d                                    ║\n", correct);
    printk("║ Accuracy: %.1f%%                                ║\n",
           (float)correct * 100.0f / total);
    printk("╚════════════════════════════════════════════════╝\n\n");

    // ─── Per-class results ───
    if (total > 0) {
        printk("Per-Class Accuracy:\n");
        for (int i = 0; i < 10; i++) {
            if (per_class[i] > 0) {
                float class_acc = (float)correct_per_class[i] / per_class[i] * 100.0f;
                printk("  Class %d: %.1f%% (%d/%d)\n", i, class_acc,
                       correct_per_class[i], per_class[i]);
            }
        }
    }

    return 0;
}
