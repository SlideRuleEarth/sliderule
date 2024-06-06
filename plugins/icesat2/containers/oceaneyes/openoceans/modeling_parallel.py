# Copyright (c) 2022, Jonathan Markel/UT Austin.
# 
# Redistribution and use in source and binary forms, with or without 
# modification, are permitted provided that the following conditions are met:
# 
# Redistributions of source code must retain the above copyright notice, 
# this list of conditions and the following disclaimer.
#
# Redistributions in binary form must reproduce the above copyright notice, 
# this list of conditions and the following disclaimer in the documentation 
# and/or other materials provided with the distribution.
#
# Neither the name of the copyright holder nor the names of its 
# contributors may be used to endorse or promote products derived from this 
# software without specific prior written permission. 
# 
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS 
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED 
# TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR 
# PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR 
# CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, 
# EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, 
# PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; 
# OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, 
# WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR '
# OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF 
# ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

import copy
import json
import os
from datetime import datetime
from itertools import repeat

import numpy as np
import pandas as pd
import scipy.stats as stats

# from tqdm import tqdm
from fast_histogram import histogram1d, histogram2d
from pathos import (
    multiprocessing,  # changed from multiprocessing to remedy the "can't pickle" errors
)
from scipy.ndimage import gaussian_filter1d
from tqdm import tqdm

from waveform import Waveform

# note parallelization provided via M. Holwill at UT Austin 3DGL    

def iterate_models(params, input_profile, range_z=(-45, 15), z_res_jitter_bounds=(0.1, 0.3), verbose=False):
    """Iteritive / Ensemble / super resolution models.
    Can be run in two different modes:
        1) Random jitter Z Resolution (different values within z_res_jitter_bounds for each model):
            - Provide params as a list of 2-elements lists
            -- [[AT resol1, window size1],[AT resol2, window size2],...]
            - Provide z_res_jitter_bounds\n
        2) Fixed Z resolutions for each model (defined for each model):
            - Provide params as a list of 3-elements lists
            -- [[AT resol1, window size1, z_res1],[AT resol2, window size2, z_res2],...]

    Keyword arguments:\n
    params: list of lists
            - for jitter z | list of 2-elements lists of [[AT resol, window size]]
            - for fixed z  | list of 3-element lists [[AT resol, window size, z res]]
            
    input_profile: OpenOceans profile object from, oo.Profile.from_h5(...)
    
    range_z: tuple (min_z,max_z) elevations for analysis
    
    z_res_jitter_bounds(optional): tuple (min,max) minimum and max values for random jitter in z_res
    
    verbose: boolean [default=False], extra pront statements for debugging
    
    """

    if verbose:
        print('{}-model ...'.format(len(params)))
        print('Initiated at ' + datetime.now().strftime("%H:%M:%S"))

    model_list = []

    for ii in range(len(params)):
        
        if len(params[ii]) == 2:  # jitter Z resolution
            i_res_at, i_window = params[ii]
            # randomly jitter the z value to increase vertical resolution
            i_res_z = np.random.default_rng().uniform(
                z_res_jitter_bounds[0], z_res_jitter_bounds[1])
            
        elif len(params[ii]) == 3:  # fixed Z resolution
            i_res_at, i_window, i_res_z = params[ii]

        header_str = ' Model {}: Resolution: {} | Window: {}'.format(
            ii+1, i_res_at, i_window)
        if verbose:
            print(header_str)


        M_i = ModelMakerP(res_along_track=i_res_at,  # in meters # range from 5 to 10000 m
                         res_z=i_res_z,  # in meters
                         range_z=range_z,
                         window_size=i_window,  # in bins # range from 3 , max depending on along track length
                         fit=False, verbose=verbose)

        # use the parameters of this model to process the profile from before
        m_i = M_i.process(input_profile)
        model_list.append(m_i)

    return model_list

def average_labels(profile, waveform_list):
    """
    Generates final labels for a given profile based on (potentially duplicated) photon labels. Scores are computed as the number of times a photon received a given label for how many times it was labeled.

    Parameters:
    - profile (Profile): The profile object that needs labeling.
    - waveform_list (list): List of waveform objects.

    Returns:
    - tuple: Consists of four elements:
        1. labels (np.ndarray): Final numeric labels for each photon in the profile.
        2. confidences (pd.DataFrame): Confidence scores for each class label for each photon.
        3. weights (pd.DataFrame): Average weights for 'surface' and 'bathymetry' classes for each photon.
        4. subsurface_score (pd.Series): Subsurface flag scores for each photon.

    Notes:
    The function performs various operations such as averaging different model outputs for each photon to determine
    the final label. It also computes confidences and weights for 'surface' and 'bathymetry'.
    """

    # replacing all instances of index with photon_index 
    # hopefully this fixes the splattered classification issue with some tracks

    data_with_dupes = pd.concat(
        [wf.profile.data for wf in waveform_list]
    )

    # get scores of individual classes
    total_dupes = data_with_dupes.classification.groupby(
        data_with_dupes.photon_index).size()

    # for a given photon, average how many of the times it was classified as surface vs something else
    is_surface = data_with_dupes.classification == profile.label_help(
        "surface")
    surface_total = is_surface.groupby(data_with_dupes.photon_index).sum()
    surface_score = surface_total / total_dupes

    # average binary subsurface flag
    is_subsurface = data_with_dupes.subsurface_flag
    subsurface_total = is_subsurface.groupby(data_with_dupes.photon_index).sum()
    subsurface_score = subsurface_total / total_dupes

    # for a given photon, average how many of the times it was classified as bathymetry vs something else
    is_bathy = data_with_dupes.classification == profile.label_help(
        "bathymetry")
    bathy_total = is_bathy.groupby(data_with_dupes.photon_index).sum()
    bathy_score = bathy_total / total_dupes

    # for a given photon, average how many of the times it was classified as water column
    is_column = data_with_dupes.classification == profile.label_help("column")
    column_total = is_column.groupby(data_with_dupes.photon_index).sum()
    column_score = column_total / total_dupes

    # for a given photon, average how many of the times it was classified as background
    is_background = data_with_dupes.classification == profile.label_help(
        "background")
    background_total = is_background.groupby(data_with_dupes.photon_index).sum()
    background_score = background_total / total_dupes

    # update model-based class weights
    weight_surface = data_with_dupes.weight_surface.groupby(
        data_with_dupes.photon_index
    ).mean()
    weight_bathymetry = data_with_dupes.weight_bathymetry.groupby(
        data_with_dupes.photon_index
    ).mean()

    # initialize and then update scores
    photon_class_scores = pd.DataFrame(
        -1,
        index=profile.data.photon_index.values,
        columns=["none", "background", "surface", "column", "bathymetry"],
    )

    # classed_photons = data_with_dupes.index.unique().values
    classed_photons = data_with_dupes.photon_index.unique()

    # handles if photons weren't included in a waveform
    # not sure why that's happening but seems to only be the first photon

    # also, using .values for these will break the indexing (e.g. background score)
    # if you are looking for why the sea surface etc seems to be all over the place labeled at random, this is why
    photon_class_scores.loc[classed_photons,
                            "background"] = background_score
    photon_class_scores.loc[classed_photons, "surface"] = surface_score
    photon_class_scores.loc[classed_photons, "column"] = column_score
    photon_class_scores.loc[classed_photons, "bathymetry"] = bathy_score

    # assign final class based on which label a photon was assigned most frequently
    photon_final_class = -np.ones((profile.data.shape[0], 1))
    class_str_labels = photon_class_scores.idxmax(axis=1).values

    # convert string labels to numeric
    photon_final_class[class_str_labels == "background"] = profile.label_help(
        "background"
    )
    photon_final_class[class_str_labels ==
                       "surface"] = profile.label_help("surface")
    photon_final_class[class_str_labels ==
                       "column"] = profile.label_help("column")
    photon_final_class[class_str_labels == "bathymetry"] = profile.label_help(
        "bathymetry"
    )
    photon_final_class[class_str_labels == "none"] = profile.label_help("none")

    profile.data.loc[:, "classification"] = photon_final_class

    # outputs for profile.data

    labels = photon_final_class

    # confidences = average waveform labels
    confidences = photon_class_scores

    # weights = average waveform weights
    weights = pd.DataFrame(
        [weight_surface, weight_bathymetry], index=["surface", "bathymetry"]
    ).T

    # subsurface_score = defined above

    return labels, confidences, weights, subsurface_score

def round_if_integer(x, n=2):
    if round(x, n) == x:
        return np.int64(x)
    return x

def effectively_zero(values):

    if len(values) < 2:
        # not enough data to make a meaningful comparison
        return False

    t, p = stats.ttest_1samp(values, 0)

    return p > 0.05


class ModelMakerP:


    def __init__(self, res_along_track, res_z, range_z, window_size, fit=False, verbose=False, photon_bins = False, parallel=False):
        """Create an instance with specified processing parameters at which to process profiles..

        Args:
            res_along_track (float): Along-track bin size which to histogram the photon data, in meters.
            res_z (float): Size of elevation bins with which to histogram photon data, in meters. Will be approximated as closely as possible to evenly fit the specified range_z, so recommended using nicely dividing decimals (like 0.1m).
            range_z (tuple): Total range of elevation values to bin, in meters. DEPTH OR ELEVATION
            window_size (float): How large of an along-track window to aggregate for constructing pseudowaveforms, in multiples of res_along_track. Recommended value of 1, must be odd.
            step_along_track (float): How many along track bins from step to step when constructing waveforms. Must be <= window_size.
            fit (boolean?): ?
            photonbins (boolean): Converts the processing pipeline to use photon count binning, not distance. Specify # of photons per bin with res_along_track 
        """
        assert window_size > 0
        # odd for symmetry about center bins
        assert np.mod(window_size, 2) == 1

        self.res_along_track = res_along_track
        self.res_z = res_z
        self.range_z = range_z
        self.window_size = window_size
        self.step_along_track = 1
        self.fit = fit
        self.photon_bins = photon_bins
        self.parallel = parallel
        self.verbose = verbose

        # vertical bin sizing
        bin_count_z = np.ceil((self.range_z[1] - self.range_z[0]) / self.res_z)
        res_z_actual = np.abs(self.range_z[1] - self.range_z[0]) / bin_count_z

        if res_z_actual != self.res_z:
            print(f'Adjusting z_res - new value: {res_z_actual:.4f}')
            self.z_res = res_z_actual


    def __str__(self):
        return (f"ModelMaker(res_along_track={self.res_along_track}, res_z={self.res_z}, "
                f"range_z={self.range_z}, window_size={self.window_size}, "
                f"step_along_track={self.step_along_track}, fit={self.fit}, verbose={self.verbose})")

    def get_formatted_filename(self):

        res_at = round_if_integer(self.res_along_track)
        res_z = self.res_z
        range_z_0 = -round_if_integer(self.range_z[0])
        range_z_1 = round_if_integer(self.range_z[1])
        win = self.window_size
        step_at = self.step_along_track

        # programmatically create string to concatenate self attributes res_along_track, res_z, range_z, window_size, step_along_track
        model_id = f'{res_at:.0f}_\
{res_z:.2f}_{range_z_0:.0f}_\
{range_z_1:.0f}_{win:.0f}_\
{step_at:.0f}'

        return model_id
    
    def process_window(self, window_size=None, window_centre=None, z_min=None, z_max=None, z_bin_count=None, win_profile=None, bin_edges_z=None, at_beg=None, at_end=None, beam_strength=None):

        # version of dataframe with only nominal photons
        # use this data for constructing waveforms
        df_win_nom = win_profile.data.loc[win_profile.data.quality_ph == 0]

        height_geoid = df_win_nom.z_ph.values

        # subset of histogram data in the evaluation window
        h_ = histogram1d(height_geoid,
                         range=[z_min, z_max], bins=z_bin_count)

        # smooth the histogram with 0.1 sigma gaussian kernel
        h = gaussian_filter1d(h_, 0.1/self.res_z)

        # identify median lat lon values of photon data in this chunk
        # x_win = df_win_nom.lon_ph.median()
        # y_win = df_win_nom.lat_ph.median()
        at_win = df_win_nom.x_ph.median()
        any_sat_ph = (win_profile.data.quality_ph > 0).any()

        w = Waveform(np.flip(h), -np.flip(bin_edges_z),
                     fit=self.fit, sat_check=any_sat_ph, profile=win_profile, 
                     beam_strength=beam_strength)

        #################################################################

        # this next section is basically all reformatting and organizing the data
        # might be better elsewhere but keeping here so i dont have to
        # run through another loop

        # we also want to have the original photon data for photons in this chunk
        # may also be helpful to have track metadata/functions
        # this is for ALL photons, not just the nominal subset

        # w.add_profile(p)  # bug somewhere here returning nans

        # combine useful data into a nice dict for turning into a df
        extra_dict = {'window_centre': window_centre,  # indexing/location fields
                    #   'x_med': x_win,
                    #   'y_med': y_win,
                      'at_med': at_win,
                      'at_range': (at_beg, at_end),
                      'quality_flag': w.quality_flag,
                      'water_conf': -999,
                      'n_photons': win_profile.data.shape[0]}

        # combine into unified dict/df
        data_dict = dict(extra_dict, **w.params.to_dict())

        # testing
        # use -1 instead of nan in order to be able to ffill sparse matrix
        # -1 indicates we checked this chunk and it actually is empty, not just
        # that is got skipped by the step sizes
        if np.all(np.isnan(w.model.output)):
            w.model.output = -1 * np.ones_like(w.model.output)

        return window_centre, pd.Series(data_dict), w


    def process(self, in_profile, n_cpu_cores=10):


        profile = copy.deepcopy(in_profile)

        z_min = self.range_z[0]
        z_max = self.range_z[1]
        z_bin_count = np.int64(np.ceil((z_max - z_min) / self.res_z))
        bin_edges_z = np.linspace(z_min, z_max, num=z_bin_count+1)
        beam_strength = in_profile.info['beam_strength']


        data = copy.deepcopy(profile.data) 

        if self.photon_bins == False:  
        
            # along track bin sizing
            #   get the max index of the dataframe
            #   create bin group ids (at_idx_grp) based on pho_count spacing
            at_max_idx = data.x_ph.max()
            at_min_idx = data.x_ph.min()
            at_idx_grp = np.arange(data.x_ph.min(), at_max_idx + self.res_along_track, self.res_along_track)
            
            # sort the data by distance along track, reset the index
            # add 'at_grp' column for the bin group id
            data.sort_values(by='x_ph', inplace=True)
            #data.reset_index(inplace=True)
            data['idx'] = data.index
            data['at_grp'] = 0

            # for each bin group, assign an integer id for index values between each of the 
            #   group bin values. is pho_count = 20 then at_idx_grp = [0,20,49,60...]
            #   - data indices between 0-20: at_grp = 1
            #   - data indices between 20-40: at_grp = 2...
            print('Computing at_grp...')
            for i, grp in tqdm(enumerate(at_idx_grp)):
                if grp < at_idx_grp.max():
                    data['at_grp'][data['x_ph'].between(at_idx_grp[i], at_idx_grp[i+1])] = (at_idx_grp[i] - at_idx_grp.min()) / self.res_along_track
            
            # add group bin columns to the profile, photon group bin index
            profile.data['pho_grp_idx'] = data['at_grp']
            
            # calculating the range so the histogram output maintains exact along track values
            at_min = data.x_ph.min()
            xh = (data.x_ph.values)
            print('Computing bin_edges_at...')
            bin_edges_at_min = data.groupby('at_grp').x_ph.min().values
            bin_edges_at_max = data.groupby('at_grp').x_ph.max().values

            bin_edges_at = np.concatenate([np.array([data.x_ph.min()]), bin_edges_at_max])

            # array to store actual interpolated model and fitted model
            hist_modeled = np.nan * np.zeros((bin_edges_at.shape[0] -1, z_bin_count))
            
            start_step = (self.window_size) / 2
            end_step = len(bin_edges_at)

            # -1 to start index at 0 instead of 1. For correct indexing when writing to hist_modeled array.
            win_centers = np.arange(np.int64(start_step), np.int64(
                end_step), self.step_along_track) -1

            print('Running create_window_processing_args...')
            window_args = self.create_window_processing_args(profile=profile, data=data, win_centers=win_centers, bin_edges_at=bin_edges_at)
        else: 

            print("Photon Bin Processing")
            
            # set pho_count to the number of photons per bin from res_along_track input
            pho_count = self.res_along_track
            
            # along track bin sizing
            #   get the max index of the dataframe
            #   create bin group ids (at_idx_grp) based on pho_count spacing
            at_max_idx = data.index.max()
            at_min_idx = data.index.min()
            at_idx_grp = np.arange(data.index.min(), at_max_idx + pho_count, pho_count)
            
            # sort the data by distance along track, reset the index
            # add 'at_grp' column for the bin group id
            data.sort_values(by='x_ph', inplace=True)
            #data.reset_index(inplace=True)
            data['idx'] = data.index
            data['at_grp'] = 0

            # for each bin group, assign an integer id for index values between each of the 
            #   group bin values. is pho_count = 20 then at_idx_grp = [0,20,49,60...]
            #   - data indices between 0-20: at_grp = 1
            #   - data indices between 20-40: at_grp = 2...
            for i, grp in enumerate(at_idx_grp):
                if grp < at_idx_grp.max():
                    data['at_grp'][data['idx'].between(at_idx_grp[i], at_idx_grp[i+1])] = (at_idx_grp[i] - at_idx_grp.min()) / pho_count
            
            # add group bin columns to the profile, photon group bin index
            profile.data['pho_grp_idx'] = data['at_grp']
            
            # calculating the range so the histogram output maintains exact along track values
            at_min = data.x_ph.min()
            xh = (data.x_ph.values)
            bin_edges_at_min = data.groupby('at_grp').x_ph.min().values
            bin_edges_at_max = data.groupby('at_grp').x_ph.max().values

            bin_edges_at = np.concatenate([np.array([data.x_ph.min()]), bin_edges_at_max]) 
            
            # array to store actual interpolated model and fitted model
            hist_modeled = np.nan * np.zeros((bin_edges_at.shape[0] -1, z_bin_count))

            # print('bin_edges_at: ',bin_edges_at[0:5])
            # print('hist_modeled james: ',hist_modeled.shape)

            win_centers = data['at_grp'].unique()
            print('win_centers: ', win_centers)

            window_args = self.create_window_processing_args(profile=profile, data=data, win_centers=win_centers, bin_edges_at=bin_edges_at)

        if self.parallel:
            pstarttime = datetime.now()
            data = []

            print(f'Parallel processing {len(window_args["window_centres"])} bins...')
            with multiprocessing.Pool(n_cpu_cores) as pool:
                data = pool.starmap(self.process_window, zip(repeat(self.window_size), window_args['window_centres'], repeat(z_min), repeat(z_max),
                                                             repeat(z_bin_count), window_args['window_profiles'], repeat(bin_edges_z), window_args['at_begs'],
                                                             window_args['at_ends'], repeat(beam_strength)))
            pendtime = datetime.now()

            self.ptime = pendtime - pstarttime
            # summary of model params
            params = pd.DataFrame(list(zip(*data))[1])
    
            w_d = {elem[0]: elem[2] for elem in data}

            replace_indices = np.vstack([np.full(hist_modeled.shape[1], elem[0]) for elem in data])
    
            replace_with = np.vstack([np.flip(elem[2].model.output) for elem in data])
    
            np.put_along_axis(hist_modeled, replace_indices, replace_with, axis=0)

        else:

            # dictionary of waveform objects - (AT_bin_i, w)
            w_d = {}
    
            # list of organized/simple data series'
            d_list = []

            for at_beg, at_end, w_profile, window_centre in zip(window_args['at_begs'],
                                                                window_args['at_ends'],
                                                                window_args['window_profiles'],
                                                                window_args['window_centres']):
            
                window_centre, data_dict, waveform = self.process_window(window_size=self.window_size,
                                                                    window_centre=window_centre,
                                                                    z_min=z_min, z_max=z_max,
                                                                    z_bin_count=z_bin_count,
                                                                    win_profile=w_profile, bin_edges_z=bin_edges_z, 
                                                                    at_beg=at_beg, at_end=at_end, 
                                                                    beam_strength=beam_strength)
                
                d_list.append(pd.Series(data_dict))
    
                # update dictionary of waveforms
                w_d.update({window_centre: waveform})
    
                hist_modeled[window_centre, :] = np.flip(waveform.model.output)

            # summary of model params
            params = pd.DataFrame(d_list)

        print('creating model...')
        m = ModelP(params=params, model_hist=hist_modeled,
                  bin_edges_z=bin_edges_z,
                  bin_edges_at=bin_edges_at,
                  window_size=self.window_size,
                  waves=w_d, profile=profile, ModelMaker=self)

        return m


    def create_window_processing_args(self, profile=None, data=None, win_centers=None, bin_edges_at=None):

        at_begs = []
        at_ends = []
        window_profiles = []  # This will now hold DataFrames directly, not profile copies
        window_centres = []

        xh = profile.data.x_ph.values

        if not self.photon_bins:
            # optimized for speed, see slack message from Matt Holwill for original
            for window_centre in tqdm(win_centers):
                i_beg = np.int64(max((window_centre - (self.window_size-1) / 2), 0))
                i_end = np.int64(min((window_centre + (self.window_size-1) / 2), len(bin_edges_at)-2)) + 1

                at_beg = bin_edges_at[i_beg]
                at_end = bin_edges_at[i_end]

                # Subset data using along-track distance window
                # Does this need to be a deepcopy to avoid overwriting neighboring waveform labels?
                w_profile = copy.deepcopy(profile)

                i_cond = (xh > at_beg) & (xh < at_end)
                df_win = w_profile.data.loc[i_cond, :].copy()  # Make a copy here if needed

                w_profile.data = df_win

                at_begs.append(at_beg)
                at_ends.append(at_end)
                window_profiles.append(w_profile)
                window_centres.append(window_centre)
        
        else:
            raise ValueError('Optimized photon bin processing not yet implemented')
            # # not optimized
            # bin_edges_at_min = data.groupby('at_grp').x_ph.min().values
            # bin_edges_at_max = data.groupby('at_grp').x_ph.max().values

            # for window_centre in win_centers:

            #     # beginning and end dist for the photon group
            #     if self.window_size == 1:
            #         at_beg = data[data['at_grp']==window_centre].x_ph.min()
            #         at_end = data[data['at_grp']==window_centre].x_ph.max()
            #     else:
            #         if self.window_size - window_centre > 1:
            #             i_beg = 0
            #             i_end = self.window_size - window_centre
            #         elif window_centre - (data['at_grp'].max() - self.window_size) > 1:
            #             i_beg = np.int64(data['at_grp'].max() - np.floor(self.window_size/2))
            #             i_end = np.int64(data['at_grp'].max())
            #         else:
            #             i_beg = np.int64(window_centre - np.floor(self.window_size/2))
            #             i_end = np.int64(window_centre + np.floor(self.window_size/2))
                
            #         at_beg = bin_edges_at_min[i_beg]
            #         at_end = bin_edges_at_max[i_end]
                
            #     # subset data using photon group window
            #     df_win = data[data['at_grp'].between(i_beg,i_end)]
                
            #     # copy profile to avoid overwriting
            #     w_profile = copy.deepcopy(profile)
                
            #     # remove all data except for the photons in this window
            #     w_profile.data = df_win
                
            #     at_begs.append(at_beg)
            #     at_ends.append(at_end)
            #     window_profiles.append(w_profile)
            #     window_centres.append(window_centre)

        
        return {'at_begs': at_begs,
                'at_ends': at_ends,
                'window_profiles': window_profiles,
                'window_centres': window_centres}


    def process_orig(self, in_profile):
        
        profile = copy.deepcopy(in_profile)

        z_min = self.range_z[0]
        z_max = self.range_z[1]
        z_bin_count = np.int64(np.ceil((z_max - z_min) / self.res_z))
        beam_strength = in_profile.info['beam_strength']

        data = copy.deepcopy(profile.data) #TO DO: profile.data.copy(deep=true)

        # along track bin sizing
        at_min = data.x_ph.min()

        # calculating the range st the histogram output maintains exact along track values
        last_bin_offset = (self.res_along_track - np.mod((data.x_ph.max() -
                           data.x_ph.min()), self.res_along_track))

        at_max = data.x_ph.max() + last_bin_offset
        at_bin_count = np.int64((at_max - at_min) / self.res_along_track)

        xh = (data.x_ph.values)

        yh = (data.z_ph.values)

        bin_edges_at = np.linspace(at_min, at_max, num=at_bin_count+1)
        bin_edges_z = np.linspace(z_min, z_max, num=z_bin_count+1)

        # dictionary of waveform objects - (AT_bin_i, w)
        w_d = {}

        # list of organized/simple data series'
        d_list = []

        print('bin_edges_at markel: ',bin_edges_at.shape)
        print('at_bin_count markel: ',at_bin_count.shape)
        print('bin_edges_at start markel: ',bin_edges_at[0])
        print('bin_edges_at end markel: ',bin_edges_at[-1])

        # array to store actual interpolated model and fitted model
        hist_modeled = np.nan * np.zeros((at_bin_count, z_bin_count))

        print('hist_modeled markel: ',hist_modeled.shape)

        # at the center of each evaluation window
        # win centers needs to actually handle the center values unlike here
        # currently bugs out if win is 1

        # this step ensures that histograms at edges don't have lower 'intensity' just because the window exceeds the data range
        start_step = (self.window_size-1) / 2
        end_step = len(bin_edges_at) - (self.window_size-1) / 2 - 1

        win_centers = np.arange(np.int64(start_step), np.int64(
            end_step), self.step_along_track)

        for i in tqdm(win_centers):

            # get indices/at distance of evaluation window
            i_beg = np.int64(max((i - (self.window_size-1) / 2), 0))
            i_end = np.int64(
                min((i + (self.window_size-1) / 2), len(bin_edges_at)-2)) + 1
            # + 1 above pushes i_end to include up to the edge of the bin when used as index

            at_beg = bin_edges_at[i_beg]
            at_end = bin_edges_at[i_end]

            # could be sped up with numba if this is an issue
            # subset data using along track distance window
            i_cond = ((xh > at_beg) & (xh < at_end))
            df_win = data.loc[i_cond, :]

            # version of dataframe with only nominal photons
            # use this data for constructing waveforms
            df_win_nom = df_win.loc[df_win.quality_ph == 0]
            height_geoid = (df_win_nom.z_ph.values)

            # subset of histogram data in the evaluation window
            h_ = histogram1d(height_geoid,
                             range=[z_min, z_max], bins=z_bin_count)

            # smooth the histogram with 0.1 sigma gaussian kernel
            h = gaussian_filter1d(h_, 0.1/self.res_z)

            # identify median lat lon values of photon data in this chunk
            # x_win = df_win_nom.lon_ph.median()
            # y_win = df_win_nom.lat_ph.median()
            at_win = df_win_nom.x_ph.median()
            any_sat_ph = (df_win.quality_ph > 0).any()

            # copy profile to avoid overwriting
            w_profile = copy.deepcopy(profile)

            # remove all data except for the photons in this window
            w_profile.data = df_win

            w = Waveform(np.flip(h), -np.flip(bin_edges_z),
                         fit=self.fit, sat_check=any_sat_ph, profile=w_profile, beam_strength=beam_strength)

            #################################################################

            # this next section is basically all reformatting and organizing the data
            # might be better elsewhere but keeping here so i dont have to
            # run through another loop

            # we also want to have the original photon data for photons in this chunk
            # may also be helpful to have track metadata/functions
            # this is for ALL photons, not just the nominal subset

            # w.add_profile(p)  # bug somewhere here returning nans

            # combine useful data into a nice dict for turning into a df
            extra_dict = {'window_centre': i,  # indexing/location fields
                        #   'x_med': x_win,
                        #   'y_med': y_win,
                          'at_med': at_win,
                          'at_range': (at_beg, at_end),
                          'quality_flag': w.quality_flag,
                          'water_conf': -999,
                          'n_photons': df_win.shape[0]}

            # combine into unified dict/df
            data_dict = dict(extra_dict, **w.params.to_dict())

            # add formatted series to the list to combine when done
            # might be faster if you preallocate the dataframe instead
            # but i dont want to have to constantly tweak the number
            # of rows if i add fields to this

            d_list.append(pd.Series(data_dict))

            # update dictionary of waveforms
            w_d.update({i: w})

            # testing
            # use -1 instead of nan in order to be able to ffill sparse matrix
            # -1 indicates we checked this chunk and it actually is empty, not just
            # that is got skipped by the step sizes
            if np.all(np.isnan(w.model.output)):
                w.model.output = -1 * np.ones_like(w.model.output)

            hist_modeled[i, :] = np.flip(w.model.output)

        print('Final hist_modeled markel: ',hist_modeled.shape)

        # summary of model params
        params = pd.DataFrame(d_list)

        m = ModelP(params=params, model_hist=hist_modeled,
                  bin_edges_z=bin_edges_z,
                  bin_edges_at=bin_edges_at,
                  window_size=self.window_size,
                  waves=w_d, profile=profile, ModelMaker=self)

        return m





class ModelP:
    # more processing after the histogram model has been estimated

    def __init__(self, params, model_hist, bin_edges_z, bin_edges_at,
                 window_size, waves, profile, ModelMaker):
        # stores data for a model over a full profile

        self.params = params
        self.model_hist = model_hist
        self.bin_edges_z = bin_edges_z
        self.bin_edges_at = bin_edges_at
        self.res_z = np.diff(bin_edges_z)[0]
        self.res_at = np.diff(bin_edges_at)[0]
        self.window_size = window_size
        self.step_along_track = 1
        self.waves = waves
        self.profile = profile
        self.ModelMaker = ModelMaker

        # slightly different than the model resolution
        # depending on step size, window
        self.hist = self._compute_histogram()
        self.model_hist_interp = self._interp_model_hist(self.model_hist)

        column_score = self._compute_column_score()

        # compute column score and append to model data
        self.params.loc[:, 'turb_score'] = column_score

        # compute labels for photon data and update m.profile.data.classifications
        # _ = self.label_by_pseudowaveform(inplace=True)
        
        list_of_models = self.waves.values() 
        
        labels, confidences, weights, subsurface_score = average_labels(self.profile, list_of_models)
        
        # update values in self.profile.data
        self.profile.data.classification = labels
        self.profile.data.conf_surface = confidences.surface.values
        self.profile.data.conf_column = confidences.column.values
        self.profile.data.conf_bathymetry = confidences.bathymetry.values
        self.profile.data.conf_background = confidences.background.values
        self.profile.data.weight_surface = weights.surface
        self.profile.data.weight_bathymetry = weights.bathymetry
        self.profile.data.subsurface_flag = subsurface_score
        
    def __str__(self):

        # print out useful info about this model like params, the ranges of the z , AT values, win, step size, num waveforms

        pass

    def _compute_column_score(self):

        # index via wave keys or something else?
        n = len(self.waves.keys())
        column_score = np.zeros((n, ))

        for i in self.params.index.values:

            # sum of column component
            # column_score[i] = np.sum(self.waves[i].model.column)

            # get wave indexing key
            i_wave_key = self.params.iloc[i].window_centre

            # would need to be made into a true integral to be consistent regardless of bin resolution
            column_score[i] = np.trapz(self.waves[i_wave_key].model.column)

        return column_score

    def _compute_histogram(self):
        """
        Computes a 2D histogram based on orthometric heights and along-track distance.

        Returns:
            h (array): Computed histogram.
        """

        # backing histogram params from inputs
        xh = self.profile.data.x_ph.values
        yh = self.profile.data.z_ph.values

        at_min = np.min(self.bin_edges_at)
        at_max = np.max(self.bin_edges_at)

        z_min = np.min(self.bin_edges_z)
        z_max = np.max(self.bin_edges_z)
        z_bin_count = len(self.bin_edges_z) - 1
        at_bin_count = len(self.bin_edges_at) - 1

        # compute fast histogram over large photon data
        h_ = histogram2d(
            xh,
            yh,
            range=[[at_min, at_max], [z_min, z_max]],
            bins=[at_bin_count, z_bin_count],
        )

        g_sigma = 0.1 / np.diff(self.bin_edges_z)[0]

        h = gaussian_filter1d(h_, g_sigma, axis=1)

        return h

    def _interp_model_hist(self, hist):

        # move this to the actual model section

        # print('Going into interp model hist...')

        # print('Hist in shape: ', hist.shape)

        # interpolating between steps of data
        hist_interp = pd.DataFrame(hist).interpolate(
            method='linear', axis=0, limit=None, inplace=False, limit_area='inside')

        # using the first/last steps to fill the edges
        hist_interp.iloc[:self.window_size] = hist_interp.iloc[:self.window_size].fillna(
            method='backfill')

        hist_interp.iloc[-self.window_size:] = hist_interp.iloc[-self.window_size:].fillna(
            method='ffill')

        # print('Hist out shape: ', hist_interp.shape)

        return hist_interp
    
    def label_by_pseudowaveform(self, inplace=False):

        # currently busted, need to redo for new waveforms

        # on the dataframe with duplicates...
        # one hot encode the class labels
        # then groupby the index and take the mean to get each class confidence


        list_of_photon_dfs = [
            waveform.profile.data for wave_idx, waveform in self.waves.items()]

        data_with_dupes = pd.concat(list_of_photon_dfs)


        one_hot_classes = pd.DataFrame(-999, columns=['is_surface', 'is_bathymetry', 'is_column', 'is_background'],
                                    index=data_with_dupes.index)

        one_hot_classes.is_surface = data_with_dupes.classification == self.profile.label_help('surface')
        one_hot_classes.is_bathymetry = data_with_dupes.classification == self.profile.label_help('bathymetry')
        one_hot_classes.is_column = data_with_dupes.classification == self.profile.label_help('column')
        one_hot_classes.is_background = data_with_dupes.classification == self.profile.label_help('background')

        confidences = one_hot_classes.groupby(one_hot_classes.index).mean()
        confidences.columns = ['surface', 'bathymetry', 'column', 'background']
        # final_class = data_with_dupes.classification.groupby(one_hot_classes.index).apply(pd.Series.mode)

        # initialize and then update scores
        photon_class_data = pd.DataFrame(self.profile.label_help('unclassified'), # integer corresponding to unclassified label
                                           index=self.profile.data.index.values,
                                           columns=['classification', 'none', 'background', 'surface', 'column', 'bathymetry'])

        # handles if photons weren't included in a waveform
        # not sure why that's happening but seems to only be the first photon
        classed_photons = data_with_dupes.index.unique().values

        photon_class_data.loc[classed_photons,
                                'background'] = confidences.background.values
        photon_class_data.loc[classed_photons,
                                'surface'] = confidences.surface.values
        photon_class_data.loc[classed_photons,
                                'column'] = confidences.column.values
        photon_class_data.loc[classed_photons,
                                'bathymetry'] = confidences.bathymetry.values
        
        # check each of the conf values and get arg max

        # reorganize such that classes correspond to 0, 1, 2, 3
        # should work for class labels below, otherwise needs tweaking
        # {'surface': 3,
        #  'column': 2,
        #  'bathymetry': 1,
        #  'background': 0,
        #  'unclassified': -1,
        #  'none': -999}


        new_confidences = photon_class_data.loc[:, ['background', 'bathymetry', 'column', 'surface']].values

        max_column_labels = np.argmax(new_confidences, axis=1).astype(int)
        
        photon_class_data.loc[:, 'classification'] = max_column_labels

        if inplace:
            # update class in full profile data
            self.profile.data['classification'] = photon_class_data.classification

            # insert individual scores
            self.profile.data['conf_background'] = photon_class_data.background
            self.profile.data['conf_surface'] = photon_class_data.surface
            self.profile.data['conf_column'] = photon_class_data.column
            self.profile.data['conf_bathymetry'] = photon_class_data.bathymetry
            return None

        else:
            # return classes and confidence
            return photon_class_data

    def save_model_data(self, output_directory, prefix='FIXEDWIN_'):
        model_prefix = 'FIXEDWIN'
        formatted_filename_base = self.profile.get_formatted_filename(human_readable=False, use_short_name=True) \
            + '_' + model_prefix +'_' + self.ModelMaker.get_formatted_filename()

        if not os.path.exists(output_directory):
            os.makedirs(output_directory)
        
        # Save tabular data (photon elevations and classifications) as an ASCII file
        output_file = os.path.join(output_directory, formatted_filename_base + '_phclass.csv')
        columns_to_save = ['lon', 'lat', 'height', 'classification', 'conf_surface',
                        'conf_column', 'conf_bathymetry', 'conf_background',
                        'weight_surface', 'weight_bathymetry', 'subsurface_flag']
        self.profile.data[columns_to_save].to_csv(output_file, sep=',', index=True, header=True)

        # Save simpler data (processing resolution, etc.) as a JSON file
        metadata_file = os.path.join(output_directory, formatted_filename_base + '_metadata.json')
        metadata = {
            'res_z': self.res_z,
            'res_at': self.res_at,
            'bin_edges_z': self.bin_edges_z.tolist(),
            'bin_edges_at': self.bin_edges_at.tolist(),
            'window_size': self.window_size,
            'step_along_track': self.step_along_track
        }
        with open(metadata_file, 'w') as f:
            json.dump(metadata, f, indent=4)

        # Save model parameters as a CSV file
        params_file = os.path.join(output_directory, formatted_filename_base + '_model.csv')
        self.params.to_csv(params_file, sep=',', index=False, header=True)


