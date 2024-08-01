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

import geopandas as gpd
import pandas as pd
import numpy as np
import h5py
import sys
import json

import warnings
warnings.simplefilter(action='ignore', category=FutureWarning)

# constants
ATLAS_GPS_EPOCH = 1198800018
RELEASE = 1

# check command line parameters
if len(sys.argv) <= 1:
    print("Not enough parameters: python runner.py <control json file> [<ancillary json file> <orbit json file>]")
    sys.exit()

# read control json
control_json = sys.argv[1]
with open(control_json, 'r') as json_file:
    control = json.load(json_file)

# get inputs from control
input_files = control["input_files"]
output_parms = control["output_parms"]
atl24_filename = control["atl24_filename"]
version = control["version"]
commit = control["commit"]
environment = control["environment"]
resource = control["resource"]

# read in data
spot_table = {}

# read in photons as dataframe from csv file
for spot in input_files["spot_photons"]:
    if spot not in spot_table:
        spot_table[spot] = {}
    spot_table[spot]["df"] = pd.read_csv(input_files["spot_photons"][spot], engine='pyarrow')
print("Read photon data into dataframes")

# read in granule info as dictionary from json file and add as necessary to dataframes
for spot in input_files["spot_granule"]:
    spot_table[spot]["info"] = json.load(open(input_files["spot_granule"][spot], 'r'))
    spot_table[spot]["df"]["spot"] = spot_table[spot]["info"]["spot"] # add spot info to dataframe

# read in photon classifications as dataframe from csv file and merge into dataframes
for classifier in input_files["classifiers"]:
    for spot in input_files["classifiers"][classifier]:
        classifier_df = pd.read_csv(input_files["classifiers"][classifier][spot], engine='pyarrow')
        classifier_df.rename(columns={'class_ph': classifier}, inplace=True)
        spot_table[spot]["df"] = pd.merge(spot_table[spot]["df"], classifier_df, on='index_ph', how='left')
print("Added photon classifications to dataframes")

# concatenate (vertically) all dataframes
spot_dfs = [spot_table[spot]["df"] for spot in spot_table]
df = pd.concat(spot_dfs)
print("Concatenated data frames into a single data frame")

# Metadata Output 
metadata = {spot: spot_table[spot]["info"] for spot in spot_table}
metadata["profile"] = control["profile"]
metadata["granule"] = {
    "total_photons": len(df),
    "sea_surface_photons": len(df[df["class_ph"] == 41]),
    "bathy_photons": len(df[df["class_ph"] == 40]),
    "subaqueous_photons": len(df[df["ortho_h"] < df["surface_h"]])
}
metadata["version"] = version
metadata["commit"] = commit
metadata["environment"] = environment
metadata["resource"] = resource
with open(atl24_filename + ".json", "w") as file:
    file.write(json.dumps(metadata, indent=2))

# Photon Output (Non-HDF5)
if output_parms["format"] != "hdf5" and output_parms["format"] != "h5":

    # write output
    if output_parms["format"] == "csv":
        
        # CSV
        df.to_csv(atl24_filename, index=False)
        print("CSV file written: " + atl24_filename)
    
    elif output_parms["format"] == "parquet" or output_parms["format"] == "geoparquet":

        if output_parms["as_geo"] or output_parms["format"] == "geoparquet":
    
            # GeoParquet
            df['time'] = df['time'].astype('datetime64[ns]')
            geometry = gpd.points_from_xy(df['lon_ph'], df['lat_ph'])
            df.drop(columns=['lon_ph', 'lat_ph'], inplace=True)
            gdf = gpd.GeoDataFrame(df, geometry=geometry, crs="EPSG:7912")
            gdf.set_index('time', inplace=True)
            gdf.sort_index(inplace=True)
            gdf.to_parquet(atl24_filename, index=None)
            print("Writing GeoParquet file: " + atl24_filename)

        else:

            # Parquet
            import pyarrow as pa
            import pyarrow.parquet as pq
            table = pa.Table.from_pandas(df, preserve_index=False)
            schema = table.schema.with_metadata({"sliderule": json.dumps(metadata)})
            table = table.replace_schema_metadata(schema.metadata)
            pq.write_table(table, atl24_filename)
            print("Writing Parquet file: " + atl24_filename)

# Photon Output (HDF5)
else:

    def add_variable(group, name, data, dtype, attrs):
        dataset = group.create_dataset(name, data=data, dtype=dtype)
        for key in attrs:
            dataset.attrs[key] = attrs[key]

    with h5py.File(atl24_filename, 'w') as hf:

        if len(sys.argv) > 3:

            # ancillary data
            ancillary_json = sys.argv[2]
            with open(ancillary_json, 'r') as json_file:
                ancillary = json.load(json_file)
            ancillary_group = hf.create_group("ancillary_data")
            add_variable(ancillary_group, "atlas_sdp_gps_epoch", ancillary["atlas_sdp_gps_epoch"], 'float64',
                        {'contentType':'auxiliaryInformation', 
                         'description':'Number of GPS seconds between the GPS epoch (1980-01-06T00:00:00.000000Z UTC) and the ATLAS Standard Data Product (SDP) epoch (2018-01-01:T00.00.00.000000 UTC). Add this value to delta time parameters to compute full gps_seconds (relative to the GPS epoch) for each data point.', 
                         'long_name':'ATLAS Epoch Offset', 
                         'source':'Operations', 
                         'units':'seconds since 1980-01-06T00:00:00.000000Z'})
            add_variable(ancillary_group, "data_end_utc",        ancillary["data_end_utc"],        h5py.string_dtype(encoding='utf-8'),
                        {'contentType':'auxiliaryInformation', 
                         'description':'UTC (in CCSDS-A format) of the last data point within the granule.', 
                         'long_name':'End UTC Time of Granule (CCSDS-A, Actual)', 
                         'source':'Derived', 
                         'units':'1'})
            add_variable(ancillary_group, "data_start_utc",      ancillary["data_start_utc"],      h5py.string_dtype(encoding='utf-8'),
                        {'contentType':'auxiliaryInformation', 
                         'description':'UTC (in CCSDS-A format) of the first data point within the granule.', 
                         'long_name':'Start UTC Time of Granule (CCSDS-A, Actual)', 
                         'source':'Derived', 
                         'units':'1'})
            add_variable(ancillary_group, "end_cycle",           ancillary["end_cycle"],           'int32',
                        {'contentType':'auxiliaryInformation', 
                         'description':'The ending cycle number associated with the data contained within this granule. The cycle number is the counter of the number of 91-day repeat cycles completed by the mission.', 
                         'long_name':'Ending Cycle', 
                         'source':'Derived', 
                         'units':'1'})
            add_variable(ancillary_group, "end_delta_time",      ancillary["end_delta_time"],      'float64',
                        {'contentType':'auxiliaryInformation', 
                         'description':'Number of GPS seconds since the ATLAS SDP epoch at the last data point in the file. The ATLAS Standard Data Products (SDP) epoch offset is defined within /ancillary_data/atlas_sdp_gps_epoch as the number of GPS seconds between the GPS epoch (1980-01-06T00:00:00.000000Z UTC) and the ATLAS SDP epoch. By adding the offset contained within atlas_sdp_gps_epoch to delta time parameters, the time in gps_seconds relative to the GPS epoch can be computed.', 
                         'long_name':'ATLAS End Time (Actual)', 
                         'source':'Derived', 
                         'units':'seconds since 2018-01-01',
                         'standard_name': 'time'})
            add_variable(ancillary_group, "end_geoseg",          ancillary["end_geoseg"],          'int32',
                        {'contentType':'auxiliaryInformation', 
                         'description':'The ending geolocation segment number associated with the data contained within this granule. ICESat granule geographic regions are further refined by geolocation segments. During the geolocation process, a geolocation segment is created approximately every 20m from the start of the orbit to the end.  The geolocation segments help align the ATLAS strong a weak beams and provide a common segment length for the L2 and higher products. The geolocation segment indices differ slightly from orbit-to-orbit because of the irregular shape of the Earth. The geolocation segment indices on ATL01 and ATL02 are only approximate because beams have not been aligned at the time of their creation.', 
                         'long_name':'Ending Geolocation Segment', 
                         'source':'Derived', 
                         'units':'1'})
            add_variable(ancillary_group, "end_gpssow",          ancillary["end_gpssow"],          'float64',
                        {'contentType':'auxiliaryInformation', 
                         'description':'GPS seconds-of-week of the last data point in the granule.', 
                         'long_name':'Ending GPS SOW of Granule (Actual)', 
                         'source':'Derived', 
                         'units':'seconds'})
            add_variable(ancillary_group, "end_gpsweek",         ancillary["end_gpsweek"],         'int32',
                        {'contentType':'auxiliaryInformation', 
                         'description':'GPS week number of the last data point in the granule.', 
                         'long_name':'Ending GPSWeek of Granule (Actual)', 
                         'source':'Derived', 
                         'units':'weeks from 1980-01-06'})
            add_variable(ancillary_group, "end_orbit",           ancillary["end_orbit"],           'int32',
                        {'contentType':'auxiliaryInformation', 
                         'description':'The ending orbit number associated with the data contained within this granule. The orbit number increments each time the spacecraft completes a full orbit of the Earth.', 
                         'long_name':'Ending Orbit Number', 
                         'source':'Derived', 
                         'units':'1'})
            add_variable(ancillary_group, "end_region",          ancillary["end_region"],          'int32',
                        {'contentType':'auxiliaryInformation', 
                         'description':'The ending product-specific region number associated with the data contained within this granule. ICESat-2 data products are separated by geographic regions. The data contained within a specific region are the same for ATL01 and ATL02. ATL03 regions differ slightly because of different geolocation segment locations caused by the irregular shape of the Earth. The region indices for other products are completely independent.', 
                         'long_name':'Ending Region',
                         'source':'Derived',
                         'units':'1'})
            add_variable(ancillary_group, "end_rgt",             ancillary["end_rgt"],             'int32',
                        {'contentType':'auxiliaryInformation', 
                         'description':'The ending reference groundtrack (RGT) number associated with the data contained within this granule. There are 1387 reference groundtrack in the ICESat-2 repeat orbit. The reference groundtrack increments each time the spacecraft completes a full orbit of the Earth and resets to 1 each time the spacecraft completes a full cycle.', 
                         'long_name':'Ending Reference Groundtrack', 
                         'source':'Derived', 
                         'units':'1'})
            add_variable(ancillary_group, "granule_end_utc",     ancillary["granule_end_utc"],     h5py.string_dtype(encoding='utf-8'),
                        {'contentType':'auxiliaryInformation', 
                         'description':'Requested end time (in UTC CCSDS-A) of this granule.', 
                         'long_name':'End UTC Time of Granule (CCSDS-A, Requested)', 
                         'source':'Derived', 
                         'units':'1'})
            add_variable(ancillary_group, "granule_start_utc",   ancillary["granule_start_utc"],   h5py.string_dtype(encoding='utf-8'),
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
            add_variable(ancillary_group, "resource",            resource,                         h5py.string_dtype(encoding='utf-8'),
                        {'contentType':'auxiliaryInformation', 
                         'description':'ATL03 granule used to produce this granule', 
                         'long_name':'ATL03 Resource', 
                         'source':'Operations', 
                         'units':'1'})
            add_variable(ancillary_group, "sliderule_version",   version,                          h5py.string_dtype(encoding='utf-8'),
                        {'contentType':'auxiliaryInformation', 
                         'description':'Version of SlideRule software used to generate this granule', 
                         'long_name':'SlideRule Version', 
                         'source':'Operations', 
                         'units':'1'})
            add_variable(ancillary_group, "sliderule_commit",    commit,                           h5py.string_dtype(encoding='utf-8'),
                        {'contentType':'auxiliaryInformation', 
                         'description':'Git commit ID (https://github.com/SlideRuleEarth/sliderule.git) of SlideRule software used to generate this granule', 
                         'long_name':'SlideRule Commit', 
                         'source':'Operations', 
                         'units':'1'})
            add_variable(ancillary_group, "sliderule_environment",   environment,                  h5py.string_dtype(encoding='utf-8'),
                        {'contentType':'auxiliaryInformation', 
                         'description':'Git commit ID (https://github.com/SlideRuleEarth/sliderule.git) of SlideRule environment used to generate this granule', 
                         'long_name':'SlideRule Environment', 
                         'source':'Operations', 
                         'units':'1'})
            add_variable(ancillary_group, "start_cycle",         ancillary["start_cycle"],         'int32',
                        {'contentType':'auxiliaryInformation', 
                         'description':'The starting cycle number associated with the data contained within this granule. The cycle number is the counter of the number of 91-day repeat cycles completed by the mission.', 
                         'long_name':'Starting Cycle', 
                         'source':'Derived', 
                         'units':'1'})
            add_variable(ancillary_group, "start_delta_time",    ancillary["start_delta_time"],    'float64',
                        {'contentType':'auxiliaryInformation', 
                         'description':'Number of GPS seconds since the ATLAS SDP epoch at the first data point in the file. The ATLAS Standard Data Products (SDP) epoch offset is defined within /ancillary_data/atlas_sdp_gps_epoch as the number of GPS seconds between the GPS epoch (1980-01-06T00:00:00.000000Z UTC) and the ATLAS SDP epoch. By adding the offset contained within atlas_sdp_gps_epoch to delta time parameters, the time in gps_seconds relative to the GPS epoch can be computed.', 
                         'long_name':'ATLAS Start Time (Actual)',
                         'source':'Derived', 
                         'units':'seconds since 2018-01-01'})
            add_variable(ancillary_group, "start_geoseg",        ancillary["start_geoseg"],        'int32',
                        {'contentType':'auxiliaryInformation', 
                         'description':'The starting geolocation segment number associated with the data contained within this granule. ICESat granule geographic regions are further refined by geolocation segments. During the geolocation process, a geolocation segment is created approximately every 20m from the start of the orbit to the end.  The geolocation segments help align the ATLAS strong a weak beams and provide a common segment length for the L2 and higher products. The geolocation segment indices differ slightly from orbit-to-orbit because of the irregular shape of the Earth. The geolocation segment indices on ATL01 and ATL02 are only approximate because beams have not been aligned at the time of their creation.', 
                         'long_name':'Starting Geolocation Segment', 
                         'source':'Derived', 
                         'units':'1'})
            add_variable(ancillary_group, "start_gpssow",        ancillary["start_gpssow"],        'float64',
                        {'contentType':'auxiliaryInformation', 
                         'description':'GPS seconds-of-week of the first data point in the granule.', 
                         'long_name':'Start GPS SOW of Granule (Actual)', 
                         'source':'Derived', 
                         'units':'seconds'})
            add_variable(ancillary_group, "start_gpsweek",       ancillary["start_gpsweek"],       'int32',
                        {'contentType':'auxiliaryInformation', 
                         'description':'GPS week number of the first data point in the granule.', 
                         'long_name':'Start GPSWeek of Granule (Actual)', 
                         'source':'Derived', 
                         'units':'weeks from 1980-01-06'})
            add_variable(ancillary_group, "start_orbit",         ancillary["start_orbit"],         'int32',
                        {'contentType':'auxiliaryInformation', 
                         'description':'The starting orbit number associated with the data contained within this granule. The orbit number increments each time the spacecraft completes a full orbit of the Earth.', 
                         'long_name':'Starting Orbit Number', 
                         'source':'Derived', 
                         'units':'1'})
            add_variable(ancillary_group, "start_region",         ancillary["start_region"],       'int32',
                        {'contentType':'auxiliaryInformation', 
                         'description':'The starting product-specific region number associated with the data contained within this granule. ICESat-2 data products are separated by geographic regions. The data contained within a specific region are the same for ATL01 and ATL02. ATL03 regions differ slightly because of different geolocation segment locations caused by the irregular shape of the Earth. The region indices for other products are completely independent.', 
                         'long_name':'Starting Region', 
                         'source':'Derived', 
                         'units':'1'})
            add_variable(ancillary_group, "start_rgt",         ancillary["start_rgt"],             'int32',
                        {'contentType':'auxiliaryInformation', 
                         'description':'The starting reference groundtrack (RGT) number associated with the data contained within this granule. There are 1387 reference groundtrack in the ICESat-2 repeat orbit. The reference groundtrack increments each time the spacecraft completes a full orbit of the Earth and resets to 1 each time the spacecraft completes a full cycle.', 
                         'long_name':'Starting Reference Groundtrack', 
                         'source':'Derived', 
                         'units':'1'})
            add_variable(ancillary_group, "version",             ancillary["version"],             h5py.string_dtype(encoding='utf-8'),
                        {'contentType':'auxiliaryInformation', 
                         'description':'Version number of this granule within the release. It is a sequential number corresponding to the number of times the granule has been reprocessed for the current release.', 
                         'long_name':'Version', 
                         'source':'Operations', 
                         'units':'1'})

            # orbit info
            orbit_json = sys.argv[3]
            with open(orbit_json, 'r') as json_file:
                orbit = json.load(json_file)
            orbit_group = hf.create_group("orbit_info")
            add_variable(orbit_group, "crossing_time",         orbit["crossing_time"],         'float64',
                        {'contentType':'referenceInformation', 
                         'description':'The time, in seconds since the ATLAS SDP GPS Epoch, at which the ascending node crosses the equator. The ATLAS Standard Data Products (SDP) epoch offset is defined within /ancillary_data/atlas_sdp_gps_epoch as the number of GPS seconds between the GPS epoch (1980-01-06T00:00:00.000000Z UTC) and the ATLAS SDP epoch. By adding the offset contained within atlas_sdp_gps_epoch to delta time parameters, the time in gps_seconds relative to the GPS epoch can be computed.', 
                         'long_name':'Ascending Node Crossing Time', 
                         'source':'POD/PPD', 
                         'units':'seconds since 2018-01-01',
                         'standard_name': 'time'})
            add_variable(orbit_group, "cycle_number",          orbit["cycle_number"],          'int8',
                        {'contentType':'referenceInformation', 
                         'description':'Tracks the number of 91-day cycles in the mission, beginning with 01.  A unique orbit number can be determined by subtracting 1 from the cycle_number, multiplying by 1387 and adding the rgt value.', 
                         'long_name':'Cycle Number', 
                         'source':'POD/PPD', 
                         'units':'counts'})
            add_variable(orbit_group, "lan",                   orbit["lan"],                   'float64',
                        {'contentType':'referenceInformation', 
                         'description':'Longitude at the ascending node crossing.', 
                         'long_name':'Ascending Node Longitude', 
                         'source':'POD/PPD', 
                         'units':'degrees_east'})
            add_variable(orbit_group, "orbit_number",          orbit["orbit_number"],          'int16',
                        {'contentType':'referenceInformation', 
                         'description':'Unique identifying number for each planned ICESat-2 orbit.', 
                         'long_name':'Orbit Number', 
                         'source':'Operations', 
                         'units':'1'})
            add_variable(orbit_group, "rgt",                   orbit["rgt"],                   'int16',
                        {'contentType':'referenceInformation', 
                         'description':'The reference ground track (RGT) is the track on the earth at which a specified unit vector within the observatory is pointed. Under nominal operating conditions, there will be no data collected along the RGT, as the RGT is spanned by GT2L and GT2R.  During slews or off-pointing, it is possible that ground tracks may intersect the RGT. The ICESat-2 mission has 1387 RGTs.', 
                         'long_name':'Reference Ground Track', 
                         'source':'POD/PPD', 
                         'units':'counts'})
            add_variable(orbit_group, "sc_orient",             orbit["sc_orient"],             'int8',
                        {'contentType':'referenceInformation', 
                         'description':'This parameter tracks the spacecraft orientation between forward, backward and transitional flight modes. ICESat-2 is considered to be flying forward when the weak beams are leading the strong beams; and backward when the strong beams are leading the weak beams. ICESat-2 is considered to be in transition while it is maneuvering between the two orientations. Science quality is potentially degraded while in transition mode.', 
                         'long_name':'Spacecraft Orientation', 
                         'source':'POD/PPD', 
                         'units':'1',
                         'flag_meanings':'backward forward transition', 
                         'flag_values': [0, 1, 2]})
            add_variable(orbit_group, "sc_orient_time",        orbit["sc_orient_time"],        'float64',
                        {'contentType':'referenceInformation', 
                         'description':'The time of the last spacecraft orientation change between forward, backward and transitional flight modes, expressed in seconds since the ATLAS SDP GPS Epoch. ICESat-2 is considered to be flying forward when the weak beams are leading the strong beams; and backward when the strong beams are leading the weak beams. ICESat-2 is considered to be in transition while it is maneuvering between the two orientations. Science quality is potentially degraded while in transition mode. The ATLAS Standard Data Products (SDP) epoch offset is defined within /ancillary_data/atlas_sdp_gps_epoch as the number of GPS seconds between the GPS epoch (1980-01-06T00:00:00.000000Z UTC) and the ATLAS SDP epoch. By adding the offset contained within atlas_sdp_gps_epoch to delta time parameters, the time in gps_seconds relative to the GPS epoch can be computed.', 
                         'long_name':'Time of Last Spacecraft Orientation Change', 
                         'source':'POD/PPD', 
                         'units':'seconds since 2018-01-01',
                         'standard_name':'time'})

        # spots
        for spot in spot_table:
            spot_info = spot_table[spot]["info"]
            spot_df = spot_table[spot]["df"]
            spot_df["delta_time"] = (spot_df["time"] / 1000000000.0) - ATLAS_GPS_EPOCH

            # apply refraction correction
            spot_df["ellipse_h"] += spot_df["delta_h"]
            spot_df["ortho_h"] += spot_df["delta_h"]

            # calculate depth
            spot_df["depth"] = 0.0
            subaqueous = spot_df["ortho_h"] < spot_df["surface_h"]
            spot_df.loc[subaqueous, 'depth'] = spot_df.loc[subaqueous, 'surface_h'] - spot_df.loc[subaqueous, 'ortho_h']

            beam_group = hf.create_group(spot_info["beam"]) # e.g. gt1r, gt2l, etc.
            add_variable(beam_group, "index_ph",   spot_df["index_ph"],     'float32',
                        {'contentType':'physicalMeasurement', 
                         'description':'0-based index of the photon in the ATL03 heights group', 
                         'long_name':'Photon index', 
                         'source':'ATL03', 
                         'units':'scalar'})
            add_variable(beam_group, "index_seg",  spot_df["index_seg"],    'int32',
                        {'contentType':'physicalMeasurement', 
                         'description':'0-based index of the photon in the ATL03 geolocation group', 
                         'long_name':'Segment index', 
                         'source':'ATL03', 
                         'units':'scalar'})
            add_variable(beam_group, "delta_time", spot_df["delta_time"],   'float64',
                        {'contentType':'physicalMeasurement', 
                         'description':'The transmit time of a given photon, measured in seconds from the ATLAS Standard Data Product Epoch. Note that multiple received photons associated with a single transmit pulse will have the same delta_time. The ATLAS Standard Data Products (SDP) epoch offset is defined within /ancillary_data/atlas_sdp_gps_epoch as the number of GPS seconds between the GPS epoch (1980-01-06T00:00:00.000000Z UTC) and the ATLAS SDP epoch. By adding the offset contained within atlas_sdp_gps_epoch to delta time parameters, the time in gps_seconds relative to the GPS epoch can be computed.', 
                         'long_name':'Elapsed GPS seconds', 
                         'source':'ATL03', 
                         'units':'seconds since 2018-01-01'})
            add_variable(beam_group, "lat_ph",   spot_df["lat_ph"],     'float64',
                        {'contentType':'modelResult', 
                         'description':'Latitude of each received photon. Computed from the ECF Cartesian coordinates of the bounce point.', 
                         'long_name':'Latitude', 
                         'source':'ATL03', 
                         'units':'degrees_north',
                         'standard_name':'latitude',
                         'valid_max': 90.0,
                         'valid_min': -90.0})
            add_variable(beam_group, "lon_ph",  spot_df["lon_ph"],    'float64',
                        {'contentType':'modelResult', 
                         'description':'Longitude of each received photon. Computed from the ECF Cartesian coordinates of the bounce point.', 
                         'long_name':'Longitude', 
                         'source':'ATL03', 
                         'units':'degrees_east',
                         'standard_name': 'longitude',
                         'valid_max': 180.0,
                         'valid_min': -180.0})
            add_variable(beam_group, "x_atc",      spot_df["x_atc"],        'float32',
                        {'contentType':'modelResult', 
                         'description':'Along-track distance in a segment projected to the ellipsoid of the received photon, based on the Along-Track Segment algorithm.  Total along track distance can be found by adding this value to the sum of segment lengths measured from the start of the most recent reference groundtrack.', 
                         'long_name':'Distance from equator crossing', 
                         'source':'ATL03', 
                         'units':'meters'})
            add_variable(beam_group, "y_atc",      spot_df["y_atc"],        'float32',
                        {'contentType':'modelResult', 
                         'description':'Across-track distance projected to the ellipsoid of the received photon from the reference ground track.  This is based on the Along-Track Segment algorithm described in Section 3.1.', 
                         'long_name':'Distance off RGT', 
                         'source':'ATL03', 
                         'units':'meters'})
            add_variable(beam_group, "ellipse_h",  spot_df["ellipse_h"],    'float32',
                        {'contentType':'physicalMeasurement', 
                         'description':'Height of each received photon, relative to the WGS-84 ellipsoid including refraction correction. Note neither the geoid, ocean tide nor the dynamic atmosphere (DAC) corrections are applied to the ellipsoidal heights.', 
                         'long_name':'Photon WGS84 height', 
                         'source':'ATL03', 
                         'units':'meters'})
            add_variable(beam_group, "ortho_h",    spot_df["ortho_h"], 'float32',
                        {'contentType':'physicalMeasurement', 
                         'description':'Height of each received photon, relative to the geoid.', 
                         'long_name':'Orthometric height', 
                         'source':'ATL03', 
                         'units':'meters'})
            add_variable(beam_group, "depth",      spot_df["depth"],        'float32',
                        {'contentType':'modelResult', 
                         'description':'Depth of the received photon below the sea surface', 
                         'long_name':'Depth', 
                         'source':'ATL03', 
                         'units':'meters'})
            add_variable(beam_group, "sigma_thu",  spot_df["sigma_thu"],    'float32',
                        {'contentType':'physicalMeasurement', 
                         'description':'The combination of the aerial and subaqueous horizontal uncertainty for each received photon', 
                         'long_name':'Total horizontal uncertainty', 
                         'source':'ATL03', 
                         'units':'meters'})
            add_variable(beam_group, "sigma_tvu",  spot_df["sigma_tvu"],    'float32',
                        {'contentType':'modelResult', 
                         'description':'The combination of the aerial and subaqueous vertical uncertainty for each received photon', 
                         'long_name':'Total vertical uncertainty', 
                         'source':'ATL03', 
                         'units':'meters'})
            add_variable(beam_group, "flags",       spot_df["flags"],       'int32',
                        {'contentType':'modelResult', 
                         'description':'bit 0 - max sensor depth exceeded', 
                         'long_name':'Processing flags', 
                         'source':'ATL03', 
                         'units':'bit mask'})

            if "ensemble" in spot_df:
                add_variable(beam_group, "class_ph", spot_df["ensemble"].astype(np.int16), 'int16',
                            {'contentType':'modelResult', 
                             'description':'0 - unclassified, 1 - other, 40 - bathymetry, 41 - sea surface', 
                             'long_name':'Photon classification', 
                             'source':'ATL03', 
                             'units':'scalar'})
            else:
                add_variable(beam_group, "class_ph", spot_df["class_ph"].astype(np.int16), 'int16',
                            {'contentType':'modelResult', 
                             'description':'0 - unclassified, 1 - other, 40 - bathymetry, 41 - sea surface', 
                             'long_name':'Photon classification', 
                             'source':'ATL03', 
                             'units':'scalar'})
                for classifier in input_files["classifiers"]:
                    try:
                        add_variable(beam_group, classifier, spot_df[classifier].astype(np.int16), 'int16',
                                    {'contentType':'modelResult', 
                                    'description':'0 - unclassified, 1 - other, 40 - bathymetry, 41 - sea surface', 
                                    'long_name':f'Photon subclassification by {classifier}', 
                                    'source':'ATL03', 
                                    'units':'scalar'})
                    except Exception as e:
                        print(f'Failed to add classifier <{classifier}>: {e}')


    print("HDF5 file written: " + atl24_filename)
