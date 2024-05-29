# Copyright (c) 2021, University of Washington
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are met:
#
# 1. Redistributions of source code must retain the above copyright notice,
#    this list of conditions and the following disclaimer.
#
# 2. Redistributions in binary form must reproduce the above copyright notice,
#    this list of conditions and the following disclaimer in the documentation
#    and/or other materials provided with the distribution.
#
# 3. Neither the name of the University of Washington nor the names of its
#    contributors may be used to endorse or promote products derived from this
#    software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE UNIVERSITY OF WASHINGTON AND CONTRIBUTORS
# “AS IS” AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
# TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
# PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE UNIVERSITY OF WASHINGTON OR
# CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
# EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
# PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
# OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
# WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
# OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
# ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
#
# ----------------------------------------------------------------------------
# The code below was adapted from
#
#   https://github.com/kiefk/SeafloorMapping_PointNet2
#
# with the associated license replicated here:
# ----------------------------------------------------------------------------
#
# PointNet++: Deep Hierarchical Feature Learning on Point Sets in a Metric Space.
# 
# Copyright (c) 2017, Geometric Computation Group of Stanford University
# 
# The MIT License (MIT)
# 
# Copyright (c) 2017 Charles R. Qi
# 
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
# 
# The above copyright notice and this permission notice shall be included in all
# copies or substantial portions of the Software.
# 
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
# SOFTWARE.

import warnings
warnings.simplefilter(action='ignore', category=FutureWarning)

import logging
logging.basicConfig(level=logging.DEBUG)

import pandas as pd
import numpy as np
from tqdm import tqdm
import torch
from torch.utils.data import Dataset
import importlib
import math
import sys
import json
import os

##############
# READ INPUTS
##############

# process command line
if len(sys.argv) == 5:
    settings_json   = sys.argv[1]
    info_json       = sys.argv[2]
    input_csv       = sys.argv[3]
    output_csv      = sys.argv[4]
elif len(sys.argv) == 4:
    settings_json   = None
    info_json       = sys.argv[1]
    input_csv       = sys.argv[2]
    output_csv      = sys.argv[3]
elif len(sys.argv) == 3:
    settings_json   = None
    info_json       = sys.argv[1]
    input_csv       = sys.argv[2]
    output_csv      = None
else:
    print("Incorrect parameters supplied: python runner.py [<settings json>] <input json spot file> <input csv spot file> [<output csv spot file>]")
    sys.exit()

# read settings json
if settings_json != None:
    with open(settings_json, 'r') as json_file:
        settings = json.load(json_file)
else:
    settings = {}

# set configuration
maxElev         = settings.get('maxElev', 10) 
minElev         = settings.get('minElev', -50)
minSignalConf   = settings.get('minSignalConf', 3)
minNWDI         = settings.get('minNWDI', 0.3)
useNWDI         = settings.get('useNWDI', False)
batch_size      = settings.get('batch_size', 1)
gpu             = settings.get('gpu', "0")
num_point       = settings.get('num_point', 8192)
use_signal_conf = settings.get('use_signal_conf', True)
num_votes       = settings.get('num_votes', 10)
threshold       = settings.get('threshold', 0.5)

# read info json
with open(info_json, 'r') as json_file:
    beam_info = json.load(json_file)

##################
# BUILD DATAFRAME
##################

print(f'Preprocessing {input_csv}...')

# read input csv and store into a dataframe
beam_df = pd.read_csv(input_csv)

# Add a class column
beam_df['class'] = np.full((len(beam_df)), 3)

# If beam_df.class_ph == 41 (sea surface), then set the class for beam_df at that same photon index to 5 (sea surface for PointNet)
beam_df.loc[beam_df.class_ph == 41, 'class'] = 5

# Get the subset of the sea_surface_df where class_ph is sea surface
sea_surface_df = beam_df[beam_df['class'] == 5]
# Take the average of the geoid corrected height of the sea surface photons.
average_sea_surface_level = sea_surface_df['geoid_corr_h'].mean()
# Only keep the photons that are lower than average sea surface level to decrease data volume.
beam_df = beam_df[beam_df['geoid_corr_h'] < average_sea_surface_level]
# Keep photons where the max_signal_conf is greater than a minimum threshold.
beam_df = beam_df[beam_df['max_signal_conf'] >= minSignalConf]

# Remove irrelevant photons (deeper than 50m, higher than 10m)
beam_df = beam_df[(beam_df["geoid_corr_h"] > minElev) & (beam_df["geoid_corr_h"] < maxElev)]
    
beam_df = beam_df[(beam_df["dem_h"] - beam_df["geoid_corr_h"] < 50)]

# Remove photons where the NDWI value is greater than a minimum threshold.
if useNWDI:
    beam_df = beam_df[beam_df["ndwi"] >= minNWDI]

# Create dataframe with the columns and order expected by the rest of pointnet
data_df = pd.DataFrame()
data_df['index_ph'] = beam_df['index_ph']
data_df['x'] = beam_df['x_ph']
data_df['y'] = beam_df['y_ph']
data_df['lon'] = beam_df['longitude']
data_df['lat'] = beam_df['latitude']
data_df['elev'] = beam_df['geoid_corr_h']
data_df['signal_conf_ph'] = beam_df['max_signal_conf']
data_df['class'] = beam_df['class']

# Normalize signal confidence
data_df['signal_conf_ph'] = data_df['signal_conf_ph'] - 2

# Create the 2d numpy array used by pointnet
data = data_df.to_numpy().astype(np.float64)

################
# RUN POINTNET2
################

def to_categorical(y, num_classes):
    """ 1-hot encodes a tensor """
    new_y = torch.eye(num_classes)[y.cpu().data.numpy(),]
    return new_y.to(y.device)

def pc_denormalize(pc, pc_min, pc_max):
    for i in range(pc.shape[1]):
        pc[:, i] = (pc[:, i] + 1) / 2 * (pc_max[i] - pc_min[i]) + pc_min[i]
    return pc

def pc_normalize(pc):
    pc_min = np.empty(3, dtype=np.float64)
    pc_max = np.empty(3, dtype=np.float64)
    for i in range(pc.shape[1]):
        pc_min[i] = min(pc[:, i])
        pc_max[i] = max(pc[:, i])
        pc[:, i] = 2 * ((pc[:, i] - pc_min[i]) / (pc_max[i] - pc_min[i])) - 1
    return pc, pc_min, pc_max

class PartNormalDataset(Dataset):
    def __init__(self, data, npoints, use_conf_channel, use_utm_coords=True):
        self.data = data
        self.npoints = npoints
        self.use_conf_channel = use_conf_channel
                            #   use_utm_coords      use_conf_channel                          
        self.column_indices = { True:           {   True: [1, 2, 5, 6], # use x,y,elev,signal_conf
                                                    False: [1, 2, 5]},  # use x,y,elev
                                False:          {   True: [3, 4, 5, 6], # use lon,lat,elev,signal_conf
                                                    False: [3, 4, 5]}   # use lon,lat,elev
                                }[use_utm_coords][use_conf_channel]


    def __getitem__(self, index):
        r0 = self.npoints * index
        r1 = r0 + self.npoints
        cls = np.array([0]).astype(np.int32)
        point_set = np.copy(self.data[r0:r1, self.column_indices])
        if self.use_conf_channel:
            point_set[:, -1] = point_set[:, -1].astype(np.int32)
        point_set_normalized = point_set
        point_set_normalized[:, 0:3], pc_min, pc_max = pc_normalize(point_set[:, 0:3])
        point_set_normalized_mask = np.full(self.npoints, True, dtype=bool)
        # resample
        length = len(point_set)
        if length < self.npoints:
            pad_point = np.ones((self.npoints-length, len(self.column_indices)), dtype=np.float64)
            if self.use_conf_channel:
                pad_point[:, -1] = pad_point[:, -1].astype(np.int32)
            point_set_normalized = np.concatenate((point_set_normalized, pad_point), axis=0)
            # create mask for point set - mask out the padded points
            pad_point_bool = np.full(self.npoints - length, False, dtype=bool)
            point_set_normalized_bool = np.full(length, True, dtype=bool)
            point_set_normalized_mask = np.concatenate((point_set_normalized_bool, pad_point_bool))

        return point_set_normalized, cls, point_set_normalized_mask, pc_min, pc_max, (r0,r1)

    def __len__(self):
        return math.ceil(len(self.data) / self.npoints)


os.environ["CUDA_VISIBLE_DEVICES"] = gpu
# set device
device = torch.device("cuda" if torch.cuda.is_available() else "cpu")


TEST_DATASET = PartNormalDataset(data, num_point, use_signal_conf)
testDataLoader = torch.utils.data.DataLoader(TEST_DATASET, batch_size=batch_size, shuffle=False, num_workers=3)
print("The number of test data is: %d" % len(TEST_DATASET))
num_classes = 1
num_part = 2

'''MODEL LOADING'''
sys.path.append('/pointnet2')
sys.path.append('/pointnet2/models')
MODEL = importlib.import_module('pointnet2_part_seg_msg')
classifier = MODEL.get_model(num_part, conf_channel=use_signal_conf).to(device)
trained_model = torch.load('/pointnet2/trained_model/model.pth', map_location=torch.device(device))

model_state_dict = {k.replace('module.', ''): v for k, v in trained_model['model_state_dict'].items()}
classifier.load_state_dict(model_state_dict)

with torch.no_grad():

    classifier = classifier.eval()
    for batch_id, (points, label, point_set_normalized_mask, pc_min, pc_max, row_slice) in \
            tqdm(enumerate(testDataLoader), total=len(testDataLoader), smoothing=0.9):

        cur_batch_size, NUM_POINT, _ = points.size()
        points, label = points.float().to(device), label.long().to(device)
        points = points.transpose(2, 1)
        vote_pool = torch.zeros(cur_batch_size, NUM_POINT, num_part).to(device)

        for _ in range(num_votes):
            seg_pred, _ = classifier(points, to_categorical(label, num_classes))
            vote_pool += seg_pred

        seg_pred = vote_pool / num_votes

        cur_pred = seg_pred.cpu().numpy()
        cur_pred_val = np.zeros((cur_batch_size, NUM_POINT)).astype(np.int32)
        cur_pred_prob = np.zeros((cur_batch_size, NUM_POINT)).astype(np.float64)
        point_set_normalized_mask = point_set_normalized_mask.numpy()

        prob = np.exp(cur_pred[0, :, :])
        cur_pred_prob[0, :] = prob[:, 1]  # the probability of belonging to seafloor class
        cur_pred_val[0, :] = np.where(prob[:, 1] < threshold, 0, 1)
        cur_mask = point_set_normalized_mask[0, :]
        cur_pred_prob_mask = cur_pred_prob[0, cur_mask]
        cur_pred_val_mask = cur_pred_val[0, cur_mask]


        index_ph = data[row_slice[0]:row_slice[1], 0]
        class_ph = data[row_slice[0]:row_slice[1], 7]
        columns = {'index_ph': index_ph, 'class_ph': class_ph}            
        output_df = pd.DataFrame(columns)
        output_df.loc[output_df['class_ph'] == 3, 'class_ph'] = 1   # other
        output_df.loc[output_df['class_ph'] == 5, 'class_ph'] = 41  # sea surface
        output_df.loc[cur_pred_val_mask == 1, 'class_ph'] = 40   # bathymetry

        output_data = output_df.to_numpy().astype(np.int32)

        # output file
        if batch_id == 0:
            mode = 'w'
            header = 'index_ph,class_ph'
        else:
            mode = 'a'
            header = ''
        if output_csv != None:
            with open(output_csv, mode) as f:
                np.savetxt(f, output_data, delimiter=',', header=header, fmt='%d', comments='')
        else:
            np.savetxt(sys.stdout, output_data, delimiter=',', header=header, fmt='%d', comments='')
