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

import pandas as pd
import pyarrow as pa
import pyarrow.parquet as pq
import numpy as np
import xgboost as xgb
import multiprocessing
import h5py
import sys
import time
import json

from cshelph import c_shelph as CSHELPH
from medianfilter import medianmodel as MEDIANFILTER
from bathypathfinder.BathyPathFinder import BathyPathSearch
from pointnet.pointnet2 import PointNet2
from openoceans.openoceans import OpenOceans

import warnings
warnings.simplefilter(action='ignore', category=FutureWarning)

# constants
ATLAS_GPS_EPOCH = 1198800018
RELEASE = "1"
CLASSIFIERS = ['qtrees', 'coastnet', 'openoceanspp', 'medianfilter', 'cshelph', 'bathypathfinder', 'pointnet', 'openoceans', 'ensemble']
BEAMS = ["gt1l", "gt1r", "gt2l", "gt2r", "gt3l", "gt3r"]

# #####################
# Command Line Inputs
# #####################

# check command line parameters
if len(sys.argv) <= 1:
    print("Not enough parameters: python runner.py <settings json file>")
    sys.exit()

# read settings json
settings_json = sys.argv[1]
with open(settings_json, 'r') as json_file:
    settings = json.load(json_file)

# get setttings
profile = settings.get('profile', {})
format  = settings.get('format', 'parquet')

# #####################
# Read In Data
# #####################

beam_list = [] # list of beams there is data for
beam_table = {} # [beam] => DataFrame
spot_table = {} # [beam] => spot
rqst_parms = {} # request parameters

for beam in BEAMS:
    if beam in settings:
        # read input data
        parquet_file = pq.ParquetFile(settings[beam])
        beam_meta = json.loads(parquet_file.metadata.metadata[b'meta'])
        beam_df = pd.read_parquet(settings[beam])
        # create spot column
        beam_df["spot"] = beam_meta["spot"]
        # expand predictions column
        classifiers_2d = np.stack(beam_df['predictions'].values)
        for i in range(len(CLASSIFIERS)):
            beam_df[CLASSIFIERS[i]] = classifiers_2d[:, i]
        beam_df.drop(columns='predictions', inplace=True)
        # set global variables
        beam_list.append(beam)
        beam_table[beam] = beam_df
        spot_table[beam] = beam_meta["spot"]
        if len(rqst_parms) == 0:
            rqst_parms = json.loads(parquet_file.metadata.metadata[b'sliderule'])

# #####################
# CShelph
# #####################

def cshelph(spot, df):
    parms = settings.get('cshelph', {})
    results = CSHELPH.c_shelph_classification(
        df[["lat_ph", "lon_ph", "geoid_corr_h", "index_ph", "class_ph"]],
        surface_buffer      = parms.get('surface_buffer', -0.5),
        h_res               = parms.get('h_res', 0.5),
        lat_res             = parms.get('lat_res', 0.001),
        thresh              = parms.get('thresh', 25),
        min_buffer          = parms.get('min_buffer', -80),
        max_buffer          = parms.get('max_buffer', 5),
        min_photons_per_bin = parms.get('min_photons_per_bin', 5),
        sea_surface_label   = 41,
        bathymetry_label    = 40 )
    print(f'cshelph completed spot {spot}')
    return results['classification'].astype(np.int8)

# #####################
# MedianFilter
# #####################

def medianfilter(spot, df):
    parms = settings.get('medianfilter', {})
    results = MEDIANFILTER.rolling_median_bathy_classification(
        point_cloud         = df[["lat_ph", "lon_ph", "geoid_corr_h", "index_ph", "class_ph"]],
        window_sizes        = parms.get('window_sizes', [51, 30, 7]) ,
        kdiff               = parms.get('kdiff', 0.75),
        kstd                = parms.get('kstd', 1.75),
        high_low_buffer     = parms.get('high_low_buffer', 4),
        min_photons         = parms.get('min_photons', 14),
        segment_length      = parms.get('segment_length', 0.001),
        compress_heights    = parms.get('compress_heights', None),
        compress_lats       = parms.get('compress_lats', None))
    print(f'medianfilter completed spot {spot}')
    return results['classification'].astype(np.int8)

# #####################
# BathyPathFinder
# #####################

def bathypathfinder(spot, df):
    # get parameters
    parms           = settings.get('bathypathfinder', {})
    tau             = parms.get('tau', 0.5)
    k               = parms.get('k', 15)
    n               = parms.get('n', 99)
    find_surface    = parms.get('find_surface', False)

    # build dataframe to process
    bathy_df = pd.DataFrame()
    bathy_df['x_atc'] = df['x_atc'].values
    bathy_df['geoid_corr_h'] = df['geoid_corr_h'].values
    bathy_df['class_ph'] = df['class_ph'].values

    # keep only photons below sea surface
    if not find_surface:
        sea_surface_df = bathy_df[bathy_df['class_ph'] == 41]
        sea_surface_bottom = sea_surface_df['geoid_corr_h'].min()
        data_df = bathy_df.loc[bathy_df['geoid_corr_h'] < sea_surface_bottom] # remove sea photons and above
    else:
        data_df = bathy_df

    # run bathy pathfinder
    bps = BathyPathSearch(tau, k, n)
    bps.fit(data_df['x_atc'], data_df['geoid_corr_h'], find_surface)

    # write bathy classifications to spot df
    bathy_df['bathypathfinder'] = 0
    bathy_df.loc[bps.bathy_photons.index, 'bathypathfinder'] = 40
    if not find_surface:
        bathy_df.loc[sea_surface_df.index, 'bathypathfinder'] = 41
    else:
        bathy_df.loc[bps.sea_surface_photons.index, 'bathypathfinder'] = 41

    print(f'bathypathfinder completed spot {spot}')
    return bathy_df['bathypathfinder'].to_numpy()

# #####################
# PointNet2
# #####################

def pointnet(spot, df):
    parms = settings.get('pointnet', {})
    results = PointNet2(df[['index_ph', 'x_ph', 'y_ph', 'geoid_corr_h', 'max_signal_conf', 'class_ph', 'surface_h']],
        model_filename          = parms.get('model_filename', '/data/pointnet2_model.pth'),
        maxElev                 = parms.get('maxElev', 10),
        minElev                 = parms.get('minElev', -50),
        minSignalConf           = parms.get('minSignalConf', 3),
        gpu                     = parms.get('gpu', "0"),
        num_point               = parms.get('num_point', 8192),
        batch_size              = parms.get('batch_size', 8),
        num_votes               = parms.get('num_votes', 10),
        threshold               = parms.get('threshold', 0.5),
        model_seed              = parms.get('model_seed', 24),
        seaSurfaceDecimation    = parms.get('seaSurfaceDecimation', 0.8)) # removes 80% of sea surface
    print(f'pointnet completed spot {spot}')
    return results

# #####################
# OpenOceans
# #####################

def openoceans(spot, df):
    parms = settings.get('openoceans', {})
    results = OpenOceans(df[['index_ph', 'x_ph', 'y_ph', 'geoid_corr_h', 'max_signal_conf', 'class_ph', 'surface_h']],
        spot            = spot,
        res_along_track = parms.get('res_along_track', 10),
        res_z           = parms.get('res_z', 0.2),
        window_size     = parms.get('window_size', 11), # 3x overlap is not enough to filter bad daytime noise
        range_z         = parms.get('range_z', [-50, 30]), # include at least a few meters more than 5m above the surface for noise estimation, key for daytime case noise filtering
        verbose         = parms.get('verbose', False), # not really fully integrated, it's still going to print some recent debugging statements
        chunk_size      = parms.get('chunk_size', 65536)) # number of photons to process at one time
    print(f'openoceans completed spot {spot}')
    return results

# #####################
# Ensemble
# #####################

def ensemble(spot, df):
    ensemble_model_filename = settings['ensemble']['ensemble_model_filename']
    print(f'loading ensemble model: {ensemble_model_filename}')
    df = df[['geoid_corr_h', 'surface_h', 'qtrees', 'cshelph', 'medianfilter', 'bathypathfinder', 'openoceanspp', 'coastnet']]
    clf = xgb.XGBClassifier(device='cpu')
    clf.load_model(ensemble_model_filename)
    x = df.to_numpy()
    p = clf.predict(x)
    p[p == 1] = 40
    p[p == 2] = 41
    print(f'ensemble completed spot {spot}')
    return p

# #####################
# Run Classifiers
# #####################

# runner function
def runClassifier(classifier, classifier_func, num_processes=6):
    global beam_list, beam_table, spot_table, rqst_parms
    print(f'running classifier {classifier}...')
    start = time.time()
    if classifier in rqst_parms["classifiers"]:
        if num_processes > 1:
            pool = multiprocessing.Pool(processes=num_processes)
            results = pool.starmap(classifier_func, [(spot_table[beam], beam_table[beam]) for beam in beam_list])
            pool.close()
            pool.join()
            for i in range(len(beam_list)):
                beam_table[beam_list[i]][classifier] = results[i]
        else:
            for beam in beam_list:
                beam_table[beam][classifier] = classifier_func(spot_table[beam], beam_table[beam])
    duration = time.time() - start
    print(f'{classifier} finished in {duration:.03f} seconds')
    return duration

# call runners
profile["cshelph"] = runClassifier("cshelph", cshelph)
profile["medianfilter"] = runClassifier("medianfilter", medianfilter)
profile["bathypathfinder"] = runClassifier("bathypathfinder", bathypathfinder)
profile["pointnet"] = runClassifier("pointnet", pointnet, num_processes=1)
profile["ensemble"] = runClassifier("ensemble", ensemble)

# #####################
# DataFrame & MetaData
# #####################

# create one large dataframe of all spots
df = pd.concat([beam_table[beam] for beam in beam_list])
print("Concatenated data frames into a single data frame")

# set processing flags
df["processing_flags"] = df["processing_flags"] + ((df["cshelph"] == 40) * 2**28) + ((df["medianfilter"] == 40) * 2**27) + ((df["bathypathfinder"] == 40) * 2**29) + ((df["pointnet"] == 40) * 2**30)

# apply subaqueous corrections
corrections_start_time = time.time()
df.reset_index(drop=False, inplace=True)
subaqueous_df = df[df["geoid_corr_h"] < df["surface_h"]] # only photons below sea surface
subaqueous_df = subaqueous_df[subaqueous_df["ensemble"].isin([0, 40])] # exclude photons labeled as sea surface
df.loc[subaqueous_df.index, 'ortho_h'] += df.loc[subaqueous_df.index, 'refracted_dZ']
df.loc[subaqueous_df.index, 'ellipse_h'] += df.loc[subaqueous_df.index, 'refracted_dZ']
df.loc[subaqueous_df.index, 'lat_ph'] = df.loc[subaqueous_df.index,"refracted_lat"]
df.loc[subaqueous_df.index, 'lon_ph'] = df.loc[subaqueous_df.index, "refracted_lon"]
df.loc[subaqueous_df.index, 'sigma_thu'] = df.loc[subaqueous_df.index, "subaqueous_sigma_thu"]
df.loc[subaqueous_df.index, 'sigma_tvu'] = df.loc[subaqueous_df.index, "subaqueous_sigma_tvu"]
df.set_index("time_ns", inplace=True)
profile["corrections_duration"] = time.time() - corrections_start_time

# get dataframe of only bathy points to calculate bathymetry statistics
bathy_df = df[df["ensemble"] == 40]
bathy_df["depth"] = bathy_df["surface_h"] - bathy_df["ortho_h"]

# build stats table
stats = {
    "total_photons": len(df),
    "sea_surface_photons": len(df[df.ensemble == 41]),
    "subaqueous_photons": len(subaqueous_df),
    "bathy_photons": len(bathy_df),
    "bathy_strong_photons": len(bathy_df[bathy_df.spot.isin([1,3,5])]),
    "bathy_linear_coverage":  bathy_df.index_seg.nunique() * 20.0, # number of unique segments with bathymetry X size of each segment in meters
    "bathy_mean_depth": bathy_df.depth.mean().item(),
    "bathy_min_depth": bathy_df.depth.min().item(),
    "bathy_max_depth": bathy_df.depth.max().item(),
    "bathy_std_depth": bathy_df.depth.std().item(),
    "subaqueous_mean_uncertainty": bathy_df.subaqueous_sigma_tvu.mean().item(),
    "subaqueous_min_uncertainty": bathy_df.subaqueous_sigma_tvu.min().item(),
    "subaqueous_max_uncertainty": bathy_df.subaqueous_sigma_tvu.max().item(),
    "subaqueous_std_uncertainty": bathy_df.subaqueous_sigma_tvu.std().item()
}

# read versions
with open("cshelph/cshelph_version.txt") as file:
    cshelph_version = file.read()
with open("medianfilter/medianfilter_version.txt") as file:
    medianfilter_version = file.read()

# update profile
profile["total_duration"] = time.time() - settings["latch"]
print(f'ATL24 total duration is {profile["total_duration"]:.03f} seconds')

# build metadata table
metadata = {
    "sliderule": json.dumps(rqst_parms),
    "profile": json.dumps(profile),
    "stats": json.dumps(stats),
    "cshelph": cshelph_version,
    "medianfilter": medianfilter_version
}

# #####################
# Write Output
# #####################

# Arrow
if format == "parquet":

    table = pa.Table.from_pandas(df, preserve_index=False)
    schema = table.schema.with_metadata(metadata)
    table = table.replace_schema_metadata(schema.metadata)
    pq.write_table(table, settings["filename"])
    print("Writing Parquet file: " + settings["filename"])

# HDF5
elif format == "hdf5" or format == "h5":

    def add_variable(group, name, data, dtype, attrs):
        dataset = group.create_dataset(name, data=data, dtype=dtype)
        for key in attrs:
            dataset.attrs[key] = attrs[key]

    with h5py.File(settings["filename"], 'w') as hf:

        # open granule information
        with open(settings["granule"], 'r') as json_file:
            granule = json.load(json_file)

        # meta data
        metadata_group = hf.create_group("metadata")
        add_variable(metadata_group, "sliderule", metadata["sliderule"], h5py.string_dtype(encoding='utf-8'),
                    {'contentType':'auxiliaryInformation',
                        'description':'sliderule server and request information',
                        'long_name':'SlideRule MetaData',
                        'source':'Derived',
                        'units':'json'})
        add_variable(metadata_group, "profile", metadata["profile"], h5py.string_dtype(encoding='utf-8'),
                    {'contentType':'auxiliaryInformation',
                        'description':'runtimes of the various algorithms',
                        'long_name':'Algorithm RunTimes',
                        'source':'Derived',
                        'units':'json'})
        add_variable(metadata_group, "stats", metadata["stats"], h5py.string_dtype(encoding='utf-8'),
                    {'contentType':'auxiliaryInformation',
                        'description':'granule level statistics',
                        'long_name':'Granule Metrics',
                        'source':'Derived',
                        'units':'json'})

        # ancillary data
        ancillary_group = hf.create_group("ancillary_data")
        add_variable(ancillary_group, "atlas_sdp_gps_epoch", granule["atlas_sdp_gps_epoch"], 'float64',
                    {'contentType':'auxiliaryInformation',
                        'description':'Number of GPS seconds between the GPS epoch (1980-01-06T00:00:00.000000Z UTC) and the ATLAS Standard Data Product (SDP) epoch (2018-01-01:T00.00.00.000000 UTC). Add this value to delta time parameters to compute full gps_seconds (relative to the GPS epoch) for each data point.',
                        'long_name':'ATLAS Epoch Offset',
                        'source':'Operations',
                        'units':'seconds since 1980-01-06T00:00:00.000000Z'})
        add_variable(ancillary_group, "data_end_utc",        granule["data_end_utc"],        h5py.string_dtype(encoding='utf-8'),
                    {'contentType':'auxiliaryInformation',
                        'description':'UTC (in CCSDS-A format) of the last data point within the granule.',
                        'long_name':'End UTC Time of Granule (CCSDS-A, Actual)',
                        'source':'Derived',
                        'units':'1'})
        add_variable(ancillary_group, "data_start_utc",      granule["data_start_utc"],      h5py.string_dtype(encoding='utf-8'),
                    {'contentType':'auxiliaryInformation',
                        'description':'UTC (in CCSDS-A format) of the first data point within the granule.',
                        'long_name':'Start UTC Time of Granule (CCSDS-A, Actual)',
                        'source':'Derived',
                        'units':'1'})
        add_variable(ancillary_group, "end_cycle",           rqst_parms["cycle"],           'int32',
                    {'contentType':'auxiliaryInformation',
                        'description':'The ending cycle number associated with the data contained within this granule. The cycle number is the counter of the number of 91-day repeat cycles completed by the mission.',
                        'long_name':'Ending Cycle',
                        'source':'Derived',
                        'units':'1'})
        add_variable(ancillary_group, "end_delta_time",      granule["end_delta_time"],      'float64',
                    {'contentType':'auxiliaryInformation',
                        'description':'Number of GPS seconds since the ATLAS SDP epoch at the last data point in the file. The ATLAS Standard Data Products (SDP) epoch offset is defined within /ancillary_data/atlas_sdp_gps_epoch as the number of GPS seconds between the GPS epoch (1980-01-06T00:00:00.000000Z UTC) and the ATLAS SDP epoch. By adding the offset contained within atlas_sdp_gps_epoch to delta time parameters, the time in gps_seconds relative to the GPS epoch can be computed.',
                        'long_name':'ATLAS End Time (Actual)',
                        'source':'Derived',
                        'units':'seconds since 2018-01-01',
                        'standard_name': 'time'})
        add_variable(ancillary_group, "end_geoseg",          granule["end_geoseg"],          'int32',
                    {'contentType':'auxiliaryInformation',
                        'description':'The ending geolocation segment number associated with the data contained within this granule. ICESat granule geographic regions are further refined by geolocation segments. During the geolocation process, a geolocation segment is created approximately every 20m from the start of the orbit to the end.  The geolocation segments help align the ATLAS strong a weak beams and provide a common segment length for the L2 and higher products. The geolocation segment indices differ slightly from orbit-to-orbit because of the irregular shape of the Earth. The geolocation segment indices on ATL01 and ATL02 are only approximate because beams have not been aligned at the time of their creation.',
                        'long_name':'Ending Geolocation Segment',
                        'source':'Derived',
                        'units':'1'})
        add_variable(ancillary_group, "end_gpssow",          granule["end_gpssow"],          'float64',
                    {'contentType':'auxiliaryInformation',
                        'description':'GPS seconds-of-week of the last data point in the granule.',
                        'long_name':'Ending GPS SOW of Granule (Actual)',
                        'source':'Derived',
                        'units':'seconds'})
        add_variable(ancillary_group, "end_gpsweek",         granule["end_gpsweek"],         'int32',
                    {'contentType':'auxiliaryInformation',
                        'description':'GPS week number of the last data point in the granule.',
                        'long_name':'Ending GPSWeek of Granule (Actual)',
                        'source':'Derived',
                        'units':'weeks from 1980-01-06'})
        add_variable(ancillary_group, "end_orbit",           rqst_parms["orbit"],           'int32',
                    {'contentType':'auxiliaryInformation',
                        'description':'The ending orbit number associated with the data contained within this granule. The orbit number increments each time the spacecraft completes a full orbit of the Earth.',
                        'long_name':'Ending Orbit Number',
                        'source':'Derived',
                        'units':'1'})
        add_variable(ancillary_group, "end_region",          rqst_parms["region"],          'int32',
                    {'contentType':'auxiliaryInformation',
                        'description':'The ending product-specific region number associated with the data contained within this granule. ICESat-2 data products are separated by geographic regions. The data contained within a specific region are the same for ATL01 and ATL02. ATL03 regions differ slightly because of different geolocation segment locations caused by the irregular shape of the Earth. The region indices for other products are completely independent.',
                        'long_name':'Ending Region',
                        'source':'Derived',
                        'units':'1'})
        add_variable(ancillary_group, "end_rgt",             rqst_parms["rgt"],             'int32',
                    {'contentType':'auxiliaryInformation',
                        'description':'The ending reference groundtrack (RGT) number associated with the data contained within this granule. There are 1387 reference groundtrack in the ICESat-2 repeat orbit. The reference groundtrack increments each time the spacecraft completes a full orbit of the Earth and resets to 1 each time the spacecraft completes a full cycle.',
                        'long_name':'Ending Reference Groundtrack',
                        'source':'Derived',
                        'units':'1'})
        add_variable(ancillary_group, "granule_end_utc",     granule["granule_end_utc"],     h5py.string_dtype(encoding='utf-8'),
                    {'contentType':'auxiliaryInformation',
                        'description':'Requested end time (in UTC CCSDS-A) of this granule.',
                        'long_name':'End UTC Time of Granule (CCSDS-A, Requested)',
                        'source':'Derived',
                        'units':'1'})
        add_variable(ancillary_group, "granule_start_utc",   granule["granule_start_utc"],   h5py.string_dtype(encoding='utf-8'),
                    {'contentType':'auxiliaryInformation',
                        'description':'Requested start time (in UTC CCSDS-A) of this granule.',
                        'long_name':'Start UTC Time of Granule (CCSDS-A, Requested)',
                        'source':'Derived',
                        'units':'1'})
        add_variable(ancillary_group, "release",             RELEASE,                          h5py.string_dtype(encoding='utf-8'),
                    {'contentType':'auxiliaryInformation',
                        'description':'Release number of the granule. The release number is incremented when the software or ancillary data used to create the granule has been changed.',
                        'long_name':'Release Number',
                        'source':'Operations',
                        'units':'1'})
        add_variable(ancillary_group, "resource",            rqst_parms["resource"],           h5py.string_dtype(encoding='utf-8'),
                    {'contentType':'auxiliaryInformation',
                        'description':'ATL03 granule used to produce this granule',
                        'long_name':'ATL03 Resource',
                        'source':'Operations',
                        'units':'1'})
        add_variable(ancillary_group, "sliderule_version",   rqst_parms["sliderule_version"],  h5py.string_dtype(encoding='utf-8'),
                    {'contentType':'auxiliaryInformation',
                        'description':'Version of SlideRule software used to generate this granule',
                        'long_name':'SlideRule Version',
                        'source':'Operations',
                        'units':'1'})
        add_variable(ancillary_group, "sliderule_commit",    rqst_parms["build_information"],  h5py.string_dtype(encoding='utf-8'),
                    {'contentType':'auxiliaryInformation',
                        'description':'Git commit ID (https://github.com/SlideRuleEarth/sliderule.git) of SlideRule software used to generate this granule',
                        'long_name':'SlideRule Commit',
                        'source':'Operations',
                        'units':'1'})
        add_variable(ancillary_group, "sliderule_environment", rqst_parms["environment_version"], h5py.string_dtype(encoding='utf-8'),
                    {'contentType':'auxiliaryInformation',
                        'description':'Git commit ID (https://github.com/SlideRuleEarth/sliderule.git) of SlideRule environment used to generate this granule',
                        'long_name':'SlideRule Environment',
                        'source':'Operations',
                        'units':'1'})
        add_variable(ancillary_group, "start_cycle",         rqst_parms["cycle"],         'int32',
                    {'contentType':'auxiliaryInformation',
                        'description':'The starting cycle number associated with the data contained within this granule. The cycle number is the counter of the number of 91-day repeat cycles completed by the mission.',
                        'long_name':'Starting Cycle',
                        'source':'Derived',
                        'units':'1'})
        add_variable(ancillary_group, "start_delta_time",    granule["start_delta_time"],    'float64',
                    {'contentType':'auxiliaryInformation',
                        'description':'Number of GPS seconds since the ATLAS SDP epoch at the first data point in the file. The ATLAS Standard Data Products (SDP) epoch offset is defined within /ancillary_data/atlas_sdp_gps_epoch as the number of GPS seconds between the GPS epoch (1980-01-06T00:00:00.000000Z UTC) and the ATLAS SDP epoch. By adding the offset contained within atlas_sdp_gps_epoch to delta time parameters, the time in gps_seconds relative to the GPS epoch can be computed.',
                        'long_name':'ATLAS Start Time (Actual)',
                        'source':'Derived',
                        'units':'seconds since 2018-01-01'})
        add_variable(ancillary_group, "start_geoseg",        granule["start_geoseg"],        'int32',
                    {'contentType':'auxiliaryInformation',
                        'description':'The starting geolocation segment number associated with the data contained within this granule. ICESat granule geographic regions are further refined by geolocation segments. During the geolocation process, a geolocation segment is created approximately every 20m from the start of the orbit to the end.  The geolocation segments help align the ATLAS strong a weak beams and provide a common segment length for the L2 and higher products. The geolocation segment indices differ slightly from orbit-to-orbit because of the irregular shape of the Earth. The geolocation segment indices on ATL01 and ATL02 are only approximate because beams have not been aligned at the time of their creation.',
                        'long_name':'Starting Geolocation Segment',
                        'source':'Derived',
                        'units':'1'})
        add_variable(ancillary_group, "start_gpssow",        granule["start_gpssow"],        'float64',
                    {'contentType':'auxiliaryInformation',
                        'description':'GPS seconds-of-week of the first data point in the granule.',
                        'long_name':'Start GPS SOW of Granule (Actual)',
                        'source':'Derived',
                        'units':'seconds'})
        add_variable(ancillary_group, "start_gpsweek",       granule["start_gpsweek"],       'int32',
                    {'contentType':'auxiliaryInformation',
                        'description':'GPS week number of the first data point in the granule.',
                        'long_name':'Start GPSWeek of Granule (Actual)',
                        'source':'Derived',
                        'units':'weeks from 1980-01-06'})
        add_variable(ancillary_group, "start_orbit",         rqst_parms["orbit"],         'int32',
                    {'contentType':'auxiliaryInformation',
                        'description':'The starting orbit number associated with the data contained within this granule. The orbit number increments each time the spacecraft completes a full orbit of the Earth.',
                        'long_name':'Starting Orbit Number',
                        'source':'Derived',
                        'units':'1'})
        add_variable(ancillary_group, "start_region",         rqst_parms["region"],       'int32',
                    {'contentType':'auxiliaryInformation',
                        'description':'The starting product-specific region number associated with the data contained within this granule. ICESat-2 data products are separated by geographic regions. The data contained within a specific region are the same for ATL01 and ATL02. ATL03 regions differ slightly because of different geolocation segment locations caused by the irregular shape of the Earth. The region indices for other products are completely independent.',
                        'long_name':'Starting Region',
                        'source':'Derived',
                        'units':'1'})
        add_variable(ancillary_group, "start_rgt",         rqst_parms["rgt"],             'int32',
                    {'contentType':'auxiliaryInformation',
                        'description':'The starting reference groundtrack (RGT) number associated with the data contained within this granule. There are 1387 reference groundtrack in the ICESat-2 repeat orbit. The reference groundtrack increments each time the spacecraft completes a full orbit of the Earth and resets to 1 each time the spacecraft completes a full cycle.',
                        'long_name':'Starting Reference Groundtrack',
                        'source':'Derived',
                        'units':'1'})
        add_variable(ancillary_group, "version",             granule["version"],             h5py.string_dtype(encoding='utf-8'),
                    {'contentType':'auxiliaryInformation',
                        'description':'Version number of this granule within the release. It is a sequential number corresponding to the number of times the granule has been reprocessed for the current release.',
                        'long_name':'Version',
                        'source':'Operations',
                        'units':'1'})

        # orbit info
        orbit_group = hf.create_group("orbit_info")
        add_variable(orbit_group, "crossing_time",         granule["crossing_time"],         'float64',
                    {'contentType':'referenceInformation',
                        'description':'The time, in seconds since the ATLAS SDP GPS Epoch, at which the ascending node crosses the equator. The ATLAS Standard Data Products (SDP) epoch offset is defined within /ancillary_data/atlas_sdp_gps_epoch as the number of GPS seconds between the GPS epoch (1980-01-06T00:00:00.000000Z UTC) and the ATLAS SDP epoch. By adding the offset contained within atlas_sdp_gps_epoch to delta time parameters, the time in gps_seconds relative to the GPS epoch can be computed.',
                        'long_name':'Ascending Node Crossing Time',
                        'source':'POD/PPD',
                        'units':'seconds since 2018-01-01',
                        'standard_name': 'time'})
        add_variable(orbit_group, "cycle_number",          rqst_parms["cycle"],          'int8',
                    {'contentType':'referenceInformation',
                        'description':'Tracks the number of 91-day cycles in the mission, beginning with 01.  A unique orbit number can be determined by subtracting 1 from the cycle_number, multiplying by 1387 and adding the rgt value.',
                        'long_name':'Cycle Number',
                        'source':'POD/PPD',
                        'units':'counts'})
        add_variable(orbit_group, "lan",                   granule["lan"],                   'float64',
                    {'contentType':'referenceInformation',
                        'description':'Longitude at the ascending node crossing.',
                        'long_name':'Ascending Node Longitude',
                        'source':'POD/PPD',
                        'units':'degrees_east'})
        add_variable(orbit_group, "orbit_number",          granule["orbit_number"],          'int16',
                    {'contentType':'referenceInformation',
                        'description':'Unique identifying number for each planned ICESat-2 orbit.',
                        'long_name':'Orbit Number',
                        'source':'Operations',
                        'units':'1'})
        add_variable(orbit_group, "rgt",                   rqst_parms["rgt"],                   'int16',
                    {'contentType':'referenceInformation',
                        'description':'The reference ground track (RGT) is the track on the earth at which a specified unit vector within the observatory is pointed. Under nominal operating conditions, there will be no data collected along the RGT, as the RGT is spanned by GT2L and GT2R.  During slews or off-pointing, it is possible that ground tracks may intersect the RGT. The ICESat-2 mission has 1387 RGTs.',
                        'long_name':'Reference Ground Track',
                        'source':'POD/PPD',
                        'units':'counts'})
        add_variable(orbit_group, "sc_orient",             granule["sc_orient"],             'int8',
                    {'contentType':'referenceInformation',
                        'description':'This parameter tracks the spacecraft orientation between forward, backward and transitional flight modes. ICESat-2 is considered to be flying forward when the weak beams are leading the strong beams; and backward when the strong beams are leading the weak beams. ICESat-2 is considered to be in transition while it is maneuvering between the two orientations. Science quality is potentially degraded while in transition mode.',
                        'long_name':'Spacecraft Orientation',
                        'source':'POD/PPD',
                        'units':'1',
                        'flag_meanings':'backward forward transition',
                        'flag_values': [0, 1, 2]})
        add_variable(orbit_group, "sc_orient_time",        granule["sc_orient_time"],        'float64',
                    {'contentType':'referenceInformation',
                        'description':'The time of the last spacecraft orientation change between forward, backward and transitional flight modes, expressed in seconds since the ATLAS SDP GPS Epoch. ICESat-2 is considered to be flying forward when the weak beams are leading the strong beams; and backward when the strong beams are leading the weak beams. ICESat-2 is considered to be in transition while it is maneuvering between the two orientations. Science quality is potentially degraded while in transition mode. The ATLAS Standard Data Products (SDP) epoch offset is defined within /ancillary_data/atlas_sdp_gps_epoch as the number of GPS seconds between the GPS epoch (1980-01-06T00:00:00.000000Z UTC) and the ATLAS SDP epoch. By adding the offset contained within atlas_sdp_gps_epoch to delta time parameters, the time in gps_seconds relative to the GPS epoch can be computed.',
                        'long_name':'Time of Last Spacecraft Orientation Change',
                        'source':'POD/PPD',
                        'units':'seconds since 2018-01-01',
                        'standard_name':'time'})

        # spots
        for beam in beam_table:
            df = beam_table[beam]
            df["delta_time"] = (df["time_ns"] / 1000000000.0) - ATLAS_GPS_EPOCH

            beam_group = hf.create_group(beam) # e.g. gt1r, gt2l, etc.
            add_variable(beam_group, "index_ph",   df['index_ph'],     'int32',
                        {'contentType':'physicalMeasurement',
                         'description':'0-based index of the photon in the ATL03 heights group',
                         'long_name':'Photon index',
                         'source':'ATL03',
                         'units':'scalar'})
            add_variable(beam_group, "index_seg",  df["index_seg"],    'int32',
                        {'contentType':'physicalMeasurement',
                         'description':'0-based index of the photon in the ATL03 geolocation group',
                         'long_name':'Segment index',
                         'source':'ATL03',
                         'units':'scalar'})
            add_variable(beam_group, "delta_time", df["delta_time"],   'float64',
                        {'contentType':'physicalMeasurement',
                         'description':'The transmit time of a given photon, measured in seconds from the ATLAS Standard Data Product Epoch. Note that multiple received photons associated with a single transmit pulse will have the same delta_time. The ATLAS Standard Data Products (SDP) epoch offset is defined within /ancillary_data/atlas_sdp_gps_epoch as the number of GPS seconds between the GPS epoch (1980-01-06T00:00:00.000000Z UTC) and the ATLAS SDP epoch. By adding the offset contained within atlas_sdp_gps_epoch to delta time parameters, the time in gps_seconds relative to the GPS epoch can be computed.',
                         'long_name':'Elapsed GPS seconds',
                         'source':'ATL03',
                         'units':'seconds since 2018-01-01'})
            add_variable(beam_group, "lat_ph",   df["lat_ph"],     'float64',
                        {'contentType':'modelResult',
                         'description':'Latitude of each received photon. Computed from the ECF Cartesian coordinates of the bounce point.',
                         'long_name':'Latitude',
                         'source':'ATL03',
                         'units':'degrees_north',
                         'standard_name':'latitude',
                         'valid_max': 90.0,
                         'valid_min': -90.0})
            add_variable(beam_group, "lon_ph",  df["lon_ph"],    'float64',
                        {'contentType':'modelResult',
                         'description':'Longitude of each received photon. Computed from the ECF Cartesian coordinates of the bounce point.',
                         'long_name':'Longitude',
                         'source':'ATL03',
                         'units':'degrees_east',
                         'standard_name': 'longitude',
                         'valid_max': 180.0,
                         'valid_min': -180.0})
            add_variable(beam_group, "x_atc",      df["x_atc"],        'float32',
                        {'contentType':'modelResult',
                         'description':'Along-track distance in a segment projected to the ellipsoid of the received photon, based on the Along-Track Segment algorithm.  Total along track distance can be found by adding this value to the sum of segment lengths measured from the start of the most recent reference groundtrack.',
                         'long_name':'Distance from equator crossing',
                         'source':'ATL03',
                         'units':'meters'})
            add_variable(beam_group, "y_atc",      df["y_atc"],        'float32',
                        {'contentType':'modelResult',
                         'description':'Across-track distance projected to the ellipsoid of the received photon from the reference ground track.  This is based on the Along-Track Segment algorithm described in Section 3.1.',
                         'long_name':'Distance off RGT',
                         'source':'ATL03',
                         'units':'meters'})
            add_variable(beam_group, "ellipse_h",  df["ellipse_h"],    'float32',
                        {'contentType':'physicalMeasurement',
                         'description':'Height of each received photon, relative to the WGS-84 ellipsoid including refraction correction. Note neither the geoid, ocean tide nor the dynamic atmosphere (DAC) corrections are applied to the ellipsoidal heights.',
                         'long_name':'Photon WGS84 height',
                         'source':'ATL03',
                         'units':'meters'})
            add_variable(beam_group, "ortho_h",    df["ortho_h"], 'float32',
                        {'contentType':'physicalMeasurement',
                         'description':'Height of each received photon, relative to the geoid.',
                         'long_name':'Orthometric height',
                         'source':'ATL03',
                         'units':'meters'})
            add_variable(beam_group, "depth",      df["depth"],        'float32',
                        {'contentType':'modelResult',
                         'description':'Depth of the received photon below the sea surface',
                         'long_name':'Depth',
                         'source':'ATL03',
                         'units':'meters'})
            add_variable(beam_group, "sigma_thu",  df["sigma_thu"],    'float32',
                        {'contentType':'physicalMeasurement',
                         'description':'The combination of the aerial and subaqueous horizontal uncertainty for each received photon',
                         'long_name':'Total horizontal uncertainty',
                         'source':'ATL03',
                         'units':'meters'})
            add_variable(beam_group, "sigma_tvu",  df["sigma_tvu"],    'float32',
                        {'contentType':'modelResult',
                         'description':'The combination of the aerial and subaqueous vertical uncertainty for each received photon',
                         'long_name':'Total vertical uncertainty',
                         'source':'ATL03',
                         'units':'meters'})
            add_variable(beam_group, "flags",       df["flags"],       'int32',
                        {'contentType':'modelResult',
                         'description':'bit 0 - max sensor depth exceeded',
                         'long_name':'Processing flags',
                         'source':'ATL03',
                         'units':'bit mask'})
            add_variable(beam_group, "class_ph", df["ensemble"].astype(np.int16), 'int16',
                        {'contentType':'modelResult',
                         'description':'0 - unclassified, 1 - other, 40 - bathymetry, 41 - sea surface',
                         'long_name':'Photon classification',
                         'source':'ATL03',
                         'units':'scalar'})

    print("HDF5 file written: " + settings["filename"])
