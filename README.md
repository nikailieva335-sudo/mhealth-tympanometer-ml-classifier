# mHealth Tympanometer Embedded ML Classifier Training Data Processing
## Author: Nika Ilieva

## Table of Contents
- [Overview](#overview)
- [Model Descriptions](#models)
- [Installation](#installation)
- [How to Run](#how-to-run)


## Overview
This project contains the data-processing pipeline used to prepare tympanometric datasets for machine learning model training, validation and evaluation.
Each notebook applies a different preprocessing approach to the tympanometry data. These alternative datasets are used to compare modeling strategies and improve the accuracy and reliability of the tympanometry infection classifier.

## Model Descriptions
Model 1  \
 `model_training_data_audiologist_only_abc.ipynb`\
This approach involves only using audiologist recorded data and audiologist labels.
Removing the layman data from the training. This notebook still classifies the types as A/B/C.

Evaluation csv: *val_analysis_neuton_ao.csv* \
Analysis csv: *analysis_neuton_ao.csv*

Model 2 \
`model_training_data_audiologist_only_binary.ipynb`\
This approach involves only using audiologist recorded data and audiologist labels. Removing the layman data from the training.
This model classifies the data as normal (pass) and abnormal (refer to a doctor).

Evaluation csv: *val_neuton_ao_b.csv* \
Analysis csv: *analysis_neuton_ao_b3.csv*

Model 3 \
`model_training_data_anomaly.ipynb` \
This approach involves Neuton.AI's Anomaly Detection model. The model requires unlabeled, normal only data.

Evaluation csv: *val_neuton_anomaly.csv* \
Analysis csv: *analysis_neuton_anomaly.csv*

Model 4 \
`model_training_data_audiologist_only_b_equal_BC.ipynb`\
This approach involves only using audiologist recorded data and audiologist labels.It classifies into normal/abnormal. The data is in equal distribution between A/B/C.
In order to not lose the normal sample data, the abnormal data was upsampled instead. B and C are upsampled separately so they are represented equally. This was an attempt to fix the C underreperesentation and bias.

Evaluation csv: *val_neuton__ao_b_equal.csv* \
Analysis csv: *analysis_neuton_ao_b_equal.csv*

Model 5 \
`model_training_data_only_b_A_equal_BC.ipynb`
This approach involves only using audiologist recorded data and audiologist labels.It classifies into normal/abnormal. The data is in equal distribution between A/B/C.
In order to not lose the normal sample data, the abnormal data was upsampled instead. B and C are upsampled so B = C and B + C = A.

Evaluation csv: *val_neuton__ao_b_equal_A_BC.csv* \
Analysis csv: *analysis_neuton_ao_b_equal_A_BC.csv*

## Installation
Make sure Python is installed
## How to Run
1. Make sure all neccesary libraries are installed `%pip install numpy pandas scipy scikit-learn`

2. Open the Jupyter Notebook of interest and run all cells.

