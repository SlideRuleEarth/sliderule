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
import os

##############
# READ INPUTS
##############

# process command line
sys.path.append('../utils')
from command_line_processor import process_command_line
settings, spot_info, spot_df, output_csv, info_json = process_command_line(sys.argv, columns=['index_ph', 'x_ph', 'y_ph', 'ortho_h', 'max_signal_conf', 'class_ph', 'surface_h'])

# set configuration
maxElev         = settings.get('maxElev', 10) 
minElev         = settings.get('minElev', -50)
minSignalConf   = settings.get('minSignalConf', 3)
gpu             = settings.get('gpu', "0")
num_point       = settings.get('num_point', 8192)
batch_size      = settings.get('batch_size', 8)
num_votes       = settings.get('num_votes', 10)
threshold       = settings.get('threshold', 0.5)
model_seed      = settings.get('model_seed', 24)

##################
# BUILD DATAFRAME
##################

# Create dataframe with the columns and order expected by the rest of pointnet
data_df = pd.DataFrame()
data_df['index_ph'] = spot_df['index_ph']
data_df['x'] = spot_df['x_ph']
data_df['y'] = spot_df['y_ph']
data_df['elev'] = spot_df['ortho_h']
data_df['signal_conf_ph'] = spot_df['max_signal_conf']

# Add a pointnet specific class column
data_df['class'] = np.full((len(data_df)), 3)                               # initialize everything to 3 (pointnet noise)
data_df.loc[spot_df.class_ph == 41, 'class'] = 5                            # change sea surface (41) to 5 (pointnet sea surface)

# Remove photons above sea surfaace
#sea_surface_df = data_df[data_df['class'] == 5]                             # get the subset of the sea_surface_df where class_ph is sea surface
#average_sea_surface_level = sea_surface_df['elev'].mean()                   # take the average of the geoid corrected height of the sea surface photons
#data_df = data_df[data_df['elev'] < average_sea_surface_level]
data_df = data_df[data_df['elev'] < spot_df['surface_h']]

# Remove photons where the max_signal_conf doesn't meet minimum threshold
data_df = data_df[data_df['signal_conf_ph'] >= minSignalConf]

# Remove photons outside of height range
data_df = data_df[(data_df["elev"] > minElev) & \
                  (data_df["elev"] < maxElev)]                              # if geoid height is outside absolute range

# Normalize signal confidence
data_df['signal_conf_ph'] = data_df['signal_conf_ph'] - 2

# Create the 2d numpy array used by pointnet
data = data_df.to_numpy().astype(np.float64)

###########################
# HELPER FUNCTIONS & CLASS
###########################

def to_categorical(y, num_classes):
    """ 1-hot encodes a tensor """
    new_y = torch.eye(num_classes)[y.cpu().data.numpy(),]
    return new_y.to(y.device)

def pc_normalize(pc):
    pc_min = np.empty(3, dtype=np.float64)
    pc_max = np.empty(3, dtype=np.float64)
    for i in range(pc.shape[1]):
        pc_min[i] = min(pc[:, i])
        pc_max[i] = max(pc[:, i])
        pc[:, i] = 2 * ((pc[:, i] - pc_min[i]) / (pc_max[i] - pc_min[i])) - 1
    return pc, pc_min, pc_max

class PartNormalDataset(Dataset):
    def __init__(self, data, npoints):
        self.data = data
        self.npoints = npoints
        self.column_indices = [1, 2, 3, 4] # use x,y,elev,signal_conf

    def __getitem__(self, index):
        r0 = self.npoints * index
        r1 = r0 + self.npoints
        cls = np.array([0]).astype(np.int32)
        point_set = np.copy(self.data[r0:r1, self.column_indices])
        point_set[:, -1] = point_set[:, -1].astype(np.int32) # retype the signal confidence column
        point_set_normalized = point_set
        point_set_normalized[:, 0:3], pc_min, pc_max = pc_normalize(point_set[:, 0:3])
        point_set_normalized_mask = np.full(self.npoints, True, dtype=bool)
        # pad out the returned dataset to the fixed npoints
        length = len(point_set)
        if length < self.npoints:
            pad_point = np.ones((self.npoints-length, len(self.column_indices)), dtype=np.float64)
            pad_point[:, -1] = pad_point[:, -1].astype(np.int32) # retype the signal confidence padded column
            point_set_normalized = np.concatenate((point_set_normalized, pad_point), axis=0)
            # create mask for point set - mask out the padded points
            pad_point_bool = np.full(self.npoints - length, False, dtype=bool)
            point_set_normalized_bool = np.full(length, True, dtype=bool)
            point_set_normalized_mask = np.concatenate((point_set_normalized_bool, pad_point_bool))
        # return normalized dataset
        return point_set_normalized, cls, point_set_normalized_mask, pc_min, pc_max, (r0,r1)

    def __len__(self):
        return math.ceil(len(self.data) / self.npoints)

################
# RUN POINTNET2
################

# Set a manual_seed for reproducibilty of results. 
torch.manual_seed(model_seed)

# set device
os.environ["CUDA_VISIBLE_DEVICES"] = gpu
device = torch.device("cuda" if torch.cuda.is_available() else "cpu")

# create normalized dataset to process
TEST_DATASET = PartNormalDataset(data, num_point)
testDataLoader = torch.utils.data.DataLoader(TEST_DATASET, batch_size=batch_size, shuffle=False, num_workers=8)
print("The number of test data is: %d" % len(TEST_DATASET))
num_classes = 1
num_part = 2

# load pointnet2 ML model
sys.path.append('/pointnet2')
MODEL = importlib.import_module('pointnet2_part_seg_msg')
classifier = MODEL.get_model(num_part, conf_channel=True).to(device)
trained_model = torch.load('/data/pointnet2_model.pth', map_location=torch.device(device))
model_state_dict = {k.replace('module.', ''): v for k, v in trained_model['model_state_dict'].items()}
classifier.load_state_dict(model_state_dict)

# run classifier
output_dfs = []
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
        point_set_normalized_mask = point_set_normalized_mask.numpy()

        cur_pred_val_mask = []
        for i in range(cur_batch_size):
            prob = np.exp(cur_pred[i, :, :])
            cur_pred_val[0, :] = np.where(prob[:, 1] < threshold, 0, 1)
            cur_mask = point_set_normalized_mask[i, :]
            cur_pred_val_mask.append(cur_pred_val[i, cur_mask])
        cur_pred_val_mask = np.concatenate(cur_pred_val_mask)

        # build dataframe of index and classification
        index_ph = data[row_slice[0][0]:row_slice[1][-1], 0].astype(np.int32)
        class_ph = data[row_slice[0][0]:row_slice[1][-1], 5]
        columns = {'index_ph': index_ph, 'class_ph': class_ph}            
        batch_df = pd.DataFrame(columns)
#        batch_df.loc[(cur_pred_val_mask == 1) & (batch_df['class_ph'] != 5), 'class_ph'] = 40 # only keep bathymetry photons (prediction == 1) when original didn't have sea surface
        batch_df.loc[cur_pred_val_mask == 1, 'class_ph'] = 40 # set bathymetry photons (prediction == 1)
        batch_df.loc[batch_df['class_ph'] == 5, 'class_ph'] = 41 # restore sea surface where it wasn't overwritten
        batch_df.loc[(batch_df['class_ph'] != 40) & (batch_df['class_ph'] != 41), 'class_ph'] = 1 # set everything else to other
        output_dfs.append(batch_df)

################
# WRITE OUTPUTS
################

output_df = pd.concat(output_dfs)
output_df.set_index('index_ph', inplace=True)
spot_df.set_index('index_ph', inplace=True)
output_df = output_df.combine_first(spot_df).reset_index()

if output_csv != None:
    output_df.to_csv(output_csv, index=False, columns=["index_ph", "class_ph"])
else:
    print(output_df.to_string(index=False, header=True, columns=['index_ph', 'class_ph']))
