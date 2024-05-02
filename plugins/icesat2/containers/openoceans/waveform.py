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
import warnings

import numpy as np
import pandas as pd
from scipy.ndimage import gaussian_filter1d
from scipy.optimize import OptimizeWarning, curve_fit
from scipy.signal import find_peaks, peak_widths
from scipy.stats import norm
from sklearn.mixture import GaussianMixture

pd.options.mode.chained_assignment = None


def surface_confidence(z_inv: np.array, params, model_interp: pd.DataFrame) -> np.array:
    """
    Calculate the confidence of the surface for a given elevation model.

    Parameters:
    z_inv (np.array): Inverted elevation points.
    params: Parameters used for the surface and background models.
    model_interp (pd.DataFrame): Interpolated model dataframe with 'z_inv', 'surface', and 'background' columns.

    Returns:
    np.array: Array of confidence values for the surface.
    """
    conf_surface = np.zeros_like(z_inv)
    surf_above_background = (
        np.clip(model_interp.surface - model_interp.background, 0, None) > 0
    )

    if np.any(surf_above_background):
        surf_peak_weight_max = (
            model_interp.loc[:, ["surface_1", "surface_2"]].max(axis=0).max()
        )
        conf_surface = (
            np.interp(z_inv, model_interp.z_inv, model_interp.surface)
            / surf_peak_weight_max
        )
        conf_surface = np.clip(conf_surface, 0, 1)

    conf_surface[conf_surface < 1e-5] = 0

    return conf_surface


def bathy_confidence(z_inv: np.array, params, model_interp: pd.DataFrame) -> np.array:
    """
    Calculate the confidence for bathymetry.

    Parameters:
    z_inv (np.array): Inverted elevation points.
    params: Parameters including bathymetry location and standard deviation.
    model_interp (pd.DataFrame): Interpolated model dataframe.

    Returns:
    np.array: Array of confidence values for bathymetry.
    """
    conf_bathymetry = np.zeros_like(z_inv)
    bathy_gauss_pdf = gauss_pdf(z_inv, params.bathy_loc, params.bathy_std)
    conf_bathymetry = bathy_gauss_pdf / np.max(bathy_gauss_pdf)
    conf_bathymetry = np.clip(conf_bathymetry, 0, 1)

    # Exclude outliers beyond 3 standard deviations
    conf_bathymetry[z_inv < (params.bathy_loc - (3 * params.bathy_std))] = 0
    conf_bathymetry[z_inv > (params.bathy_loc + (3 * params.bathy_std))] = 0

    return conf_bathymetry


def compute_feature_weights(
    z_ph: np.array, params, model_interp: pd.DataFrame, bathy_flag: bool
) -> pd.DataFrame:
    """
    Compute feature weights for surface and bathymetry based on the input parameters.

    Parameters:
    z_ph (np.array): Elevation points.
    params: Parameters for the model.
    model_interp (pd.DataFrame): Interpolated model dataframe.
    bathy_flag (bool): Flag to indicate if bathymetry is present.

    Returns:
    pd.DataFrame: Dataframe containing computed confidence levels for surface and bathymetry.
    """
    z_inv = -z_ph

    if bathy_flag:
        conf_bathymetry = bathy_confidence(z_inv, params, model_interp)
    else:
        conf_bathymetry = np.zeros_like(z_inv)

    conf_surface = surface_confidence(z_inv, params, model_interp)

    class_confidence = pd.DataFrame(
        {
            "z": z_ph,
            "z_inv": -z_ph,
            "surface": conf_surface,
            "bathymetry": conf_bathymetry,
        },
        dtype=np.float64,
    )

    return class_confidence


def check_histogram_badness(
    inv_z_bins: np.array, histogram: np.array, peaks: pd.DataFrame
) -> int:
    """
    Evaluate the quality of a histogram based on the presence of peaks, data distribution, and noise.

    Parameters:
    inv_z_bins (np.array): Bins for inverted elevation.
    histogram (np.array): Elevation histogram data.
    peaks (pd.DataFrame): Dataframe containing peaks with 'heights' column.

    Returns:
    int: A quality flag indicating the goodness of the histogram.
         0: Good histogram
        -1: Insufficient data
        -2: No peaks
        -3: Likely noise
        -4: Peaks too small to assume signal exists
    """
    z_bin_size = np.unique(np.diff(inv_z_bins))[0]
    quality_flag = 0

    # Check for minimum data coverage
    if np.sum(histogram != 0) <= 0.5 / z_bin_size:
        quality_flag = -1

    # Check for the presence of peaks
    if peaks.shape[0] == 0:
        quality_flag = -2

    # Check for noise in the presence of more than two peaks
    if peaks.shape[0] > 2:
        similar_secondary = (
            peaks.iloc[1].heights / peaks.iloc[0].heights) > 0.6
        similar_tertiary = (
            peaks.iloc[2].heights / peaks.iloc[0].heights) > 0.6

        if similar_secondary and similar_tertiary:
            quality_flag = -3

    # check the height of the largest peak
    # recall there's some gaussian filter smoothing so this isn't exactly a photon count thing
    # weak beam may struggle to accumulate lots of photons for calm water
    if peaks.shape[0] > 0:
        if (peaks.iloc[0].prominences < 1):
            quality_flag = -4

    return quality_flag


def exp_spdf(z_inv, z_inv_0, I_0, decay_param):
    """Exponential decay model.

    Args:
        z_inv (array of float): Inverted depths.
        z_inv_0 (float): Start depth for the exponential decay.
        I_0 (float): Initial intensity.
        decay_param (float): Decay parameter.

    Returns:
        array: Exponential model output, matching shape of input depth array.
    """
    y_out = np.zeros_like(z_inv)
    z_depth = z_inv[z_inv >= z_inv_0] - z_inv_0
    y_out[z_inv >= z_inv_0] = I_0 * np.exp(decay_param * z_depth)
    return y_out


# COMPONENT DISTRIBUTION MODELS


def uniform_spdf(inv_z_bins, z_top, z_bot, magnitude):
    """Uniform model.

    Args:
        inv_z_bins (array of float): Inverted depths.
        z_top (float): Top depth of the uniform model.
        z_bot (float): Bottom depth of the uniform model.
        magnitude (float): Magnitude of the uniform model.

    Returns:
        array: Uniform model output, matching shape of input depth array.
    """
    return magnitude * (inv_z_bins >= z_top) * (inv_z_bins <= z_bot)


def gauss(x, mag, mu, sigma):
    """Gaussian function.

    Args:
        x (array): Input values.
        mag (float): Magnitude.
        mu (float): Mean.
        sigma (float): Standard deviation.

    Returns:
        array: Gaussian function output, matching shape of input array.
    """
    return (
        mag
        * (1 / np.sqrt(2 * np.pi * sigma**2))
        * np.exp(-((x - mu) ** 2) / (2 * sigma**2))
    )


def gauss_pdf(x, mean, stddev):
    """Gaussian PDF.

    Args:
        x (array): Input values.
        mean (float): Mean.
        stddev (float): Standard deviation.

    Returns:
        array: Gaussian PDF output, matching shape of input array.
    """
    return np.exp(-((x - mean) ** 2) / (2 * stddev**2)) / (
        np.sqrt(2 * np.pi) * stddev
    )


def gauss_mixture(x, mu1, std1, mu2, std2, w1, scaling):
    """Gaussian Mixture Model.

    Args:
        x (array): Input values.
        mu1, mu2 (float): Means for Gaussians 1 and 2.
        std1, std2 (float): Standard deviations for Gaussians 1 and 2.
        w1 (float): Weight for Gaussian 1.
        scaling (float): Scaling factor.

    Returns:
        array: Gaussian Mixture Model output, matching shape of input array.
    """
    w2 = 1 - w1
    return scaling * (w1 * gauss_pdf(x, mu1, std1) + w2 * gauss_pdf(x, mu2, std2))


# IDENTIFY AND CHARACTERIZE PEAKS IN THE WAVEFORM


def get_peak_info(hist, z_inv, verbose=False):
    """
    Evaluates input histogram to find peaks and associated peak statistics.

    Args:
        hist (array): Histogram of photons by z_inv.
        z_inv (array): Centers of z_inv bins used to histogram photon returns.
        verbose (bool, optional): Option to print output and warnings. Defaults to False.

    Returns:
        Pandas DataFrame with the following columns:
            - i: Peak indices in the input histogram.
            - prominences: Prominence of the detected peaks.
            - left_bases, right_bases: Indices of left and right bases of the peaks.
            - left_z, right_z: z_inv values of left and right bases of the peaks.
            - heights: Heights of the peaks.
            - fwhm: Full Width at Half Maximum of the peaks.
            - left_ips_hm, right_ips_hm: Left and right intersection points at half maximum.
            - widths_full: Full width of peaks.
            - left_ips_full, right_ips_full: Left and right intersection points at full width.
            - sigma_est_i: Estimated standard deviation indices.
            - sigma_est: Estimated standard deviation in units of z_inv.
            - prom_scaling_i, mag_scaling_i: Scaling factors for prominences and magnitudes using indices.
            - prom_scaling, mag_scaling: Scaling factors for prominences and magnitudes in units of z_inv.
            - z_inv: z_inv values at the peak indices.
    """

    z_inv_bin_size = np.unique(np.diff(z_inv))[0]


    # padding elevation mapping for peak finding at edges
    z_inv_padded = z_inv

    # left edge
    z_inv_padded = np.insert(
        z_inv_padded, 0, z_inv[0] - z_inv_bin_size
    )  # use zbin for actual fcn

    # right edge
    z_inv_padded = np.insert(
        z_inv_padded, len(z_inv_padded), z_inv[-1] + z_inv_bin_size
    )  # use zbin for actual fcn

    dist_req_between_peaks = 0.249  # m

    if dist_req_between_peaks / z_inv_bin_size < 1:
        warn_msg = """get_peak_info(): Vertical bin resolution is greater than the req. min. distance 
        between peak. Setting req. min. distance = z_inv_bin_size. Results may not be as expected.
        """
        if verbose:
            warnings.warn(warn_msg)
        dist_req_between_peaks = z_inv_bin_size

    # note: scipy doesn't identify peaks at the start or end of the array
    # so zeros are inserted on either end of the histogram and output indexes adjusted after

    # distance = distance required between peaks - use approx 0.5 m, accepts floats >=1
    # prominence = required peak prominence
    pk_i, pk_dict = find_peaks(
        np.pad(hist, 1),
        distance=dist_req_between_peaks / z_inv_bin_size,
        prominence=0.01,
    )

    # evaluating widths with find_peaks() seems to be using it as a threshold - not desired
    # width = required peak width (index) - use 1 to return all
    # rel_height = how far down the peak to measure its width
    # 0.5 is half way down, 1 is measured at the base
    # approximate stdm from the full width and half maximum
    (
        pk_dict["fwhm"],
        pk_dict["width_heights_hm"],
        pk_dict["left_ips_hm"],
        pk_dict["right_ips_hm"],
    ) = peak_widths(np.pad(hist, 1), pk_i, rel_height=0.5) #0.4?

    # calculate widths at full prominence, more useful than estimating peak width by std
    (
        pk_dict["widths_full"],
        pk_dict["width_heights_full"],
        pk_dict["left_ips_full"],
        pk_dict["right_ips_full"],
    ) = peak_widths(np.pad(hist, 1), pk_i, rel_height=1)

    # organize into dataframe for easy sorting and reindex
    pk_dict["i"] = pk_i - 1
    pk_dict["heights"] = hist[pk_dict["i"]]

    # draw a horizontal line at the peak height until it cross the signal again
    # min values within that window identifies the bases
    # preference for closest of repeated minimum values
    # ie. can give weird values to the left/right of the main peak, and to the right of a bathy peak
    # when there's noise in a scene with one 0 bin somewhere far
    pk_dict["left_z"] = z_inv_padded[pk_dict["left_bases"]]
    pk_dict["right_z"] = z_inv_padded[pk_dict["right_bases"]]
    pk_dict["left_bases"] -= 1
    pk_dict["right_bases"] -= 1

    # left/right ips = interpolated positions of left and right intersection points
    # of a horizontal line at the respective evaluation height.
    # mapped to input indices so needs adjustingpk_dict['left_ips'] -= 1
    pk_dict["right_ips_hm"] -= 1
    pk_dict["left_ips_hm"] -= 1
    pk_dict["right_ips_full"] -= 1
    pk_dict["left_ips_full"] -= 1

    # estimate gaussian STD from the widths
    # sigma estimate in terms of int indexes
    pk_dict["sigma_est_i"] = pk_dict["fwhm"] / 2.35
    # sigma estimate in terms of int indexes
    pk_dict["sigma_est_left_i"] = 2 * \
        (pk_dict["i"] - pk_dict["left_ips_hm"]) / 2.35
    # sigma estimate in terms of int indexes
    pk_dict["sigma_est_right_i"] = 2 * \
        (pk_dict["right_ips_hm"] - pk_dict["i"]) / 2.35

    # sigma estimate in terms meters
    pk_dict["sigma_est"] = z_inv_bin_size * pk_dict["sigma_est_i"]
    pk_dict["sigma_est_left"] = z_inv_bin_size * pk_dict["sigma_est_left_i"]
    pk_dict["sigma_est_right"] = z_inv_bin_size * pk_dict["sigma_est_right_i"]

    # approximate gaussian scaling factor based on prominence or magnitudes
    # for gaussians range indexed
    pk_dict["prom_scaling_i"] = pk_dict["prominences"] * (
        np.sqrt(2 * np.pi) * pk_dict["sigma_est_i"]
    )
    pk_dict["mag_scaling_i"] = pk_dict["heights"] * (
        np.sqrt(2 * np.pi) * pk_dict["sigma_est_i"]
    )

    # for gaussians mapped to z
    pk_dict["prom_scaling"] = pk_dict["prominences"] * (
        np.sqrt(2 * np.pi) * pk_dict["sigma_est"]
    )
    pk_dict["mag_scaling"] = pk_dict["heights"] * (
        np.sqrt(2 * np.pi) * pk_dict["sigma_est"]
    )
    pk_dict["z_inv"] = z_inv[pk_dict["i"]]

    peak_info = pd.DataFrame.from_dict(pk_dict, orient="columns", dtype=np.float64)

    # compute peak area approximation using prominence (instead of magnitude)
    peak_info['area'] = peak_info['prominences'] * peak_info['sigma_est'] / 0.3989
    peak_info['area_by_height'] = peak_info['heights'] * peak_info['sigma_est'] / 0.3989
    peak_info['subsurface_peak'] = False # preallocated
    
    return peak_info # note that returned data is not sorted!


def calculate_photons(row, z_inv):
    # Used for computing how many photons fall inside a peak given some dataframe of photon data
    # Row corresponds to rows pulled from peak info, with basic sigma estimates
    # Determine the range of z_inv values that fall within the peak
    lower_bound = row.z_inv - row.sigma_est_left
    upper_bound = row.z_inv + row.sigma_est_right
    
    # Calculate the number of photons within the peak
    ph_within_peak = (z_inv > lower_bound) & (z_inv < upper_bound)
    return ph_within_peak.sum()


# ESTIMATE COMPONENT DISTRIBUTIONS
# This is the main workhorse of the waveform.py module


def estimate_model_params(hist, z_inv, peak_info=None, verbose=False, profile=None):
    # ph elevations need to be geoid corrected
    """Calculates parameters for noise, surface, bathy, and turbidity model components from statistical approximations. Also outputs parameter bounds to use for curve fitting and a quality flag (in progress).

    Quality flag interpretation:
        -2 Photons with no distinct peaks.
        -1 Not enough data (less than 1m worth of bins).
        0  Not set
        1  Surface peak exists, but no subsurface peaks found.
        2  Surface peak and weak bathy peak identified (prominence < noise_below * thresh).
        3  Surface peak and strong bathy peak identified (prominence > noise_below * thresh).

    Args:
        hist (array): Histogram of photons by depth.
        depth (array): Centers of depth bins used to histogram photon returns.
        peaks (DataFrame): Peak info statistics. If None, will calculate from get_peak_info().
        verbose (bool, optional): Option to output warnings/print statements. Defaults to False.

    Returns:
        params_out (dict): Model parameters. See histogram_model() docstring for more detail.
        quality_flag (int): Flag summarizing parameter confidence.
    """
    if isinstance(peak_info, pd.DataFrame):
        pass
    else:
        peak_info = get_peak_info(hist, z_inv)

    # peaks are unsorted, lets sort them by prominence for surface and quality checking
    peak_info = peak_info.sort_values(by="prominences", ascending=False)

    # use profile of photon data to compute how many photons (approximately) fall within each peak
    # this is used later for knowing the 'magnitude' of the peak regardless of z_bin size and gaussian smoothing
    # testing different z_bin_sizes and gaussian smoothings and presently hard coded values are killing me, this is my fix for now
    peak_info['n_photons'] = peak_info.apply(calculate_photons, z_inv=-profile.data.z_ph, axis=1)

    # note that bathy peaks (lower elevation peaks) are sorted by area later, but get their own df for analysis

    # peak_info = peak_info
    zero_val = 1e-31
    quality_flag = False
    params_out = {
        "surf_scaling": np.nan,  # scaling factor for gaussian mixture model of surface
        # mean of gaussian component 1 in terms of z_inv (change later)
        "surf_loc_1": np.nan,
        # standard deviation of gaussian component 1 (m)
        "surf_std_1": np.nan,
        "surf_weight_1": np.nan,  # weight of gaussian component 1
        # mean of gaussian component 2 in terms of z_inv (change later)
        "surf_loc_2": np.nan,
        # standard deviation of gaussian component 2 (m)
        "surf_std_2": np.nan,
        "surf_weight_2": np.nan,  # weight of gaussian component 2
        # elevation of the top of the attenuation model (the max bin in the surface), ie. for approximate depth
        "decay_top_z": np.nan,
        "decay_param": np.nan,  # decay param, important to start with a low value
        "turb_intens": np.nan,  # initial intensity of the attentuaion model at the decay_top_z
        # intersection of the surface and the attenuation model
        "column_top": np.nan,
        # background noise rate (count)
        "background": np.nan,
        # prominence of potential bathy peak (count)
        "bathy_mag": np.nan,
        # location of potential bathy peak (m)
        "bathy_loc": np.nan,
        "bathy_std": np.nan,
    }  # deviation of potential bathy peak (m)

    # corresponding to data that's been successfully evaluated, just has no good data (eg all clouds)
    # values allow computation to proceed and assigns photons to background class
    params_empty = {
        "surf_scaling": zero_val,
        "surf_loc_1": 2 * zero_val,  # avoid collocated peaks
        "surf_std_1": zero_val,
        "surf_weight_1": 1,
        "surf_loc_2": zero_val,
        "surf_std_2": zero_val,
        "surf_weight_2": zero_val,
        "decay_top_z": zero_val,
        "decay_param": -zero_val,
        "turb_intens": zero_val,
        "column_top": zero_val,
        # higher value means photons will be classified as background
        "background": 2 * zero_val,
        "bathy_mag": zero_val,
        "bathy_loc": zero_val,
        "bathy_std": zero_val,
    }

    z_bin_size = np.unique(np.diff(z_inv))[0]

    # ##################### QUALITY CHECKS ON HISTOGRAM ##################################

    # 0: nominal
    # -1: not enough data
    # -2: no peaks
    # -3: no dominant peak

    histogram = hist

    quality_flag = check_histogram_badness(z_inv, histogram, peak_info)

    if quality_flag != 0:
        # dont just use nans, construct the zeroed out model

        params_out = pd.Series(params_empty, name="params", dtype=np.float64)
        bathy_quality_ratio = -1
        surface_gm = None

        return params_out, quality_flag, bathy_quality_ratio, surface_gm, peak_info

    # ########################## ESTIMATING SURFACE PEAK PARAMETERS #############################

    # Surface return - largest peak

    # if the top two peaks are within 20% of each other
    # go with the higher elev one/the one closest to 0m
    # mitigating super shallow, bright reefs where seabed is actually brighter

    if peak_info.shape[0] > 1:

        # if area 2 is within 5m below peak 1's z_inv location and more than 30% as tall
        # likely a shallow bathy / calm water case
        # switch rows 1 and 2 to keep the true water surface first in the dataframe
        # same function as the prominence based check below, but applied for peak area

        switch_peaks_threshold = 0.2

        # check if second peak is greater than 20% the prominence of the primary
        # this is really a concern in cases of strong bathymetry, and where many photons are expected to be returned
        # if applied everywhere, noise just above the surface can be detected as the next largest peak and reassigned surface
        # so also check magnitude
        n_photons_threshold = 10

        two_tall_peaks = (((peak_info.iloc[1].heights) / peak_info.iloc[0].heights) > switch_peaks_threshold) & \
            ((peak_info.iloc[1].n_photons > n_photons_threshold) & (peak_info.iloc[0].n_photons > n_photons_threshold))

        # # check if the second peak is within 5m below the primary z_inv location
        # # and if the second peak is more than 30% as large by area as the primary
        # # mitigating shallow bathy cases with calm water as above, but using area for consistency 
        # two_large_peaks = ((peak_info.iloc[0].z_inv - peak_info.iloc[1].z_inv) < 5) & ((peak_info.iloc[1].area / peak_info.iloc[0].area) > switch_peaks_threshold)

        if two_tall_peaks:# and two_large_peaks:
            # use the peak on top of the other
            # anywhere truly open water will have the surface as the top peak
            pks2 = peak_info.iloc[[0, 1]]
            peak_above = pks2.z_inv.argmin()
            surf_pk = peak_info.iloc[peak_above]

        else:
            surf_pk = peak_info.iloc[0]
    else:
        surf_pk = peak_info.iloc[0]

    

    # reconstruct peak_info with the surface peak first, and the remaining peaks sorted by area
    peak_info = pd.concat([surf_pk.to_frame().T, peak_info.drop(surf_pk.name).sort_values(by=['area', 'z_inv'], ascending=[False, False])])


    # estimate noise rates above surface peak and top of surface peak

    # dont use ips, it will run to the edge of an array with a big peak
    # using std estimate of surface peak instead
    surface_peak_left_edge_i = np.int64(
        np.floor(surf_pk.i - 3 * surf_pk.sigma_est_left_i)
    )

    # distance above surface peak to start estimating noise

    height_above_surf_noise_est = 5  # meters

    above_surface_noise_left_edge_i = surface_peak_left_edge_i - (
        height_above_surf_noise_est / z_bin_size
    )

    # if there's not 15m above surface, use what there is above the surface
    if above_surface_noise_left_edge_i <= 0:
        above_surface_noise_left_edge_i = 0

    above_surface_idx_range = np.arange(
        above_surface_noise_left_edge_i, surface_peak_left_edge_i, dtype=np.int64
    )

    # above surface estimation
    if surface_peak_left_edge_i <= 0:
        # no bins above the surface
        params_out["background"] = zero_val

    else:
        # median of all bins above the peak
        params_out["background"] = (
            np.mean(hist[above_surface_idx_range]) + zero_val
        )  # eps to avoid 0

    # use above surface data to refine the estimate of the top of the surface
    # top of the surface is the topmost surface bin with a value greater than the noise above

    # photons through the top of the surface
    top_to_surf_center_i = np.arange(surf_pk.i + 1, dtype=np.int64)
    less_than_noise_bool = hist[top_to_surf_center_i] <= (
        2 * params_out["background"])
    
    # found just using the noise estimate x 1 leads to too wide of a window in quick testing, using x2 for now

    # get the lowest elevation bin higher than the above surface noise rate
    # set as the top of the surface

    if less_than_noise_bool.any():
        surface_peak_left_edge_i = np.where(less_than_noise_bool)[0][-1]

    # else defaults to 3 sigma range defined above

    # but this is a rough estimate of the surface peak assuming perfect gaussian on peak
    # surface peak is too important to leave this rough estimate
    # surf peak can have a turbid subsurface tail, too

    # elbow detection - where does the surface peak end and become subsurface noise/signal?
    # basically mean value theorem applied to the subsurface histogram slope
    # how to know when the histogram starts looking like possible turbidity?
    # we assume the surface should dissipate after say X meters depth, but will sooner in actuality
    # therefore, the actual numerical slope must cross the threshold required to dissipate after X meters at some point
    # where X is a function of surface peak width

    # what if the histogram has a rounded surface peak, or shallow bathy peaks?
    # rounded surface peak would be from pos to neg (excluded)
    # shallow bathy peaks would be the 2nd or 3rd slope threshold crossing
    # some sketches with the curve and mean slope help show the sense behind this

    # we'll use this to improve our estimate of the surface gaussian early on
    # elbow detection start
    dissipation_range = 10  # m # was originally 3m but not enough for high turbid cases
    slope_thresh = -surf_pk.heights / (dissipation_range / z_bin_size)
    diffed_subsurf = np.diff(hist[np.int64(surf_pk.i):])

    # detection of slope decreasing in severity, crossing the thresh
    sign_ = np.sign(diffed_subsurf - slope_thresh)
    # ie where sign changes (from negative to positive only)
    sign_changes_i = np.where(
        (sign_[:-1] != sign_[1:]) & (sign_[:-1] < 0))[0] + 1

    if len(sign_changes_i) == 0:
        no_sign_change = True
        # basically aa surface at the bottom of window somehow
        quality_flag = -4
        params_out = pd.Series(params_out, name="params", dtype=np.float64)
        bathy_quality_ratio = -1
        surface_gm = None
        return params_out, quality_flag, bathy_quality_ratio, surface_gm, peak_info

    else:
        # calculate corner details
        transition_i = np.int64(surf_pk.i) + \
            sign_changes_i[0]  # full hist index

        # bottom bound of surface peak in indices
        surface_peak_right_edge_i = transition_i + 1
        params_out["column_top"] = -z_inv[surface_peak_right_edge_i]

    # end elbow detection

    if surface_peak_right_edge_i > len(hist):
        surface_peak_right_edge_i = len(hist)

    surf_range_i = np.arange(
        surface_peak_left_edge_i, surface_peak_right_edge_i, dtype=np.int64
    )

    # initialize values for a single gaussian guessed from the peak statistics
    mu1 = surf_pk.z_inv
    std1 = surf_pk.sigma_est_i * z_bin_size
    gaussian_pdf = norm.pdf(z_inv, mu1, std1)
    mu2 = mu1
    std2 = zero_val
    w1 = 1
    # _surf_gm_scaling = 1
    _surf_gm_scaling = np.max(hist) / np.max(gaussian_pdf)
    _surf_mag = surf_pk.heights

    mixture_model_flag = False

    # # if the photon data has been supplied, we can use it to optimize the surface gaussian mixture
    if profile is not None and mixture_model_flag:
        try:
            z_ph = profile.data.z_ph
            z_surf = z_ph[
                (z_ph < -z_inv[surface_peak_left_edge_i])
                & (z_ph > -z_inv[surface_peak_right_edge_i])
            ]
            z_inv_surf = -z_surf

            X = np.vstack([z_inv_surf]).T

            # initialize non-zero gaussian mixture at estimated values
            w1 = 0.9
            w2 = 0.1

            mu2 = mu1 + 1.5 * std1

            gm = GaussianMixture(
                n_components=2, tol=1e-3, max_iter=100, random_state=33
            ).fit(X)

            # reconstruct x values for getting total histogram scaling
            gm_x = z_inv.reshape(-1, 1)

            logprob = gm.score_samples(gm_x)

            # scale from pdf to match actual data
            _surf_gm_scaling = len(z_inv_surf) / sum(np.exp(logprob))
            _surf_mag = max(_surf_gm_scaling * np.exp(logprob))

            # get values from mixture model instance
            gm_w = gm.weights_.flatten()
            gm_mu = gm.means_.flatten()
            gm_cov = gm.covariances_.flatten()
            gm_std = np.sqrt(gm_cov)

            std1 = gm_std[0]
            mu1 = gm_mu[0]
            w1 = gm_w[0]

            std2 = gm_std[1]
            mu2 = gm_mu[1]

            surface_gm = gm

        except:
            surface_gm = None
            if verbose:
                print("Surface mixture model failed to converge (1)")

    else:

        surface_gm = None
        if verbose:
            print("Using single gaussian for surface - either missing photon data or mixture model turned off manually.")

    # check that the surface peaks aren't too far apart (3 meters max, but totally arbitrary)
    # more common for actual land, but helps avoid bad looking data on the model

    # get whichever peak is wider
    # require that the other peak be within X meters

    if np.abs(mu1 - mu2) > 5:
        # use the single gaussian estimate from before the mixture model
        mu1 = surf_pk.z_inv
        std1 = surf_pk.sigma_est_i * z_bin_size
        mu2 = mu1
        std2 = zero_val
        w1 = 1
        _surf_gm_scaling = 1
        _surf_mag = surf_pk.heights

    _surf_loc_1 = mu1
    _surf_sigma_1 = std1
    _surf_weight_1 = w1

    _surf_loc_2 = mu2
    _surf_sigma_2 = std2
    _surf_weight_2 = 1 - w1

    # FINAL QUALITY CHECK ON SURFACE GAUSSIAN
    # add check on how well the 'surface' gaussian has to be defined
    # in particular, want to avoid very short/squat gaussians
    # so, a basic heuristic determined completely subjectively
    # - if the final, best fit surface gaussian is wider than it is tall
    # - its a bad surface peak and should be ignored

    # # commented out after updating surface model to use gaussian mixture
    # # leaving for now until refactor complete
    # if (_surf_mag / (6 * _surf_sigma_1)) < 1:
    #     # surface peak is too squat / poorly defined
    #     # exit pseudowaveform parameter estimation
    #     quality_flag = -4
    #     params_out = pd.Series(params_out, name='params')
    #     bathy_quality_ratio = -1
    #     return params_out, quality_flag, bathy_quality_ratio, surface_gm

    # similar quality check might be that the value of the mixture model where the gaussians intersect is at least 50% the peak etc

    # check surface peak is at least some multiple of the background noise
    # currently above surface noise picks up vegetation over land, come back to this later

    # subsurface estimation check
    if surface_peak_right_edge_i >= len(hist):
        # surface peak abuts bottom of range, no subsurface bins
        params_out["noise_below"] = zero_val

    else:
        # continue on to subsurface estimation
        pass
        # will consider subsurface noise later

    params_out["surf_scaling"] = _surf_gm_scaling

    params_out["surf_weight_1"] = _surf_weight_1
    params_out["surf_weight_2"] = _surf_weight_2

    params_out["surf_loc_1"] = _surf_loc_1
    params_out["surf_std_1"] = _surf_sigma_1

    params_out["surf_loc_2"] = _surf_loc_2
    params_out["surf_std_2"] = _surf_sigma_2

    # ########################## ESTIMATE BATHYMETRY PEAK PARAMETERS ############################

    # Remove any histogram peaks above the surface from consideration
    # note this 'i' is the index of the peak, with higher index being deeper
    subsurface_peak_flag = peak_info.i > surf_pk.i
    peak_info['subsurface_peak'] = subsurface_peak_flag

    bathy_peaks = peak_info[subsurface_peak_flag]

    # symmetry of the peak matters, if the left side is siginicantly wider than the right, it's likely that 
    # this peak is in the very shallow water column - and shallow returns are generally well defined / symmetric
    # this is one of those hard to describe checks that seems generally true based on looking at lots of data
    # meant to address cases where shallow turbity is still the most prominent peak but is 
    # convolved with lots of shallow water particulate, meanwhile there's still a clear peak some X m deeper
    # note we only apply this check on the left side of the peak (higher elevation)
    # bathymetry attenuates with depth and so the right side may be wider under normal conditions
    bathy_peaks = bathy_peaks[bathy_peaks.sigma_est_left < 3 * bathy_peaks.sigma_est_right]

    # sort by peak area, computed using prominence
    bathy_peaks.sort_values(by=['area', 'z_inv'], ascending=[False, False], inplace=True)#.reset_index(drop=True)


    # check for peaks at depth with similar areas
    if bathy_peaks.shape[0] > 2:
        # this is a bathy specific check on likely bathy peaks
        area1 = bathy_peaks.iloc[0].area
        area2 = bathy_peaks.iloc[1].area

        z_inv1 = bathy_peaks.iloc[0].z_inv
        z_inv2 = bathy_peaks.iloc[1].z_inv

        # if area 2 is witthin 20% of area 1 and more than 10m deeper, switch rows 1 and 2
        # i.e. there's a similar peak at a deeper depth we should prioritize
        # hopefully helps improve retention of deep bathymetry
        if (area2 / area1) > 0.8 and (z_inv2 - z_inv1) > 10:
            bathy_peaks.iloc[[0, 1]] = bathy_peaks.iloc[[0, 1]]  

    # If no peaks below the primary surface peak
    if bathy_peaks.shape[0] == 0:
        # leave bathy peak fitting params set to 0

        # set bathy peak values to 0
        params_out["bathy_loc"] = zero_val
        params_out["bathy_std"] = zero_val
        params_out["bathy_mag"] = zero_val

        # may still be subsurface noise
        # commenting out for now, will assume subsurface noise picked up by decay

        # params_out['noise_below'] = np.median(
        #     hist[int(surface_peak_right_edge_i):]) + zero_val

        # note the parameter is still fittable
        params_out["noise_below"] = zero_val

        # may still have turbidity without subsurface peaks
        # e.g. in the case of a perfectly smooth turbid descent, or with coarsely binned data
        # continuing to turbidity estimation...

    # If some peak below the primary peak, quick checks for indexing, add as needed
    else:  # df.shape always non negative
        # this section can be improved

        # Very shallow peak, with limited data below
        # Surface range can extend beyond the end of the array/bathy
        if surface_peak_right_edge_i >= bathy_peaks.iloc[0].i:
            surface_peak_right_edge_i = int(bathy_peaks.iloc[0].i) - 1

        # Bathymetry peak estimation

        # Use largest (or only) subsurface peak for bathy estimate
        bathy_pk = bathy_peaks.iloc[0]

        params_out["bathy_loc"] = bathy_pk.z_inv
        params_out["bathy_std"] = bathy_pk.sigma_est
        params_out["bathy_mag"] = bathy_pk.heights

        # Estimate subsurface noise using bins excluding the bathy peak
        bathy_peak_left_edge_i = np.int64(
            np.floor(bathy_pk.i - 3 * bathy_pk.sigma_est_left_i)
        )

        bathy_peak_right_edge_i = np.int64(
            np.ceil(bathy_pk.i + 3 * bathy_pk.sigma_est_right_i)
        )

        if bathy_peak_right_edge_i >= len(hist):
            pass
            # don't currently need to do this because array index slicing returns empty array
            # doing this when the bathy peak is at the edge includes the peak value
            # bathy_peak_right_edge_i = len(hist) - 1

        # subsurface_wout_peak = np.concatenate(
        #     (
        #         hist[surface_peak_right_edge_i:bathy_peak_left_edge_i],
        #         hist[bathy_peak_right_edge_i:],
        #     )
        # )

    # ###################### ESTIMATE TURBIDITY EXPONENTIAL PARAMETERS ############################

    # only use photons from the first 10m to fit the attenuation model

    max_turb_depth = 10  # m below the surface

    if bathy_peaks.shape[0] > 0:
        # if there is a bathymetry peak, remove the values of this peak from consideration for fitting turbidity

        # # adding +-1 to extend window just to be safe
        # note that the standard deviation is in terms of bins (integer index)
        # bathy_range_i is the indices of histogram associated with the bathy peak
        # min() is to make sure we dont go beyond the length of the histogram data
        bathy_range_i = np.arange(
            np.int64(bathy_pk.i - np.floor(2 * bathy_pk.sigma_est_left_i) - 1),
            min(
                np.int64(bathy_pk.i + np.ceil(2 *
                         bathy_pk.sigma_est_right_i) + 1),
                len(hist),
            ),
        )

        max_turb_depth_i = surf_pk.i + (max_turb_depth / z_bin_size) + 1
        max_turb_depth_i = np.int64(max_turb_depth_i)

        if max_turb_depth_i > len(hist):
            max_turb_depth_i = len(hist)

        # get indexer for turbidity data below the surface
        shallow_subsurface = np.full((len(hist),), False)

        # add in all subsurface within 10m
        shallow_subsurface[transition_i:max_turb_depth_i] = True

        # remove bathy peak from subsurface histogram
        shallow_subsurface[bathy_range_i] = False

    else:
        max_turb_depth_i = surf_pk.i + (max_turb_depth / z_bin_size) + 1
        max_turb_depth_i = np.int64(max_turb_depth_i)

        if max_turb_depth_i > len(hist):
            max_turb_depth_i = len(hist)

        shallow_subsurface = np.full((len(hist),), False)
        shallow_subsurface[transition_i:max_turb_depth_i] = True

    # require hist values to be at least 2x the above surface background noise rate for inclusion
    # not technically required, but should help with fitting in daytime cases
    # ideally want to just remove the 'base' of noise from the histogram
    # but want a turbidity model that doesn't require subsurface noise to be too accurate,
    # and also dont want a bunch of subsurface bins that just represent noise

    # Commented out for debugging a separate issue
    # shallow_subsurface[shallow_subsurface & (hist < 2 * params_out['background'])] = False

    # get histogram values to use for turbidity estimation
    hist_t = hist[shallow_subsurface]
    z_inv_t = z_inv[shallow_subsurface]

    # hist/z_inv _t represent turbidity specific depth and histogram data
    # starts at the transition point and MAY have depth bins missing (removed bathy peak)

    # picking back up from transition point detection used in surface peak fitting
    # set up parameters and quality checks up to this point
    not_enough_subsurface = False

    # continue with some quality checks
    # non_zero_column_bins = np.argwhere(hist[transition_i:] > 0).flatten()
    non_zero_column_bins = np.argwhere(hist_t > 0).flatten()

    if len(non_zero_column_bins) < 3:
        # not enough data to actually evaluate 'turbidity'
        not_enough_subsurface = True

    if not_enough_subsurface:
        # null case - smooth transition from peak to nothing
        # or just too few bins to reasonably calculate turbidity
        # or first rise in photons is too far below the surface to be turb.
        params_out["decay_param"] = -zero_val  # send to zero asap
        params_out["decay_top_z"] = -surf_pk.z_inv
        params_out["turb_intens"] = 1e-4

    else:
        # continue estimating turbidity decay parameter
        deepest_nonzero_bin_edge = np.argwhere(hist_t > 0).flatten()[-1] + 1

        # extra +1 due to range behavior, not actual data
        # wc = water column
        # wc_i = np.arange(transition_i, deepest_nonzero_bin_edge + 1)
        wc_i = np.arange(0, deepest_nonzero_bin_edge)

        # getting the decay parameter *in terms of DEPTH* to preserve any physical mapping
        # technically more of an inverted height than a depth
        wc_hist_ = hist_t[wc_i]
        wc_depth_ = z_inv_t[wc_i] - surf_pk.z_inv  # true depth below surface

        # ignoring zero bins so log doesn't explode
        wc_hist = wc_hist_[wc_hist_ > 0]
        wc_depth = wc_depth_[wc_hist_ > 0]

        # remember these data might be missing a chunk where a bathy peak was when indexing

        # decay modeled in terms of depth

        # weighting isn't just to give more weight to shallow bins
        # also helps avoid overfitting to subsurface noise
        # we know 99% of the time we care most about the first 10m, no matter the noise
        # inverse distance of the first 10 meters
        weights = np.sqrt(wc_hist) * (1 - wc_depth / 15)

        weights[weights < 0] = 0

        # feb '23: enough evidence to show weights are helping *a lot*, don't discard without good reason

        m, b = np.polyfit(wc_depth, np.log(wc_hist), deg=1, w=weights)

        turb_intens = np.exp(b)
        decay_param = m

        # # where does our turbidity model intersect with our surface model? defines the top of column - useful stat
        # # set up equation with exponential and gaussian mixture, solve for roots
        # # won't work if the input data has been smoothed, hence commented out
        # A = 4 * _surf_sigma_1 **2 * _surf_sigma_2**2 * -decay_param
        # C1 = _surf_loc_1 + surf_pk.z_inv
        # C2 = _surf_loc_2 + surf_pk.z_inv
        # aa = -2
        # bb = A + 2 * C1 + 2 * C2
        # cc = C1**2 + C2**2 - A * surf_pk.z_inv - np.log(2 * np.pi * turb_intens * _surf_sigma_1 * _surf_sigma_2)
        # surf_turb_roots = np.roots([aa, bb, cc])
        # transition_z_inv = np.max(surf_turb_roots)
        # bad_transition = (transition_z_inv < surf_pk.z_inv) | (transition_z_inv > (surf_pk.z_inv + 20))

        bad_decay_component = (
            (decay_param > -zero_val)
            or (decay_param < -1000)
            or (turb_intens > 5 * surf_pk.heights)
            or (turb_intens < 0)
        )

        if bad_decay_component:
            # set turbidity as 0
            params_out["decay_param"] = -zero_val
            params_out["decay_top_z"] = -surf_pk.z_inv
            params_out["turb_intens"] = 1e-3

        else:
            # numerical intersection of turbidity and surface models

            # recompute the column z values at higher resoluion
            z_column_interp = np.arange(
                surf_pk.z_inv, z_inv[(np.argwhere(
                    hist > 0).flatten()[-1])], 1e-3
            )

            _surface_model = gauss_mixture(
                z_column_interp,
                params_out["surf_loc_1"],
                params_out["surf_std_1"],
                params_out["surf_loc_2"],
                params_out["surf_std_2"],
                params_out["surf_weight_1"],
                params_out["surf_scaling"],
            )

            # # smooth the mixture model with the same gaussian filter as the original histogram of photon data
            _smoothed_surface_model = gaussian_filter1d(
                _surface_model, 0.1 / z_bin_size
            )

            # compute turbidity model
            _turbid_model = turb_intens * np.exp(decay_param * z_column_interp)

            # find the intersection of the two models
            model_diff = _smoothed_surface_model - _turbid_model

            # first sign change is the intersection we want
            sign_ = np.sign(model_diff)
            # ie where sign changes (from negative to positive only)
            sign_changes_i = np.where((sign_[:-1] != sign_[1:]))[0] + 1

            if len(sign_changes_i) > 0:
                in_first_10m = (
                    z_column_interp[sign_changes_i[0]] - surf_pk.z_inv) < 10
            else:
                in_first_10m = False

            # check that there is a sign change in the first 10m
            if in_first_10m:
                params_out["column_top"] = -z_column_interp[sign_changes_i[0]]

                params_out["decay_param"] = decay_param
                # used to compute attnetuation depth independent of surface mixture model
                params_out["decay_top_z"] = -surf_pk.z_inv
                params_out["turb_intens"] = turb_intens

            else:
                params_out["decay_param"] = -zero_val
                params_out["decay_top_z"] = -surf_pk.z_inv
                params_out["turb_intens"] = 1e-3

    # get the index of the refined surface to subsurface transition

    # negative because column top is in terms of z, z_inv = -z
    column_top_z_inv_i = np.argmin(np.abs(z_inv - (-params_out["column_top"])))

    ##############################    REFINING BATHY PEAK     ################################
    # now that we have a more solid estimate of the base of the bathy peak (turbidity/decay)
    # lets improve our estimate of the bathy model, if there is one

    # if there's a bathy peak
    if bathy_peaks.shape[0] > 0:
        # remove any contamination from the surface peak using the refined intersection
        z_inv_to_fit = z_inv[bathy_range_i[bathy_range_i > column_top_z_inv_i]]
        hist_to_fit = hist[bathy_range_i][bathy_range_i > column_top_z_inv_i]
        p_init_b = [bathy_pk.mag_scaling, bathy_pk.z_inv, bathy_pk.sigma_est]
        [_bathy_mag_scale, _bathy_loc, _bathy_sigma] = p_init_b

        try:
            with warnings.catch_warnings():
                warnings.simplefilter("ignore", OptimizeWarning)

                [_bathy_mag_scale, _bathy_loc, _bathy_sigma], _ = curve_fit(
                    f=gauss,
                    xdata=z_inv_to_fit,
                    ydata=hist_to_fit,
                    p0=p_init_b,
                    bounds=(
                        [zero_val, -params_out["column_top"], zero_val],
                        [3 * _bathy_mag_scale, z_inv.max(), 3],
                    ),
                )

        except Exception as e:
            # Log the exception if needed
            # print(f"Curve fitting failed due to: {e}")
            # curve fit can fail under a lot of common cases like 1 bin peaks etc
            # just catch them all instead of accounting for each for now
            [_bathy_mag_scale, _bathy_loc, _bathy_sigma] = p_init_b

            params_out["bathy_loc"] = _bathy_loc
            params_out["bathy_std"] = _bathy_sigma
            params_out["bathy_mag"] = _bathy_mag_scale

    ############################ CONFIDENCE OF BATHY PEAK ############################
    # One way we can measure the confidence of our bathy peak is how it compares
    # to the variations about the rest of the subsurface model
    # eg check if the peak is on the order of the rest of the subsurface variations

    # careful, using all subsurface data might skew things when we really just care
    # about the region with valid data

    # actually trying something a little simpler
    # quality_flag = 2 : only 1 isolated subsurface peak
    # quality_flag = 3 : multiple subsurface peaks -> check bathy_conf

    # bathy_conf = ratio of bathy peak prominence to next most prominent peak
    # still undefined if only 1 bathy peak - do something else for these

    # if there's a bathy peak
    if bathy_peaks.shape[0] > 1:
        # trying a more naive approach
        # consider any peaks further than 1m in either direction
        # to help prevent cases of spread bathy signal falling in separate bins

        non_bathy_pks = bathy_peaks.iloc[1:]
        comparison_peaks = non_bathy_pks.loc[
            np.abs(non_bathy_pks.z_inv - bathy_pk.z_inv) > 1, :
        ]

        if comparison_peaks.shape[0] == 0:
            # no subsurface peaks 1m+ away from probably bathy peak
            quality_flag = 2  # good surface, effectively 1 bathy
            bathy_quality_ratio = -1

        else:
            quality_flag = 3  # good surface, multiple bathy, check quality ratio
            bathy_quality_ratio = (
                bathy_pk.prominences / comparison_peaks.prominences.iloc[0]
            )

    elif bathy_peaks.shape[0] == 1:
        quality_flag = 2  # good surface, 1 bathy
        bathy_quality_ratio = -1  # do something here

    else:
        # recall if the surface peak is badly defined we wouldn't get to this point
        quality_flag = 1  # good surface, no bathy
        bathy_quality_ratio = -1

    params_out = pd.Series(params_out, name="params", dtype=np.float64)

    return params_out, quality_flag, bathy_quality_ratio, surface_gm, peak_info


# QUALITY CHECKS TO REMOVE NOISE MISCLASSIFIED AS BATHYMETRY


def bathy_quality_check(model_interp, params, peaks):
    # break this up into smaller functions?
    # consider moving into estimate_model_parameters()

    # remove the turbidity / baackground noise from the bathymetry and bound at 0 below
    column_background = np.max(
        [model_interp.column, model_interp.background], axis=0)

    bathy_remainder = np.clip(
        model_interp.bathymetry - column_background, 0, None)

    # compute the ratio of outright bathymetry component of the model to that which is explained by turbidity
    if np.sum(model_interp.bathymetry) > 0:
        bathy_area_ratio = np.sum(bathy_remainder) / \
            np.sum(model_interp.bathymetry)
    else:
        # avoid divide by 0
        bathy_area_ratio = 0

    any_good_data = bathy_area_ratio > 0

    if any_good_data:

        # not just find_peaks, this is the fitted gaussian peak
        modeled_bathy_peak_height = model_interp.bathymetry.max()

        # get the heights corresponding to where the bathy gaussian intersects the turbidity, or iss eff. zero, compute prominence
        prominent_bathy = np.where(bathy_remainder != 0)[0]

        idx_top = prominent_bathy[0]
        idx_bot = prominent_bathy[-1]

        # reequire at least a 10% prominence above its highest edge

        # note that instead of using the actual model output, we only use the subsurface (column + backgr)
        # this is because in really shallow cases the bathy prominence gets reduced by its proximity to the strong surface return

        model_subsurf_top_of_bathy = model_interp.loc[
            idx_top, ["column", "background"]
        ].max()
        model_subsurf_bot_of_bathy = model_interp.loc[
            idx_bot, ["column", "background"]
        ].max()

        prom_perc = (
            np.max(model_interp.bathymetry)
            - np.max([model_subsurf_top_of_bathy, model_subsurf_bot_of_bathy])
        ) / np.max([model_subsurf_top_of_bathy, model_subsurf_bot_of_bathy])

        # compute subsurface error
        subsurface_interp_idx = (-model_interp.z_inv) < params.column_top

        delta_with_bathy = (
            model_interp.output[subsurface_interp_idx]
            - model_interp.z_input[subsurface_interp_idx]
        )
        rmse_with_bathy = np.sqrt(np.mean(delta_with_bathy**2))

        delta_wout_bathy = (
            column_background[subsurface_interp_idx]
            - model_interp.z_input[subsurface_interp_idx]
        )
        rmse_wout_bathy = np.sqrt(np.mean(delta_wout_bathy**2))

        bathy_error_diff = (
            rmse_wout_bathy - rmse_with_bathy) / rmse_with_bathy

        # e.g. bathy_error_difference of 0.1 indicates that the bathymetry model is
        # a) improving the overall model error
        # b) improving it by at least 10%

        # compare the bathy peak prom to the rest of the unselected peaks
        # targeting nighttime cases were the background ~ 0 and there's several small equal sized noise peaks

        bathy_prom = params.bathy_mag - np.max(
            [model_subsurf_top_of_bathy, model_subsurf_bot_of_bathy]
        )

        # exclude bathy candidate at top of the dataframe
        subsurface_peaks = peaks[peaks.subsurface_peak].iloc[1:]
        bathy_pk = peaks[peaks.subsurface_peak].iloc[0]

        # determine nearby peaks with z_inv within xm
        neighbor_peaks_idx = (np.abs(subsurface_peaks.z_inv - params.bathy_loc) < 10)

        # note: neighbor peaks are helpful for small vertical resolutions (<1m)
        # but larger resolutions will have further spaced peaks
        # eg. 5m vertical bin size doesn't have any neighbor peaks, and this check is not useful

        # if neighbor_peaks_idx is empty, use all subsurface peaks
        if np.sum(neighbor_peaks_idx) == 0:
            neighbor_peaks_idx = np.ones(subsurface_peaks.shape[0], dtype=bool)

        # if neighbor peaks still empty, then there are no other subsurface peaks and this should work as intended
        # in this case the bathy peak candidate is distinct

        neighbor_peaks = subsurface_peaks[neighbor_peaks_idx]

        # jan 2024, changed from 1.5x the median to 3 sigma above the mean
        minimum_prom = neighbor_peaks.prominences.mean() + \
            3 * neighbor_peaks.prominences.std()

        if np.isnan(minimum_prom):  # no other subsurface peaks
            minimum_prom = 0

        column_model_at_bathy_peak = np.interp(
            params.bathy_loc, model_interp.z_inv, model_interp.column
        )

        background_model_at_bathy_peak = params.background

        minimum_height = (
            np.max([column_model_at_bathy_peak, background_model_at_bathy_peak])
            + minimum_prom
        )

        # evaluate nearby peaks to compare how many photons might get classified
        # to avoid random noise clusters, bathy should be noticeably more photons than other peaks
        # high noise cases will also feature many many 1 photon peaks which can drag down the mean to less than meaningful
        # get up to 5 of the largest n_photons values
        largest_neighbor_peaks = neighbor_peaks.sort_values( 
            by="n_photons", ascending=False).iloc[:5]


        minimum_n_photons = largest_neighbor_peaks.n_photons.mean() + 3 * largest_neighbor_peaks.n_photons.std()

        bathy_peak_n_photons = bathy_pk.n_photons
        
        # # deep bathy hotfix
        # # commenting out feb 1 2024 as Im not sure it's working as intended
        # # if the bathy peak is more than 10m below the next largest area peak, loosen the height check
        # # if there's more than 20 peaks we're proabably looking at a daytime case with lots of noise
        
        # if (subsurface_peaks.shape[0] > 2) and (subsurface_peaks.shape[0] < 20):
        #     bathy_pk = subsurface_peaks.iloc[0]
        #     next_pk = subsurface_peaks.iloc[1]

        #     if bathy_pk.z_inv - next_pk.z_inv > 20:
        #         minimum_height = 0

        null_check = False

    else:
        # no bathymetry peak above the turbidity
        prom_perc = -1
        bathy_prom = -1
        minimum_prom = 1e31
        minimum_height = 1e31
        bathy_error_diff = -1
        modeled_bathy_peak_height = -1
        null_check = True
        minimum_n_photons = -1
        bathy_peak_n_photons = 0 # null value greater than mnimum n_photons

        pass

    # in various localized metrics, bathy should be at least 10% of the measurable subsurface return
    # ok if the bathymetry model rises at least 10% above the combines column/background subsurface model
    min_prom_perc = 0.1
    ok_prom = prom_perc > min_prom_perc

    if ok_prom:
        test_prom_str = "PASSED"
    else:
        test_prom_str = "FAILED"

    # ok if bathy is significantly  more prominent than the next 10 (or fewer) unselected subsurface peaks
    ok_relheight = modeled_bathy_peak_height > minimum_height

    if ok_relheight:
        test_relheight_str = "PASSED"
    else:
        test_relheight_str = "FAILED"

    # ok if the remainder of the bathymetry component makes up at least 10% of the area of the total bathymetry model
    min_bathy_area_ratio = 0.1
    ok_area = bathy_area_ratio > min_bathy_area_ratio

    if ok_area:
        test_area_str = "PASSED"
    else:
        test_area_str = "FAILED"

    # i think this error computation is biasing results, overfiltering deep bathy
    # ok if bathy_error_diff > 0.1
    min_bathy_error_diff = 0.1
    ok_error = bathy_error_diff > min_bathy_error_diff

    if ok_error:
        test_error_str = "PASSED"
    else:
        test_error_str = "FAILED"

    # at small scales we should have more than 3 photons to make any real classification
    # added jan 2024
    # only valid for z resolution of 0.1
    bathy_mag_minimum = 0.3 # set from inspecting the photon data, note histogram is smoothed so not integer valued
    # ok_mag = params.bathy_mag > bathy_mag_minimum
    ok_mag = modeled_bathy_peak_height > bathy_mag_minimum

    if ok_mag:
        test_mag_str = "PASSED"
    else:
        test_mag_str = "FAILED"

    # daytime cases can have a lot of noise, with random clusters of photons appearing as peaks
    # these are challenging to filter outright, as they are real, noticeable peaks in the histogram, above the background rate
    # one solution may be to ensure bathymetry peaks classify more photons than noise peaks
    # this also implicitly accounts for changes in the background rate due to solar elevation / surface brightness
    ok_n_photons = bathy_peak_n_photons > minimum_n_photons

    if ok_n_photons:
        test_n_photons_str = "PASSED"
    else:
        test_n_photons_str = "FAILED"

    # if ok_prom and ok_error and ok_area and ok_relheight and ok_mag:
    if ok_prom and ok_area and ok_relheight and ok_mag and ok_n_photons:
        bathy_flag = True
    else:
        bathy_flag = False

    # combine the tests into a nice dict

    test_results = {
        "has_bathy": bathy_flag,
        "area_perc_check": test_area_str,
        "prominence_perc_check": test_prom_str,
        "error_check": test_error_str,
        "peak_comparison_check": test_relheight_str,
        "peak_height_check": test_mag_str,
        "n_photons_check": test_n_photons_str,
        "test_bathy_mag": params.bathy_mag,
        "test_bathy_std": params.bathy_std,
        "test_bathy_loc": params.bathy_loc,
        "prom_perc": prom_perc,
        "test_bathy_prom": bathy_prom,
        "bathy_area_ratio": bathy_area_ratio,
        "bathy_error_diff": bathy_error_diff,
        "min_prom": minimum_prom,
        "min_height": minimum_height,
        "min_bathy_error_diff": min_bathy_error_diff,
        "min_bathy_area_ratio": min_bathy_area_ratio,
        "min_prom_perc": min_prom_perc,
        "modeled_bathy_peak_height": modeled_bathy_peak_height,
        "null_check": null_check,
        "min_n_photons": minimum_n_photons,
        "bathy_peak_n_photons": bathy_peak_n_photons,
        # "bathy_peak_area": bathy_peak_area,
        # "area_val_check": test_area_val_str,
    }

    return bathy_flag, test_results


# WAVEFORM CLASS TO STORE DATA/METHODS FOR VERTICAL HISTOGRAM ANALYSIS


class Waveform:
    """Class for managing pseudowaveform data.

    Attributes:
        data (array): Histogram data input.
        z_inv_bin_edges (array): Histogram bin edges input.
        peaks (DataFrame): Info related to peaks of input histogram.
        quality_flag (int): Quality flag for the waveform.
                            -11: not enough surface photons classified - all classes ignored
                            -2: no peaks
                            -1: not enough data
                            0: not set
                            1: surface only
                            2: strong surface and weak bathy peaks
                            3: strong surface and bathy peaks
                            4: bathy surface has been removed for lack of photons (general)
                            5: bathy surface has been removed for lack of photons (ultra shallow)

        model (DataFrame): DataFrame for histogram model components and output.
        model_interp (DataFrame): Interpolated model data.
        fitted (bool): [DEPRECATED] Indicates if the waveform has been fitted.
        profile: Additional profile information.

    Methods:
        fit(): Non-linear least squares optimization to fit the model to original data.
        show(): Plot histogram and model data.
    """

    def __init__(
        self,
        histogram,
        z_inv_bin_edges,
        fit=False,
        sat_check=False,
        verbose=False,
        profile=None,
        beam_strength=None,
    ):
        """Initialize the Waveform object.

        Args:
            histogram (array): Histogram data input.
            z_inv_bin_edges (array): Histogram bin edges input.
            fit (bool, optional): [DEPRECATED] Whether to perform fitting immediately. Defaults to False.
            sat_check (bool, optional): Satellite check flag. Defaults to False.
            verbose (bool, optional): Verbose flag for additional logging. Defaults to False.
            profile: Additional profile information. Defaults to None.
        """
        zero_val = 1e-31
        # ensure that histogram and bin edges make sense
        assert len(histogram) == (len(z_inv_bin_edges) - 1)

        # require that z_inv bins be increasing
        if np.any(np.diff(z_inv_bin_edges) < 0):
            raise Exception("Elevation bins values must be increasing.")

        self.data = histogram

        self.beam_strength = beam_strength

        self.profile = copy.deepcopy(profile)

        # edges used to calculate the histogram
        self.bin_edges = z_inv_bin_edges

        # center of histogram bins
        self.z_inv = self.bin_edges[:-1] + np.diff(self.bin_edges) / 2

        # has this been waveform model been fitted?
        self.fitted = False

        # get peak data about this waveform
        peak_info = get_peak_info(self.data, self.z_inv, verbose=verbose)

        # get hist model parameters, fitting bounds, and qualitative quality flag
        params_est, self.quality_flag, _, self.surface_gm, self.peaks = estimate_model_params(
            self.data,
            self.z_inv,
            peak_info=peak_info,
            verbose=verbose,
            profile=profile,
        )

        # interpolate to higher resolution for plotting and things
        # gives approx 0.01m vertical resolution, but not exact (depending on orig data)

        z_inv_interp = np.linspace(
            self.z_inv.min(),
            self.z_inv.max(),
            np.int64(np.abs(self.z_inv.max() - self.z_inv.min()) / 0.01),
        )

        # this higher vertical resolution model is key for numerical estimation of classifications
        # not just for plotting - be careful making too many changes!
        z_inv_interp_diff = np.abs(np.diff(z_inv_interp)[0])

        # requires to compute with z_inv_interp_diff at least once to get full res interpolated model
        model_interp = self._get_model_output(
            -z_inv_interp, params_est, z_inv_interp_diff
        )

        self.model_interp = model_interp

        update_model_interp = False

        if self.quality_flag >= 0:
            # overall waveform quality - no peak, no data, etc

            bathy_quality_flag, self.bathy_tests = bathy_quality_check(
                model_interp, params_est, self.peaks
            )

            if bathy_quality_flag == False:
                # update model parameters to remove bathymetry 
                new_params = params_est
                new_params["bathy_loc"] = zero_val
                new_params["bathy_std"] = zero_val
                new_params["bathy_mag"] = zero_val

                self.params = new_params

                update_model_interp = True

            else:
                self.params = params_est
                self.model_interp = model_interp

        else:
            bathy_quality_flag = False
            self.params = params_est
            self.model_interp = model_interp
            self.classification_data = None

        # get model output for each photon height - this is how photons are classified
        z_ph = self.profile.data.z_ph

        model_ph = self._get_model_output(
            z_ph,
            self.params,
            model_interp_df=self.model_interp,
            bathy_flag=bathy_quality_flag,
        )

        self.profile.data.classification = model_ph.classification.values.astype(int)
        feature_weights = compute_feature_weights(z_ph, self.params, self.model_interp, bathy_quality_flag)
        self.profile.data.weight_surface = feature_weights.surface.values.astype(np.float64)
        self.profile.data.weight_bathymetry = feature_weights.bathymetry.values.astype(np.float64)

        # recall col top is in (-z) units
        self.profile.data.subsurface_flag = z_ph < self.params.column_top

        self.sat_check = sat_check

        # final check on classifications
        # require at least 3 photons to make a classification 
        n_surface = np.sum(self.profile.data.classification == self.profile.label_help("surface"))
        n_column = np.sum(self.profile.data.classification == self.profile.label_help("column"))
        n_bathy = np.sum(self.profile.data.classification == self.profile.label_help("bathymetry"))
        n_background = np.sum(self.profile.data.classification == self.profile.label_help("background"))

        depth = self.params.bathy_loc - (-self.params.column_top) # column top isn't inverted? why did I do this...

        if n_surface <= 3:
            self.quality_flag = -11

            # set all photons to background
            self.profile.data.classification = np.ones_like(self.profile.data.classification) * self.profile.label_help("background")

            # remove any feature weights (positions within the gaussian) for surface/seafloor guesses
            self.profile.data.weight_surface = np.zeros_like(self.profile.data.weight_surface)
            self.profile.data.weight_bathymetry = np.zeros_like(self.profile.data.weight_bathymetry)

            # set subsurface flag to false
            self.profile.data.subsurface_flag = np.zeros_like(self.profile.data.subsurface_flag)

            # update estimated params to match empty set
            new_params = params_est
            new_params["surf_scaling"] = zero_val
            new_params["surf_loc_1"] = 2 * zero_val
            new_params["surf_std_1"] = zero_val
            new_params["surf_weight_1"] = 1
            new_params["surf_loc_2"] = zero_val
            new_params["surf_std_2"] = zero_val
            new_params["surf_weight_2"] = zero_val
            new_params["decay_top_z"] = zero_val
            new_params["decay_param"] = -zero_val
            new_params["turb_intens"] = zero_val
            new_params["column_top"] = zero_val
            new_params["background"] = 2 * zero_val # higher so null results in background classifications
            new_params["bathy_mag"] = zero_val
            new_params["bathy_loc"] = zero_val
            new_params["bathy_std"] = zero_val

            self.params = new_params
            bathy_quality_flag = False

            update_model_interp = True

        elif n_bathy <= 5: # dont need this check in cases where a bad surface removes all other classes
            self.quality_flag = 4
            # keep the surface and water column data, but ignore any bathymetry
            # recall if there's any water column returns identified they stop at the top of the bathy gaussian
            if n_column > 0:
                # if there are any column classified photons, convert bathy to column
                self.profile.data.classification[self.profile.data.classification == self.profile.label_help("bathymetry")] = self.profile.label_help("column")

            else:
                # if no column, convert bathy to background
                self.profile.data.classification[self.profile.data.classification == self.profile.label_help("bathymetry")] = self.profile.label_help("background")

            # remove any feature weights (positions within the gaussian) for seafloor guesses
            self.profile.data.weight_bathymetry = np.zeros_like(self.profile.data.weight_bathymetry)

            # update estimated params to remove bathy
            new_params = params_est
            new_params["bathy_mag"] = zero_val
            new_params["bathy_loc"] = zero_val
            new_params["bathy_std"] = zero_val

            self.params = new_params
            bathy_quality_flag = False

            update_model_interp = True

        elif self.bathy_tests['has_bathy'] and (depth < 1):
            # if the bathy peak is shallow, better have more returns, otherwise likely noise or clipped wave
            # technically this would also change by along-track length
            # this is primarily aiming to clean up the fine length scales for water surface classifications
            # longer along track scales need some other filter as waves appearing like bathymetry is a fundamental algorithmic issue
            
            if self.beam_strength == "strong":
                n_bathy_shallow_req = 20
            else:# weak or unspecified
                n_bathy_shallow_req = 10


            if n_bathy < n_bathy_shallow_req: 
                self.quality_flag = 5
                # keep the surface and water column data, but ignore any bathymetry
                # recall if there's any water column returns identified they stop at the top of the bathy gaussian

                self.profile.data.classification[self.profile.data.classification == self.profile.label_help("bathymetry")] = self.profile.label_help("surface")

                # remove any feature weights (positions within the gaussian) for seafloor guesses
                self.profile.data.weight_bathymetry = np.zeros_like(self.profile.data.weight_bathymetry)

                # update estimated params to remove bathy
                new_params = params_est
                new_params["bathy_mag"] = zero_val
                new_params["bathy_loc"] = zero_val
                new_params["bathy_std"] = zero_val

                self.params = new_params
                bathy_quality_flag = False

                update_model_interp = True
            

        # the interpolated model is used for plotting and assigning classifications
        # if not enough photons are labeled, this changes our estimated model parameters
        # and we need to recompute the interpolated model
        # computing this is relatively expensive! 
            
        if update_model_interp:
            new_model_interp = self._get_model_output(
                -z_inv_interp,
                self.params,
                z_inv_interp_diff,
                model_interp_df=None,
                bathy_flag=bathy_quality_flag,
            )

            self.model_interp = new_model_interp

        # compute model output at original histogram resolution
        self.model = self._get_model_output(
            -self.z_inv,
            self.params,
            model_interp_df=self.model_interp,
            bathy_flag=bathy_quality_flag,
        )

        # compute model output at photon resolution to update classifications
        model_ph = self._get_model_output(  
            z_ph,
            self.params,
            model_interp_df=self.model_interp,
            bathy_flag=bathy_quality_flag,
        )

        self.bathy_quality_flag = bathy_quality_flag
        # recompute classifications
        self.profile.data.classification = model_ph.classification.values.astype(int)
        feature_weights = compute_feature_weights(z_ph, self.params, self.model_interp, bathy_quality_flag)
        self.profile.data.weight_surface = feature_weights.surface.values.astype(np.float64)
        self.profile.data.weight_bathymetry = feature_weights.bathymetry.values.astype(np.float64)

        # done

    def __str__(self):
        """Provide a human-readable summary of the Waveform object.

        Returns:
            str: Human-readable summary.
        """

        s = f"""----- PSEUDOWAVEFORM -----
TOTAL PHOTONS: {np.sum(self.data)}
Elevation Range : [{min(self.bin_edges):.2f}m, {max(self.bin_edges):.2f}m]
Elevation Bin Count : {len(self.bin_edges) - 1}
Peak Count : {self.peaks.shape[0]}
Overall Quality Flag : {self.quality_flag}
        """

        return s

    def _get_model_output(
        self, z_ph, params, z_bin_interp=None, model_interp_df=None, bathy_flag=True
    ):
        """
        Evaluates a waveform and classifies photons based on their elevations.

         Args:
             z_ph (array or pd.Series): Photon elevations.
             params (object): Parameters for generating model components.
             z_bin_interp (array, optional): Bin sizes for interpolation, required for smoothing.
             model_interp_df (pd.DataFrame, optional): Interpolated model DataFrame.
             bathy_flag (bool, optional): Flag to enable bathymetry-based relabeling of photons.

         Returns:
             pd.DataFrame: DataFrame containing the following columns:
                 - z_input: Interpolated value of the actual histogram.
                 - classification: Numeric label for each photon's classification.
                 - output: Maximum likelihood among all model components at the given elevation.
                 - surface, bathymetry, column, background: Individual model components at the given elevation.
                 - surface_1, surface_2: Individual Gaussian components of the surface model if a mixture model is used.
                 - z_inv: Negative of the photon elevations.

         Notes:
             - The function assumes `self.profile.class_labels` exists for numeric to string label conversion.
             - If `model_interp_df` and `z_bin_interp` are not provided, the function looks for `self.model_interp`.

        """
        try:
            input_heights = z_ph.values
            input_idx = z_ph.index
        except:
            input_heights = z_ph
            input_idx = np.arange(len(z_ph))

        if (model_interp_df is None) and (z_bin_interp is None):
            if hasattr(self, "model_interp"):
                model_interp_df = self.model_interp

            else:
                raise ValueError(
                    "Waveform must have model_interp attribute if model_interp_df not passed manually."
                )

        # initialize all photons as unclassified
        labels = self.profile.label_help(
            "unclassified") * np.ones_like(input_heights)

        z_inv = -input_heights

        model_out = pd.DataFrame(
            np.nan,
            index=input_idx,
            columns=[
                "z_input",
                "classification",
                "output",
                "surface",
                "bathymetry",
                "column",
                "background",
                "surface_1",
                "surface_2",
                "z_inv",
            ],
            dtype=np.float64,
        )

        model_out.z_inv = z_inv

        # get approximate value of the actual histogram here
        model_out.z_input = np.interp(-input_heights, self.z_inv, self.data)

        model_out.background = uniform_spdf(
            z_inv, self.z_inv.min(), self.z_inv.max(), params.background
        )

        if params.bathy_std > 1e-8:
            # actual bathy peak
            model_out.bathymetry = gauss(
                z_inv, params.bathy_mag, params.bathy_loc, params.bathy_std
            )
            model_out.bathymetry[model_out.z_inv < -params.column_top] = 0

            # ignore classifications beyond 2 sigma of the bathy peak
            model_out.bathymetry[model_out.z_inv < (
                params.bathy_loc - 2 * params.bathy_std)] = 0
            
            model_out.bathymetry[model_out.z_inv > (
                params.bathy_loc + 2 * params.bathy_std)] = 0

        else:
            # no bathy, just null values
            model_out.bathymetry = np.zeros_like(model_out.bathymetry)

        model_out.column = exp_spdf(
            z_inv, -params.decay_top_z, params.turb_intens, params.decay_param
        )

        if z_bin_interp is not None:  # user supplied bins in order with known sizing
            model_out.surface = gauss_mixture(
                z_inv,
                params.surf_loc_1,
                params.surf_std_1,
                params.surf_loc_2,
                params.surf_std_2,
                params.surf_weight_1,
                params.surf_scaling,
            )
            g_sigma_interp = 0.1 / z_bin_interp
            model_out.surface = gaussian_filter1d(
                model_out.surface, g_sigma_interp)

            # if there's a surface mixture model of the surface, compute components and smooth
            if self.surface_gm is not None:
                responsibilities = self.surface_gm.predict_proba(
                    z_inv.reshape(-1, 1))

                model_out.surface_1 = (
                    responsibilities[:, 0]
                    * params.surf_scaling
                    * params.surf_weight_1
                    * gauss_pdf(z_inv, params.surf_loc_1, params.surf_std_1)
                )

                model_out.surface_2 = (
                    responsibilities[:, 1]
                    * params.surf_scaling
                    * params.surf_weight_2
                    * gauss_pdf(z_inv, params.surf_loc_2, params.surf_std_2)
                )

                model_out.surface_1 = gaussian_filter1d(
                    model_out.surface_1, g_sigma_interp
                )
                model_out.surface_2 = gaussian_filter1d(
                    model_out.surface_2, g_sigma_interp
                )

            else:
                # if no mixture, just use the single gaussian model
                # recall that the weighting gets zeroed for no mixture, so can just use the same mixture function
                model_out.surface_1 = model_out.surface
                model_out.surface_2 = np.zeros_like(model_out.surface)


                # remove classifications more than 3 sigma above the top surface peak
                model_out.loc[:, "surface"][ 
                    model_out.z_inv < (params["surf_loc_1"] - 3 * params["surf_std_1"])] = 0
                
                # surface classifications below are not really a problem because of the water column class


        else:
            # estimate from model_interp
            model_out.surface = np.interp(
                -z_ph, model_interp_df.z_inv, model_interp_df.surface
            )
            model_out.surface_1 = np.interp(
                -z_ph, model_interp_df.z_inv, model_interp_df.surface_1
            )
            model_out.surface_2 = np.interp(
                -z_ph, model_interp_df.z_inv, model_interp_df.surface_2
            )
            model_out.column = np.interp(
                -z_ph, model_interp_df.z_inv, model_interp_df.column
            )
            model_out.bathymetry = np.interp(
                -z_ph, model_interp_df.z_inv, model_interp_df.bathymetry
            )
            model_out.background = np.interp(
                -z_ph, model_interp_df.z_inv, model_interp_df.background
            )

        # # for each value get all the model components and max likelihood them
        model_at_z_ph = model_out.loc[
            :, ["surface", "bathymetry", "column", "background"]
        ]

        # compute maximum likelihood
        model_out.output = model_at_z_ph.max(axis=1)

        # # switch column names to numeric for labeling ease
        model_at_z_ph.columns = [
            self.profile.label_help(x) for x in model_at_z_ph.columns
        ]

        # # get the column label with the max value
        labels = model_at_z_ph.idxmax(axis=1).values

        # switch sub seafloor column classifications to noise
        if bathy_flag:
            ph_below_seafloor = model_out.z_inv > params.bathy_loc
            ph_column = labels == self.profile.label_help("column")

            labels[ph_below_seafloor & ph_column] = self.profile.label_help(
                "background"
            )

        model_out.classification = labels

        return model_out


if __name__ == "__main__":
    pass
