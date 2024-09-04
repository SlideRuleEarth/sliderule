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
import traceback

def rolling_median_bathy_classification(point_cloud, 
                         window_sizes=[51, 30, 7],
                         kdiff=0.6, kstd=1.2,
                         high_low_buffer=4,
                         min_photons=14,
                         segment_length=0.001, # 0.001
                         compress_heights=None,
                         compress_lats=None):
    
    sea_surface_label = 41
    bathymetry_label = 40

    class_arr = point_cloud['class_ph'].to_numpy()
    sea_surface_indices = np.argwhere(class_arr == sea_surface_label).flatten()
    
    indices = np.arange(0, point_cloud.shape[0], 1, dtype=int)
    
    # sea_surf = np.ones(point_cloud['h_ph'].to_numpy().size, dtype=bool)

    median_sea_surf = np.nanmedian(point_cloud.ortho_h[sea_surface_indices])

    unique_bathy_filterlow = np.argwhere(point_cloud.ortho_h > (median_sea_surf - 1.5)).flatten()
    
    mask_sea_surf = np.ones(point_cloud.shape[0], dtype=bool)
    mask_sea_surf[sea_surface_indices] = False

    heights = point_cloud['ortho_h'].to_numpy()[mask_sea_surf]
    lons = point_cloud['lon_ph'].to_numpy()[mask_sea_surf]
    lats = point_cloud['lat_ph'].to_numpy()[mask_sea_surf]
    # times = point_cloud['delta_time'].to_numpy()[mask_sea_surf]

    if compress_heights is not None:
        heights = heights * compress_heights
    
    if compress_lats is not None:
        lats = lats * compress_lats

    h, lons, lats, ind_keep = rolling_median_buffer(heights=heights, lons=lons,
                                                              lats=lats,
                                                              window_size=window_sizes[0],
                                                              high_low_buffer=high_low_buffer,
                                                              indices=indices[mask_sea_surf])

    high_ph, high_lons, high_lats, std_ind_keep = rolling_median_std(heights=h, lons=lons, lats=lats,
                                                                                 keep_index=ind_keep, window_size=window_sizes[1], kdiff=kdiff, kstd=kstd)
    
    try:

        ## Rough Select Bathymetry
        rg_h_lons, rg_h_lats, rg_h_heights, rg_keep_index = real_group_eliminate(lons=high_lons, lats=high_lats,
                                                                ph_h=high_ph, keep_index=std_ind_keep, segment_length=segment_length,
                                                                min_photons=min_photons)

        ## Average Smoothing Window
        _, _, _, keep_indices = rolling_average_smooth(heights=rg_h_heights,
                                                          lons=rg_h_lons,
                                                          lats=rg_h_lats,
                                                          keep_index=rg_keep_index,
                                                          window_size=window_sizes[2])

        classifications = np.zeros((point_cloud.shape[0]))
        classifications[:] = 0
        
        classifications[np.asarray(keep_indices)] = bathymetry_label  # sea floor
        classifications[unique_bathy_filterlow] = 0
        classifications[sea_surface_indices] = sea_surface_label  # sea surface
        
        results = {'classification': classifications}
        
        return results
    
    except Exception as rolling_med_model_error:

        if 'cannot unpack non-iterable NoneType object' in str(rolling_med_model_error):
            print('Median Model: Failed to find bathymetry photons.')
            # print(str(traceback.format_exc()))

        else:
            print(str(traceback.format_exc()))

        classifications = np.empty((point_cloud['class_ph'].to_numpy().shape))
        classifications[:] = 0
        classifications[sea_surface_indices] = sea_surface_label

        return {'classification': classifications}
            

def rolling_median_buffer(heights=None, lons=None, lats=None,
                          window_size=None, high_low_buffer=None, indices=None):

    """
        Calculates the rolling median of the heights 1D array within a defined window size.

        Based on defined high/low buffer, photons outside the median buffere range are removed.
    """

    # Adding [:,None] adds a new empty dimension to a numpy array for indexing arrays of different dims
    window_inds = np.arange(window_size) + np.arange(len(heights) - window_size + 1)[:,None]
    
    window_median = np.median(heights[window_inds], axis=1)

    high = np.unique(window_inds[heights[window_inds] > (window_median[:,None] + high_low_buffer)])
    low = np.unique(window_inds[heights[window_inds] < (window_median[:,None] - high_low_buffer)])

    ind_remove = np.unique(np.concatenate((high, low), axis=None))
    keep = np.unique(window_inds.ravel())
    ind_keep = np.delete(keep, ind_remove)

    rolling_median_heights = np.delete(heights, ind_remove)
    rolling_median_lons = np.delete(lons, ind_remove)
    rolling_median_lats = np.delete(lats, ind_remove)

    indices_to_keep = np.delete(indices, ind_remove)

    return rolling_median_heights, rolling_median_lons, rolling_median_lats, indices_to_keep


def rolling_median_std(heights=None, lons=None, lats=None,
                       keep_index=None, window_size=None,
                       kdiff=None, kstd=None):

    """
        Filters elevations based on rolling median and standard deviation criteria.

        Filters elevation photons based on their deviation from the rolling median and
        the rolling standard deviation within a specified window.

    """

    # Adding [:,None] adds a new empty dimension to a numpy array for indexing arrays of different dims
    window_inds = np.arange(window_size) + np.arange(len(heights) - window_size + 1)[:,None]
    window_median = np.median(heights[window_inds], axis=1)
    kdiff_keep_inds = np.unique(window_inds[np.abs(window_median[:,None] - heights[window_inds]) < kdiff])
    window_std = np.std(heights[window_inds], axis=1, ddof=1)
    kstd_keep_inds = window_inds[(window_std < kstd)]

    comb_std_diff_keep = np.intersect1d(kdiff_keep_inds, kstd_keep_inds)

    rolling_median_heights = heights[comb_std_diff_keep]
    rolling_median_lons = lons[comb_std_diff_keep]
    rolling_median_lats = lats[comb_std_diff_keep]
    
    keep_index = keep_index[comb_std_diff_keep]

    return rolling_median_heights, rolling_median_lons, rolling_median_lats, keep_index


def consecutive(data, stepsize=1):

    """
        Splits a 1D array into subarrays containing
        consecutive elements based on a specified step size.
    """
    return np.split(data, np.where(np.diff(data) != stepsize)[0]+1)


def real_group_eliminate(lons=None, lats=None, ph_h=None,
                         keep_index=None,
                         segment_length=None, min_photons=None):
    '''
        Group the photon heights into segments of latitude distance
        in degrees and return groups with at least min_photons. For
        groups with less than the min_photons, the heights are not
        assumed to be real.

        For the sorting to work correctly, the arrays must be
        ordered by ascending lattitude.

        segment_length of 0.001 ~ 100 m. (111m at the equator)
    '''

    if len(list(lats)) > 0:

        ## Order the arrays by ascending latitude
        if lats[0] > lats[-1]:

            lats = lats[::-1]
            lons = lons[::-1]
            ph_h = ph_h[::-1]
            # times = times[::-1]

        min_lat_range = lats.min() // segment_length / (1/segment_length) + segment_length
        max_lat_range = lats.max() // segment_length / (1/segment_length)

        split_at = lats.searchsorted(np.arange(min_lat_range,
                                               max_lat_range,
                                               segment_length))

        more_than_min_photons = (split_at[1:]-split_at[:-1] > min_photons).nonzero()[0] + 1

        lat_groups = np.split(lats, split_at)
        lat_groups = np.asarray(lat_groups, dtype=object)

        lon_groups = np.split(lons, split_at)
        lon_groups = np.asarray(lon_groups, dtype=object)

        ph_h_groups = np.split(ph_h, split_at)
        ph_h_groups = np.asarray(ph_h_groups, dtype=object)
        
        keep_index_groups = np.split(keep_index, split_at)
        keep_index_groups = np.asarray(keep_index_groups, dtype=object)

        try:

            grouped_lons = [np.concatenate([lon_groups[i]][0]).ravel() for i in consecutive(more_than_min_photons, stepsize=1)]
            grouped_lats = [np.concatenate([lat_groups[i]][0]).ravel() for i in consecutive(more_than_min_photons, stepsize=1)]
            grouped_ph_h = [np.concatenate([ph_h_groups[i]][0]).ravel() for i in consecutive(more_than_min_photons, stepsize=1)]
            grouped_keep_index_groups = [np.concatenate([keep_index_groups[i]][0]).ravel() for i in consecutive(more_than_min_photons, stepsize=1)]

            return grouped_lons, grouped_lats, grouped_ph_h, grouped_keep_index_groups

        except Exception as medmodel_error:

            if 'need at least one array to concatenate' in str(medmodel_error):
                print('Median Model: No bathymetry photons found in this segment')

            return None
    else:
        return None


def flatten_coord_blocks(coord_block_array=None):
    return [item for block in coord_block_array for item in block]


def rolling_average_smooth(heights=None, lons=None, lats=None,
                           keep_index=None, window_size=None):
    """
        Performs a rolling average smoothing along photon elevations with window_size.

        This function calculates the rolling average of a 1D array within a specified
        window_size.
    """

    if (window_size % 2) == 0:
        raise Exception('window_size must be odd')

    # Adding [:,None] adds a new empty dimension to a numpy array for indexing arrays of different dims
    window_inds = [np.arange(window_size) + np.arange(len(heights[i]) - window_size + 1)[:,None] for i in np.arange(len(heights))]
    window_mean = [np.mean(heights[i][window_inds[i]], axis=1) for i in np.arange(len(heights))]
    window_centers = [window_inds[i][:,(window_size // 2)] for i in np.arange(len(heights))]

    center_lats = [lats[i][window_centers[i]] for i in np.arange(len(heights))]
    center_lons = [lons[i][window_centers[i]] for i in np.arange(len(heights))]
    center_inds = [keep_index[i][window_centers[i]] for i in np.arange(len(heights))]

    return flatten_coord_blocks(center_lons), \
           flatten_coord_blocks(center_lats), \
           flatten_coord_blocks(window_mean), \
           flatten_coord_blocks(center_inds)



# def time2UTC(gps_seconds_array=None):

#     # Number of Leap seconds
#     # See: 'https://www.ietf.org/timezones/data/leap-seconds.list'
#     # 15 - 1 Jan 2009
#     # 16 - 1 Jul 2012
#     # 17 - 1 Jul 2015
#     # 18 - 1 Jan 2017
#     leap_seconds = 18

#     gps_start = datetime.datetime(year=1980, month=1, day=6)
#     time_ph = [datetime.timedelta(seconds=time) for time in gps_seconds_array]
#     # last_photon = datetime.timedelta(seconds=gps_seconds[-1])
#     error = datetime.timedelta(seconds=leap_seconds)
#     ph_time_utc = [(gps_start + time - error) for time in time_ph]

#     return ph_time_utc







def plot_pointcloud(classified_pointcloud=None, output_path=None):

    import matplotlib as mpl
    from matplotlib import pyplot as plt

    ylim_min = -80
    ylim_max = 20

    plt.figure(figsize=(48, 16))
    
    plt.plot(classified_pointcloud['lat_ph'][classified_pointcloud['classifications'] == 0.0],
                classified_pointcloud['ortho_h'][classified_pointcloud['classifications'] == 0.0],
                'o', color='0.7', label='Noise', markersize=2, zorder=1)
    
    plt.plot(classified_pointcloud['lat_ph'][classified_pointcloud['classifications'] == 41.0],
                classified_pointcloud['ortho_h'][classified_pointcloud['classifications'] == 41.0],
                'o', color='blue', label='Sea Surface', markersize=5, zorder=5)
    
    plt.plot(classified_pointcloud['lat_ph'][classified_pointcloud['classifications'] == 40.0],
                classified_pointcloud['ortho_h'][classified_pointcloud['classifications'] == 40.0],
                'o', color='red', label='Bathymetry', markersize=5, zorder=5)

#         plt.scatter(point_cloud.x[point_cloud._bathy_classification_counts == 1],
#                  point_cloud.z[point_cloud._bathy_classification_counts == 1],
#                  s=1, marker='.', c=point_cloud._bathy_classification_counts[point_cloud._bathy_classification_counts == 1], cmap='cool', vmin=0, vmax=1, label='Seabed')
    # if point_cloud._z_refract is not None:
    #     if point_cloud._z_refract.any():
    #         plt.scatter(point_cloud.y[point_cloud._bathy_classification_counts > 0],
    #             point_cloud._z_refract[point_cloud._bathy_classification_counts > 0],
    #             s=36, marker='o', c=point_cloud._bathy_classification_counts[point_cloud._bathy_classification_counts > 0], cmap='Reds', vmin=0, vmax=1, label='Refraction Corrected', zorder=11)

    plt.xlabel('Latitude (degrees)', fontsize=36)
    plt.xticks(fontsize=34)
    plt.ylabel('Height (m)', fontsize=36)
    plt.yticks(fontsize=34)
    plt.ylim(ylim_min, ylim_max)
    # plt.xlim(xlim_min, xlim_max)
    plt.title('Med Filter Predictions', fontsize=40)
    # plt.title(fname + ' ' + channel)
    plt.legend(fontsize=36)
    
    plt.savefig(output_path + '_FINAL.png')
    plt.close()

    

    return



# def plot_pointcloud_truth_comparison(classified_pointcloud=None, output_path=None):

#     import matplotlib as mpl
#     from matplotlib import pyplot as plt

#     f, ax = plt.subplots(2, 1, figsize=(48,16), 
#                                  sharex=True)

#     ylim_min = -80
#     ylim_max = 20
    
#     ax[0].plot(classified_pointcloud['lat_ph'][classified_pointcloud['classifications'] == 0.0],
#                 classified_pointcloud['ortho_h'][classified_pointcloud['classifications'] == 0.0],
#                 'o', color='0.7', label='Noise', markersize=2, zorder=1)
    
#     ax[0].plot(classified_pointcloud['lat_ph'][classified_pointcloud['classifications'] == 41.0],
#                 classified_pointcloud['ortho_h'][classified_pointcloud['classifications'] == 41.0],
#                 'o', color='blue', label='Sea Surface', markersize=5, zorder=5)
    
#     ax[0].plot(classified_pointcloud['lat_ph'][classified_pointcloud['classifications'] == 40.0],
#                 classified_pointcloud['ortho_h'][classified_pointcloud['classifications'] == 40.0],
#                 'o', color='red', label='Bathymetry', markersize=5, zorder=5)
    

#     ax[1].plot(classified_pointcloud['lat_ph'][classified_pointcloud['class_ph'] == 0.0],
#                 classified_pointcloud['ortho_h'][classified_pointcloud['class_ph'] == 0.0],
#                 'o', color='0.7', label='Noise', markersize=2, zorder=1)
    
#     ax[1].plot(classified_pointcloud['lat_ph'][classified_pointcloud['class_ph'] == 41.0],
#                 classified_pointcloud['ortho_h'][classified_pointcloud['class_ph'] == 41.0],
#                 'o', color='blue', label='Sea Surface', markersize=5, zorder=5)
    
#     ax[1].plot(classified_pointcloud['lat_ph'][classified_pointcloud['class_ph'] == 40.0],
#                 classified_pointcloud['ortho_h'][classified_pointcloud['class_ph'] == 40.0],
#                 'o', color='green', label='Bathymetry', markersize=5, zorder=5)

#     ax[0].set_xlabel('Latitude (degrees)', fontsize=36)
#     ax[0].set_ylabel('Height (m)', fontsize=36)
#     ax[0].tick_params(axis='x', labelsize=34)
#     ax[0].tick_params(axis='y', labelsize=34)
#     ax[0].set_ylim(ylim_min, ylim_max)
#     # ax[0].set_xlim(xlim_min, xlim_max)
#     ax[0].set_title('Med Filter Predictions', fontsize=40)
#     # plt.title(fname + ' ' + channel)
#     ax[0].legend(fontsize=36)

#     ax[1].set_xlabel('Latitude (degrees)', fontsize=36)
#     ax[1].set_ylabel('Height (m)', fontsize=36)
#     ax[1].tick_params(axis='x', labelsize=34)
#     ax[1].tick_params(axis='y', labelsize=34)
#     ax[1].set_ylim(ylim_min, ylim_max)
#     # ax[1].set_xlim(xlim_min, xlim_max)
#     ax[1].set_title('Truth Manual Labels', fontsize=40)
#     # plt.title(fname + ' ' + channel)
#     ax[1].legend(fontsize=36)
    
#     f.subplots_adjust(hspace=0.4)
#     f.savefig(output_path + '_FINAL_truth_comparison.png')
#     plt.close(f)

#     return


def main(args):

    input_fname = args.beam_data_csv
    output_label_fname = args.output_data_csv

    sea_surface_label = 41
    bathymetry_label = 40

    point_cloud = pd.read_csv(input_fname)

    # Start Bathymetry Classification
    plot_path = output_label_fname.replace('.csv', '.png')

    rolling_median_filter_results = rolling_median_bathy_classification(point_cloud=point_cloud,
                                                                        window_sizes=[51, 30, 7],
                                                                        kdiff=0.75, kstd=1.75,
                                                                        high_low_buffer=4,
                                                                        min_photons=14,
                                                                        segment_length=0.001,
                                                                        compress_heights=None,
                                                                        compress_lats=None)

    point_cloud['classifications'] = rolling_median_filter_results['classification']

    plot_path = output_label_fname.replace('.csv', '.png')

    # plot_pointcloud(classified_pointcloud=point_cloud, output_path=plot_path)
    # plot_pointcloud_QF(classified_pointcloud=classified_pointcloud, output_path=plot_path)
    # plot_pointcloud_truth_comparison(classified_pointcloud=point_cloud, output_path=plot_path)

    point_cloud.to_csv(output_label_fname)

    return

if __name__=="__main__":

    import argparse
    import sys
    import numpy as np
    import traceback
    import pandas as pd

    parser = argparse.ArgumentParser()

    # <configuration json> <beam information json> <beam data csv> <output data csv>

    parser.add_argument("--configuration-json")
    parser.add_argument("--beam-information-json")
    parser.add_argument("--beam-data-csv")
    parser.add_argument("--output-data-csv")

    args = parser.parse_args()

    main(args)

    sys.exit(0)

    # python3 /home/mjh5468/local_repo_development/ATL24-medianmodel/medianmodel.py --configuration-json '' --beam-information-json '' --beam-data-csv '/home/mjh5468/test_data/SLIDERULE_testing/bathy_spot_3.csv' --output-data-csv '/home/mjh5468/test_data/SLIDERULE_testing/bathy_spot_3_classified.csv'


