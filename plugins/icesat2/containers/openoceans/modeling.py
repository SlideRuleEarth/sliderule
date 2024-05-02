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

import numpy as np
import pandas as pd
from fast_histogram import histogram1d, histogram2d
from scipy.ndimage import gaussian_filter1d
from tqdm import tqdm

from waveform import Waveform


def round_if_integer(x, n=2):
    if round(x, n) == x:
        return np.int64(x)
    return x


def iterate_models(
    params: list,
    input_profile: Profile,
    range_z: tuple = (-45, 15),
    z_res_jitter_bounds: tuple = (0.1, 0.3),
    verbose: bool = False,
) -> list:
    """
    Iterate over a set of models defined by given parameters to process the input profile.

    Parameters:
    - params (list): List of tuples, each containing the resolution along track (float) and window size (int) for the model.
    - input_profile (Profile): The profile object to process.
    - range_z (tuple, optional): Range for the z-values (elevation). Default is (-45, 15).
    - z_res_jitter_bounds (tuple, optional): Bounds for the random jitter to apply on z-values. Default is (0.1, 0.3).
    - verbose (bool, optional): Whether to print verbose logs. Default is False.

    Returns:
    - list: List of processed model objects.
    """

    if verbose:
        print("{}-model ...".format(len(params)))
        print("Initiated at " + datetime.now().strftime("%H:%M:%S"))

    model_list = []

    for ii in range(len(params)):
        i_res_at, i_window = params[ii]

        header_str = " Model {}: Resolution: {} | Window: {}".format(
            ii + 1, i_res_at, i_window
        )
        if verbose:
            print(header_str)

        # randomly jitter the z value
        i_res_z = np.random.default_rng().uniform(
            z_res_jitter_bounds[0], z_res_jitter_bounds[1]
        )

        M_i = ModelMaker(
            res_along_track=i_res_at,
            res_z=i_res_z,
            range_z=range_z,
            window_size=i_window,
            fit=False,
            verbose=verbose,
        )

        # use the parameters of this model to process the profile from before
        m_i = M_i.process(input_profile)
        model_list.append(m_i)

    return model_list


def average_labels(profile, waveform_list):
    """
    Generates final labels for a given profile based on (potentially duplicated) photon labels. Scores are computed as the number of times a photon recieved a given label for how many times it was labeled.

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

    # handles if photons werent included in a waveform
    # not sure why thats happening but seems to only be the first photon

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


class ModelMaker:
    def __init__(
        self,
        res_along_track,
        res_z,
        window_size,
        range_z=(-100, 100),
        fit=False,
        verbose=False,
    ):
        """
        Initialize a ModelMaker instance.

        Args:
            res_along_track (float): The size of along-track bins for histogramming photon data, in meters.
            res_z (float): The size of elevation bins for histogramming photon data, in meters.
            window_size (float): Size of along-track window to aggregate when constructing pseudowaveforms.
                                 Must be an odd number.
            range_z (tuple, optional): Elevation range for binning, in meters. Defaults to (-100, 100).
            fit (bool, optional): Whether to fit the model or not. Defaults to False.
            verbose (bool, optional): Whether to print verbose output. Defaults to False.

        Raises:
            AssertionError: Raises error if window_size is not an odd number or less than 1.
        """
        assert window_size > 0, "window_size must be greater than 0"
        assert np.mod(window_size, 2) == 1, "window_size must be an odd number"

        self.res_along_track = res_along_track
        self.res_z = res_z
        self.range_z = range_z
        self.window_size = window_size
        self.step_along_track = 1  # Default step size along the track
        self.fit = fit
        self.verbose = verbose

        # Calculate the actual z resolution to make sure bins evenly fit within the specified elevation range
        bin_count_z = np.ceil((self.range_z[1] - self.range_z[0]) / self.res_z)
        res_z_actual = np.abs(self.range_z[1] - self.range_z[0]) / bin_count_z
        if res_z_actual != self.res_z:
            print(f"Adjusting z_res - new value: {res_z_actual:.4f}")
            self.res_z = res_z_actual

    def __str__(self):
        """
        Returns a string representation of the ModelMaker instance.
        """
        return (
            f"ModelMaker(res_along_track={self.res_along_track}, res_z={self.res_z}, "
            f"range_z={self.range_z}, window_size={self.window_size}, "
            f"step_along_track={self.step_along_track}, fit={self.fit}, verbose={self.verbose})"
        )

    def get_formatted_filename(self):
        """
        Generate a formatted filename based on the ModelMaker parameters.

        Returns:
            str: A string representing the filename based on parameters.
        """
        res_at = round_if_integer(self.res_along_track)
        res_z_100 = self.res_z * 100
        range_z_0 = -round_if_integer(self.range_z[0])
        range_z_1 = round_if_integer(self.range_z[1])
        win = self.window_size
        step_at = self.step_along_track

        # programmatically create string to concatenate self attributes res_along_track, res_z, range_z, window_size, step_along_track
        precision = 2
        model_id = f"{res_at:.0f}_\
{res_z_100:.0f}_{range_z_0:.0f}_\
{range_z_1:.0f}_{win:.0f}_\
{step_at:.0f}"

        return model_id

    def process(self, profile):
        """
        Process a given profile to produce a model.

        Args:
            profile: The profile object to be processed.

        Returns:
            Model: A Model object containing the processed data.
        """
        z_min = self.range_z[0]
        z_max = self.range_z[1]
        z_bin_count = np.int64(np.ceil((z_max - z_min) / self.res_z))

        # if signal has been identified, only use those photons
        if profile.signal_finding == True:
            data = copy.copy(profile.data.loc[profile.data.signal == True, :])

        else:
            data = copy.copy(profile.data)

        # along track bin sizing
        at_min = data.dist_ph_along_total.min()

        # calculating the range st the histogram output maintains exact along track values
        last_bin_offset = self.res_along_track - np.mod(
            (data.dist_ph_along_total.max() - data.dist_ph_along_total.min()),
            self.res_along_track,
        )

        at_max = data.dist_ph_along_total.max() + last_bin_offset
        at_bin_count = np.int64((at_max - at_min) / self.res_along_track)

        xh = data.dist_ph_along_total.values

        yh = data.z_ph.values

        bin_edges_at = np.linspace(at_min, at_max, num=at_bin_count + 1)
        bin_edges_z = np.linspace(z_min, z_max, num=z_bin_count + 1)

        # disctionary of waveform objects - (AT_bin_i, w)
        w_d = {}

        # list of organized/simple data series'
        d_list = []

        # array to store actual interpolated model and fitted model
        hist_modeled = np.nan * np.zeros((at_bin_count, z_bin_count))

        # at the center of each evaluation window
        # win centers needs to actually handle the center values unlike here
        # currently bugs out if win is 1

        # this step ensures that histograms at edges dont have lower 'intensity' just becasue the window exceeds the data range
        start_step = (self.window_size - 1) / 2
        end_step = len(bin_edges_at) - (self.window_size - 1) / 2 - 1

        win_centers = np.arange(
            np.int64(start_step), np.int64(end_step), self.step_along_track
        )

        for i in tqdm(win_centers):
            # get indices/at distance of evaluation window
            i_beg = np.int64(max((i - (self.window_size - 1) / 2), 0))
            i_end = (
                np.int64(min((i + (self.window_size - 1) / 2),
                         len(bin_edges_at) - 2))
                + 1
            )
            # + 1 above pushes i_end to include up to the edge of the bin when used as index

            at_beg = bin_edges_at[i_beg]
            at_end = bin_edges_at[i_end]

            # could be sped up with numba if this is an issue
            # subset data using along track distance window
            i_cond = (xh > at_beg) & (xh < at_end)
            df_win = data.loc[i_cond, :]

            # version of dataframe with only nominal photons
            # use this data for constructing waveforms
            df_win_nom = df_win.loc[df_win.quality_ph == 0]
            height_geoid = df_win_nom.z_ph.values

            # subset of histogram data in the evaluation window
            h_ = histogram1d(height_geoid, range=[
                             z_min, z_max], bins=z_bin_count)

            # smooth the histogram with 0.1 sigma gaussian kernel
            h = gaussian_filter1d(h_, 0.1 / self.res_z)

            # identify median lat lon values of photon data in this chunk
            x_win = df_win_nom.lon_ph.median()
            y_win = df_win_nom.lat_ph.median()
            at_win = df_win_nom.dist_ph_along_total.median()
            any_sat_ph = (df_win.quality_ph > 0).any()

            # copy profile to avoid overwriting
            w_profile = copy.copy(profile)

            # remove all data except for the photons in this window
            w_profile.data = df_win

            w = Waveform(
                np.flip(h),
                -np.flip(bin_edges_z),
                fit=self.fit,
                sat_check=any_sat_ph,
                profile=w_profile,
            )

            #################################################################

            # this next section is basically all reformatting and organizing the data
            # might be better elsewhere but keeping here so i dont have to
            # run through another loop

            # we also want to have the original photon data for photons in this chunk
            # may also be helpful to have track metadata/functions
            # this is for ALL photons, not just the nominal subset

            # w.add_profile(p)  # bug somewhere here returning nans

            # combine useful data into a nice dict for turning into a df
            extra_dict = {
                "i": i,  # indexing/location fields
                "x_med": x_win,
                "y_med": y_win,
                "at_med": at_win,
                "at_range": (at_beg, at_end),
                "quality_flag": w.quality_flag,
                "water_conf": -9999,
                "n_photons": df_win.shape[0],
            }

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

        # summary of model params
        params = pd.DataFrame(d_list)

        m = Model(
            params=params,
            model_hist=hist_modeled,
            bin_edges_z=bin_edges_z,
            bin_edges_at=bin_edges_at,
            window_size=self.window_size,
            waves=w_d,
            profile=copy.deepcopy(profile),
            ModelMaker=self,
        )

        return m


class Model:
    """Model class for processing and evaluating histograms."""

    def __init__(
        self,
        params,
        model_hist,
        bin_edges_z,
        bin_edges_at,
        window_size,
        waves,
        profile,
        ModelMaker,
    ):
        """
        Initialize the model with parameters, histogram data, and other associated components.

        Parameters
        ----------
        params : pd.DataFrame
            Parameters for the model. This DataFrame should contain relevant information
            needed for model construction and evaluation.

        model_hist : np.ndarray
            The pre-computed model histogram. This should be a 2D array that matches
            the size defined by `bin_edges_z` and `bin_edges_at`.

        bin_edges_z : np.ndarray
            The bin edges along the z-axis (elevation). This array should define the
            boundaries of the bins in the vertical direction.

        bin_edges_at : np.ndarray
            The bin edges along the track (along-track direction). This array should
            define the boundaries of the bins in the along-track direction.

        window_size : int
            How many neighboring bins to collect at each along-track step. Can be used to
            classify photons in more than one waveform analysis.

        waves : dict
            A dictionary containing waveforms, where each key-value pair consists of a key
            that is an identifier for the waveform and a value that is the waveform data.

        profile : object
            An object that contains profile data, including a DataFrame with classifications.

        ModelMaker : class
            A reference to the class responsible for making this Model. Used for callbacks
            and other internal functionalities.

        Attributes
        ----------
        hist : np.ndarray
            The processed histogram data, which might be smoothed or otherwise modified.

        model_hist_interp : pd.DataFrame
            An interpolated version of `model_hist` for more granular analysis.

        params : pd.DataFrame
            Updated DataFrame that now includes a 'turb_score' column.
        """
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
        self.params.loc[:, "turb_score"] = column_score

        # compute labels for photon data and update m.profile.data.classifications
        # _ = self.label_by_pseudowaveform(inplace=True)

        list_of_waveforms = self.waves.values()

        labels, confidences, weights, subsurface_score = average_labels(
            self.profile, list_of_waveforms
        )

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
        """
        Return a string representation of the model.

        Returns:
            str: A string containing the key attributes of the Model object.
        """
        # Calculate value_counts for profile classifications
        class_counts = self.profile.data.classification.value_counts().to_string()

        return (
            f"Model(\n"
            f"  params shape: {self.params.shape},\n"
            f"  model_hist shape: {self.model_hist.shape},\n"
            f"  bin_edges_z shape: {self.bin_edges_z.shape}, bin_edges_at shape: {self.bin_edges_at.shape},\n"
            f"  resolution (z, at): ({self.res_z}, {self.res_at}),\n"
            f"  window_size: {self.window_size},\n"
            f"  step_along_track: {self.step_along_track},\n"
            f"  number of waveforms: {len(self.waves)},\n"
            f"  profile classifications shape: {self.profile.data.classification.shape},\n"
            f"  profile classification counts:\n{class_counts}\n"
            f")"
        )

    def _compute_column_score(self):
        """
        Computes the column score based on the waveforms.

        Returns:
            column_score (array): Computed column score.
        """

        # index via wave keys or something else?
        n = len(self.waves.keys())
        column_score = np.zeros((n,))

        for i in self.params.index.values:
            # sum of column component
            # column_score[i] = np.sum(self.waves[i].model.column)

            # get wave indexing key
            i_wave_key = self.params.iloc[i].i

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
        xh = self.profile.data.dist_ph_along_total.values
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
        """
        Interpolates any missing data in the histogram, particularly filling edges.
        Honestly I'm not 100% why this is needed, but my guess is it was to avoid any NaNs propogating in later processing steps. (10/23)

        Args:
            hist (array): The model histogram to be interpolated.

        Returns:
            hist_interp (DataFrame): Interpolated histogram.
        """

        # interpolating between steps of data
        hist_interp = pd.DataFrame(hist).interpolate(
            method="linear", axis=0, limit=None, inplace=False, limit_area="inside"
        )

        # using the first/last steps to fill the edges
        hist_interp.iloc[: self.window_size] = hist_interp.iloc[
            : self.window_size
        ].fillna(method="backfill")

        hist_interp.iloc[-self.window_size:] = hist_interp.iloc[
            -self.window_size:
        ].fillna(method="ffill")

        return hist_interp

    def label_by_pseudowaveform(self, inplace=False):
        """
        Label data points based on pseudo-waveform analysis. Uses the highest component of the waveform model
        (noise, bathy, surface, column) to assign classifications to each photon in the waveform profile object.

        Args:
            inplace (bool, optional): Whether to update the profile data in place. Default is False.

        Returns:
            pd.DataFrame or None: Dataframe with new classifications, or None if inplace=True.
        """

        # currently busted, need to redo for new waveforms

        # on the dataframe with duplicates...
        # one hot encode the class labels
        # then groupby the index and take the mean to get each class confidence

        list_of_photon_dfs = [
            waveform.profile.data for wave_idx, waveform in self.waves.items()
        ]

        data_with_dupes = pd.concat(list_of_photon_dfs)

        one_hot_classes = pd.DataFrame(
            -9999,
            columns=["is_surface", "is_bathymetry",
                     "is_column", "is_background"],
            index=data_with_dupes.index,
        )

        one_hot_classes.is_surface = (
            data_with_dupes.classification == self.profile.label_help(
                "surface")
        )
        one_hot_classes.is_bathymetry = (
            data_with_dupes.classification == self.profile.label_help(
                "bathymetry")
        )
        one_hot_classes.is_column = (
            data_with_dupes.classification == self.profile.label_help("column")
        )
        one_hot_classes.is_background = (
            data_with_dupes.classification == self.profile.label_help(
                "background")
        )

        confidences = one_hot_classes.groupby(one_hot_classes.index).mean()
        confidences.columns = ["surface", "bathymetry", "column", "background"]
        # final_class = data_with_dupes.classification.groupby(one_hot_classes.index).apply(pd.Series.mode)

        # initialize and then update scores
        photon_class_data = pd.DataFrame(
            self.profile.label_help(
                "unclassified"
            ),  # integer corresponding to unclassified label
            index=self.profile.data.index.values,
            columns=[
                "classification",
                "none",
                "background",
                "surface",
                "column",
                "bathymetry",
            ],
        )

        # handles if photons werent included in a waveform
        # not sure why thats happening but seems to only be the first photon
        classed_photons = data_with_dupes.index.unique().values

        photon_class_data.loc[
            classed_photons, "background"
        ] = confidences.background.values
        photon_class_data.loc[classed_photons,
                              "surface"] = confidences.surface.values
        photon_class_data.loc[classed_photons,
                              "column"] = confidences.column.values
        photon_class_data.loc[
            classed_photons, "bathymetry"
        ] = confidences.bathymetry.values

        new_confidences = photon_class_data.loc[
            :, ["background", "bathymetry", "column", "surface"]
        ].values

        max_column_labels = np.argmax(new_confidences, axis=1).astype(int)

        photon_class_data.loc[:, "classification"] = max_column_labels

        if inplace:
            # update class in full profile data
            self.profile.data["classification"] = photon_class_data.classification

            # insert indivual scores
            self.profile.data["conf_background"] = photon_class_data.background
            self.profile.data["conf_surface"] = photon_class_data.surface
            self.profile.data["conf_column"] = photon_class_data.column
            self.profile.data["conf_bathymetry"] = photon_class_data.bathymetry
            return None

        else:
            # return classes and confidence
            return photon_class_data

    def save_model_data(self, output_directory, prefix="FIXEDWIN_"):
        """
        Save model data to disk.

        Args:
            output_directory (str): Directory where the output files will be saved.
            prefix (str, optional): Prefix to append to output filenames. Default is 'FIXEDWIN_'.
        """
        model_prefix = "FIXEDWIN"
        formatted_filename_base = (
            self.profile.get_formatted_filename(
                human_readable=False, use_short_name=True
            )
            + "_"
            + model_prefix
            + "_"
            + self.ModelMaker.get_formatted_filename()
        )

        if not os.path.exists(output_directory):
            os.makedirs(output_directory)

        # Save tabular data (photon elevations and classifications) as an ASCII file
        output_file = os.path.join(
            output_directory, formatted_filename_base + "_phclass.csv"
        )
        columns_to_save = [
            "photon_index",
            "lon",
            "lat",
            "height",
            "classification",
            "conf_surface",
            "conf_column",
            "conf_bathymetry",
            "conf_background",
            "weight_surface",
            "weight_bathymetry",
            "subsurface_flag",
        ]
        self.profile.data[columns_to_save].to_csv(
            output_file, sep=",", index=True, header=True
        )

        # Save simpler data (processing resolution, etc.) as a JSON file
        metadata_file = os.path.join(
            output_directory, formatted_filename_base + "_metadata.json"
        )
        metadata = {
            "res_z": self.res_z,
            "res_at": self.res_at,
            "bin_edges_z": self.bin_edges_z.tolist(),
            "bin_edges_at": self.bin_edges_at.tolist(),
            "window_size": self.window_size,
            "step_along_track": self.step_along_track,
        }
        with open(metadata_file, "w") as f:
            json.dump(metadata, f, indent=4)

        # Save model parameters as a CSV file
        params_file = os.path.join(
            output_directory, formatted_filename_base + "_model.csv"
        )
        self.params.to_csv(params_file, sep=",", index=False, header=True)


if __name__ == "__main__":
    pass
