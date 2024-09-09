# Copyright (c) 2024, University of Texas at Austin.
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
# 3. Neither the name of the University of Texas at Austin nor the names of its
#    contributors may be used to endorse or promote products derived from this
#    software without specific prior written permission.
# 
# THIS SOFTWARE IS PROVIDED BY THE UNIVERSITY OF TEXAS AT AUSTIN AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
# TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
# PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE UNIVERSITY OF TEXAS AT AUSTIN OR
# CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
# EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
# PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
# OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
# WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
# OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
# ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

import numpy as np
import pandas as pd
import copy


def bin_data(dataset, lat_res, height_res):
    '''Bin data along vertical and horizontal scales for later segmentation'''
    
    # Calculate number of bins required both vertically and horizontally with resolution size
    lat_bin_number = round(abs(dataset['lat_ph'].min() - dataset['lat_ph'].max())/lat_res)

    height_bin_number = round(abs(dataset['photon_height'].min() - dataset['photon_height'].max())/height_res)
    
    # Duplicate dataframe
    dataset1 = dataset

    pd.options.mode.chained_assignment = None 
    
    # Cut lat bins
    lat_bins = pd.cut(dataset['lat_ph'], lat_bin_number, labels = np.array(range(lat_bin_number)))
    
    # Add bins to dataframe
    dataset1['lat_bins'] = lat_bins

    dataset1['median_sea_surf'] = dataset1.groupby('lat_bins', observed=True)['photon_height'].transform('median')
    dataset1['median_sea_surf'] = dataset1['median_sea_surf'].fillna(dataset1['median_sea_surf'].median())

    
    # Cut height bins
    height_bins = pd.cut(dataset['photon_height'],
                         height_bin_number,
                         labels = np.round(np.linspace(dataset['photon_height'].min(),
                                                       dataset['photon_height'].max(),
                                                       num=height_bin_number), decimals = 1))
    
    # pd.options.mode.chained_assignment = None 
    # Add height bins to dataframe
    dataset1['height_bins'] = height_bins
    dataset1 = dataset1.reset_index(drop=True)

    return dataset1


def get_bath_height(binned_data, percentile, height_resolution, min_photons_per_bin=5):
    """
        Calculates the bathymetry level (depth) for each bin in a 2D grid based on 
        photon counts and a specified percentile threshold.

    """

    # Create sea height list
    bath_height = []
    
    geo_index_ph = []
    geo_temp_ind = []
    geo_med_surf = []
    geo_photon_height = []
    geo_longitude = []
    geo_latitude = []


    global_median_sea_surf = binned_data['median_sea_surf'].median()
    
    # Group data by latitude
    # Filter out surface data that are two bins below median surface value calculated above
    binned_data_bathy_list = []
    for group in binned_data.groupby(['lat_bins'], observed=True):

        med_sea_surf_group = group[1]['median_sea_surf'].median()

        if np.isnan(med_sea_surf_group):
            med_sea_surf_group = global_median_sea_surf

        binned_data_bath = group[1][(group[1]['photon_height'] < med_sea_surf_group - (height_resolution * 2))]
        binned_data_bathy_list.append(binned_data_bath)

    binned_data_bath = pd.concat(binned_data_bathy_list)

    grouped_data = binned_data_bath.groupby(['lat_bins'], group_keys=True, observed=True)
    data_groups = dict(list(grouped_data))

    # Create a percentile threshold of photon counts in each grid, grouped by both x and y axes.
    # count_threshold = np.percentile(binned_data.groupby(['lat_bins',
    #                                                      'height_bins']).size().reset_index().groupby('lat_bins')[[0]].max(),
    #                                 percentile)

    counts_in_bins = []
    # Loop through groups and return average bathy height
    for k,v in data_groups.items():

        new_df = pd.DataFrame(v.groupby('height_bins').count())
        
        if not new_df.empty:

            bath_bin = new_df['lat_ph'].argmax()
            bath_bin_h = new_df.index[bath_bin]

            counts_in_bins.append(new_df.iloc[bath_bin]['lat_ph'])

    counts_in_bins = np.asarray(counts_in_bins)

    cib_thresh_85 = np.percentile(counts_in_bins, 85)
    cib_thresh_user = np.percentile(counts_in_bins, percentile)

    if cib_thresh_85 == cib_thresh_user:

        print('Likely No bathymetry, normal distribution of photons.')
        if cib_thresh_85 <= min_photons_per_bin:
            print('C-Shelph too few photons per bin. Setting min photons to ' + str(min_photons_per_bin) + '.')
            counts_in_bins_thresh = min_photons_per_bin
        else:
            counts_in_bins_thresh = cib_thresh_user

    else: 
        if cib_thresh_user <= min_photons_per_bin:
            print('C-Shelph too few photons per bin. Setting min photons to ' + str(min_photons_per_bin) + '.')
            counts_in_bins_thresh = min_photons_per_bin
        else:
            counts_in_bins_thresh = cib_thresh_user
            print('C-Shelph, using lower thresh.')

    # Loop through groups and return average bathy height
    for k,v in data_groups.items():
        new_df = pd.DataFrame(v.groupby('height_bins').count())

        if not new_df.empty:
            # print('new_df: ', new_df)
            bath_bin = new_df['lat_ph'].argmax()
            bath_bin_h = new_df.index[bath_bin]
            
            # Set threshold of photon counts per bin
            if new_df.iloc[bath_bin]['lat_ph'] >= counts_in_bins_thresh:
                
                geo_photon_height.append(v.loc[v['height_bins']==bath_bin_h, 'photon_height'].values)
                geo_longitude.append(v.loc[v['height_bins']==bath_bin_h, 'lon_ph'].values)
                geo_latitude.append(v.loc[v['height_bins']==bath_bin_h, 'lat_ph'].values)
                geo_index_ph.append(v.loc[v['height_bins']==bath_bin_h, 'index_ph'].values)
                geo_temp_ind.append(v.loc[v['height_bins']==bath_bin_h, 'temp_index'].values)
                geo_med_surf.append(v.loc[v['height_bins']==bath_bin_h, 'median_sea_surf'].values)
                bath_bin_median = v.loc[v['height_bins']==bath_bin_h, 'photon_height'].median()
                bath_height.append(bath_bin_median)
                del new_df
                
            else:
                bath_height.append(np.nan)
                del new_df

    try:
        geo_index_ph_list = np.concatenate(geo_index_ph).ravel().tolist()
        geo_med_surf_list = np.concatenate(geo_med_surf).ravel().tolist()
        geo_temp_ind_list = np.concatenate(geo_temp_ind).ravel().tolist()
        geo_longitude_list = np.concatenate(geo_longitude).ravel().tolist()
        geo_latitude_list = np.concatenate(geo_latitude).ravel().tolist()
        geo_photon_list = np.concatenate(geo_photon_height).ravel().tolist()
        
        # geo_depth = WSHeight - geo_photon_list
        geo_df = pd.DataFrame({'index_ph': geo_index_ph_list, 'PC_index': geo_temp_ind_list,'lon_ph': geo_longitude_list,
                            'lat_ph':geo_latitude_list, 'photon_height': geo_photon_list, 'med_sea_surf': geo_med_surf_list})
    
        del geo_longitude_list, geo_latitude_list, geo_photon_list

        return bath_height, geo_df

    except Exception as c_shelph_err:
        print('c_shelph_err: ', c_shelph_err)
    
        return None, None


def c_shelph_classification(point_cloud, surface_buffer=-0.5,
                            h_res=0.5, lat_res=0.001,
                            thresh=20, min_buffer=-80,
                            max_buffer=5,
                            min_photons_per_bin=6,
                            sea_surface_label=None,
                            bathymetry_label=None):
    
    class_arr = point_cloud['class_ph'].to_numpy()
    sea_surface_indices = np.argwhere(class_arr == sea_surface_label).flatten()

    # Aggregate data into dataframe
    dataset_sea = pd.DataFrame({'index_ph': point_cloud['index_ph'].values,
                                'temp_index': np.arange(0, (point_cloud.shape[0]), 1),
                                'lat_ph': point_cloud['lat_ph'].values,
                                'lon_ph': point_cloud['lon_ph'].values,
                                'photon_height': point_cloud['ortho_h'],
                                'classifications': class_arr},
                           columns=['index_ph', 'temp_index', 'lat_ph', 'lon_ph', 'photon_height', 'classifications'])
    
    # Filter for elevation range
    dataset_sea1 = dataset_sea[(dataset_sea['photon_height'] > min_buffer) & (dataset_sea['photon_height'] < max_buffer)]
    
    binned_data_sea = bin_data(dataset_sea1, lat_res, h_res)

    binned_data_sea["height_bins"] = pd.to_numeric(binned_data_sea["height_bins"])

    _, geo_df = get_bath_height(binned_data_sea, thresh,
                                h_res,
                                min_photons_per_bin=min_photons_per_bin)

    if geo_df is not None:

        # Remove Bathy points without seasurface above.
        # sea_surf_lats = dataset_sea['lat_ph'][sea_surface_indices]
        # bathy_keep = _array_for_loop(geo_df['lat_ph'].to_numpy(), surf_lats=sea_surf_lats)
        # geo_df = geo_df[bathy_keep]

        classifications = np.zeros((point_cloud.shape[0]))
        classifications[:] = 0
        
        classifications[geo_df['PC_index'].to_numpy()] = bathymetry_label  # sea floor

        med_water_surface = np.nanmean(geo_df['med_sea_surf'].to_numpy())

        unique_bathy_filterlow = np.argwhere(point_cloud['ortho_h'] > (med_water_surface - (h_res * 2.5))).flatten()
        
        classifications[geo_df['PC_index'].to_numpy()] = bathymetry_label
        classifications[unique_bathy_filterlow] = 0
        classifications[sea_surface_indices] = sea_surface_label  # sea surface

        results = {'classification': classifications}
        
        return results
    else:

        classifications = np.zeros((point_cloud.shape[0]))
        classifications[:] = 0
        classifications[sea_surface_indices] = sea_surface_label  # sea surface
        results = {'classification': classifications}

        return results
    

def plot_pointcloud(classified_pointcloud=None, output_path=None):

    import matplotlib as mpl
    from matplotlib import pyplot as plt

    ylim_min = -80
    ylim_max = 20

    xlim_min = 24.5
    xlim_max = 25

    plt.figure(figsize=(48, 16))
    
    plt.plot(classified_pointcloud['lat_ph'][classified_pointcloud['classifications'] == 0.0],
                classified_pointcloud['ortho_h'][classified_pointcloud['classifications'] == 0.0],
                'o', color='0.7', label='Other', markersize=2, zorder=1)
    
    plt.plot(classified_pointcloud['lat_ph'][classified_pointcloud['classifications'] == 41.0],
                classified_pointcloud['ortho_h'][classified_pointcloud['classifications'] == 41.0],
                'o', color='blue', label='Other', markersize=5, zorder=5)
    
    plt.plot(classified_pointcloud['lat_ph'][classified_pointcloud['classifications'] == 40.0],
                classified_pointcloud['ortho_h'][classified_pointcloud['classifications'] == 40.0],
                'o', color='red', label='Other', markersize=5, zorder=5)


    plt.xlabel('Latitude (degrees)', fontsize=36)
    plt.xticks(fontsize=34)
    plt.ylabel('Height (m)', fontsize=36)
    plt.yticks(fontsize=34)
    plt.ylim(ylim_min, ylim_max)
    # plt.xlim(xlim_min, xlim_max)
    plt.title('Final Classifications - ', fontsize=40)
    # plt.title(fname + ' ' + channel)
    plt.legend(fontsize=36)
    
    # plt.savefig(output_path)
    # plt.close()

    plt.show()


    return


def main(args):

    input_fname = args.beam_data_csv
    output_label_fname = args.output_data_csv

    sea_surface_label = 41
    bathymetry_label = 40

    point_cloud = pd.read_csv(input_fname)

    point_cloud = point_cloud.rename(columns={'manual_label': 'class_ph',
                                              'ph_index': 'index_ph',
                                              'lat_ph': 'lat_ph',
                                              'lon_ph': 'lon_ph',
                                              'geoid_corrected_h': 'ortho_h'})

    # Start Bathymetry Classification
    c_shelph_results = c_shelph_classification(copy.deepcopy(point_cloud), surface_buffer=-0.5,
                                                                        h_res=0.5, lat_res=0.001, thresh=25,
                                                                        min_buffer=-80, max_buffer=5,
                                                                        min_photons_per_bin=5,
                                                                        # sea_surface_indices=sea_surface_inds,
                                                                        sea_surface_label=sea_surface_label,
                                                                        bathymetry_label=bathymetry_label)

    point_cloud['classifications'] = c_shelph_results['classification']

    # plot_path = output_label_fname.replace('.csv', '.png')
    # plot_pointcloud(classified_pointcloud=point_cloud, output_path=plot_path)

    point_cloud.to_csv(output_label_fname)

    return


if __name__=="__main__":

    import argparse
    import sys

    parser = argparse.ArgumentParser()

    # <configuration json> <beam information json> <beam data csv> <output data csv>

    parser.add_argument("--configuration-json")
    parser.add_argument("--beam-information-json")
    parser.add_argument("--beam-data-csv")
    parser.add_argument("--output-data-csv")

    args = parser.parse_args()

    main(args)

    sys.exit(0)