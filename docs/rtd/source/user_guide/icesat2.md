# ICESat-2 Module

The ICESat-2 module within SlideRule supports a number of both legacy ***p-series*** and ***s-series*** endpoints, as well as the newer DataFrame-based ***x-series*** endpoints.  This document focuses on the ***x-series*** endpoints while still referencing the other legacy endpoints when helpful.

Three main kinds of data are returned by the ICESat-2 endpoints: segmented photon data, elevation data (from the ATL06-SR algorithm), and vegetation data (from the PhoREAL algorithm). All data returned by the ICESat-2 endpoints are organized around the concept of an `extent`.  An extent is a variable length, customized ATL03 segment.  It takes the ATL03 photons and divides them up based on their along-track distance, filters them, and then packages them together a single new custom segment.  Given that the ICESat-2 standard data products have a well defined meaning for segment, SlideRule uses the term extent to indicate this custom-length and custom-filtered segment of photons.

The following processing flags are used for all ICESat-2 endpoints:
- `0x0001`: Along track spread too short
- `0x0002`: Too few photons
- `0x0004`: Maximum iterations reached
- `0x0008`: Out of bounds
- `0x0010`: Underflow
- `0x0020`: Overflow

In addition, most endpoints support the generation of a name filter using the `granule` parameter:
- `rgt`: Reference ground track
- `cycle`: Orbit cycle
- `region`: ATL03 region {1 to 14}
- `version`: ATL03 release version (e.g. 007)

## 1. ATL03 - `atl03x`

The SlideRule `atl03x` endpoint provides a service for ATL03 custom processing.  This endpoint queries ATL03 input granules for photon heights and locations based on a set of photon-input parameters that select geographic and temporal ranges.  It then selects a subset of these photons based on a set of photon classification parameters, and divides these selected photons into short along-track extents, each of which is suitable for generating a single height estimate.  These extents may be returned to the client, or may be passed to downstream algorithms like the ATL06-SR height-estimation module, or the PhoREAL algorithm.

Using the Python client, this service is called via:
```Python
sliderule.run('atl03x', parms)
```

The default resulting DataFrame from this endpoint contains the following columns:
|Field|Description|Units|Notes|
|-----|-----------|-----|-----|
|time_ns|Unix Time|nanoseconds|index column of DataFrame|
|latitude|EPSG:7912|degrees (double)|replaced by geometry column when GeoDataFrame|
|longitude|EPSG:7912|degrees (double)|replaced by geometry column when GeoDataFrame|
|x_atc|Along track distance|meters (double)|dist_ph_along + segment_distance|
|y_atc|Across track distance|meters (float)|dist_ph_across|
|height|Photon elevation above reference|meters (float)|vertical datum controlled by parameters, default is ITRF2014|
|solar_elevation|Sun elevation as provided in ATL03|degrees (float)||
|background_rate|Solar background rate|PE per second (float)||
|spacecraft_velocity|Along track velocity of footprints on surface of the earth|meters per second||
|atl03_cnf|ATL03 confidence|-2:possible_tep, -1:not considered, 0:noise, 1:within 10m, 2:low, 3:medium, 4:high|signal_conf_ph|
|quality_ph|ATL03 quality|0:nominal, 1:afterpulse, 2:impulse, 3:tep|additional values added in ATL03 version 7|
|ph_index|index of photon for the given beam in the ATL03 granule|scalar||
|relief|Relative elevation from detected surface, provided by ATL08|meters (float)|Optional: must enable `phoreal`|
|landcover|ATL08 land cover flags||Optional: must enable `phoreal`|
|snowcover|ATL08 snow cover flags||Optional: must enable `phoreal`|
|atl08_class|ATL08 photon classification|0:noise, 1:ground, 2:canopy, 3:top of canopy, 4:unclassified|Optional: must enable `phoreal` or specify `atl08_class`|
|yapc_score|YAPC photon weight|0-255, higher is denser|Optional: must enable `yapc`|
|atl24_class|ATL24 photon classification|0:unclassified, 40:bathymetry, 41:sea surface|Optional: must enable `atl24`|
|atl24_confidence|ATL24 photon classification bathymetry confidence score|0 to 1.0, higher is more confident (float)|Optional: must enable `atl24`|
|spot|ATLAS detector field of view|1-6|Independent of spacecraft orientation|
|cycle|ATLAS orbit cycle number|||
|region|ATLAS granule region|1-14||
|rgt|Reference Ground Track|||
|gt|Beam|'gt1l', 'gt1r', 'gt2l', 'gt2r', 'gt3l', 'gt3r'|Dependent on spacecraft orientation|

### 1.1 Photon-input Parameters

The photon-input parameters allow the user to select an area, a time range, or a specific ATL03 granule to use for input to the photon-selection algorithm.  If multiple parameters are specified, the result will be those photons that match all of the parameters.

* `poly`: polygon defining region of interest (see [polygons](/user_guide/basic_usage.html#polygons))
* `region_mask`: geojson describing region of interest which enables rasterized subsetting on servers (see [geojson](/user_guide/basic_usage.html#rasterized-area-of-interest))
* `track`: reference pair track number (1, 2, 3, or 0 to include for all three; defaults to 0); note that this is combined with the beam selection as a union of the two
* `beams`: list of beam identifiers (gt1l, gt1r, gt2l, gt2r, gt3l, gt3r; defaults to all)
* `spots`: list of spots (1, 2, 3, 4, 5, 6); this is only supporting by the _atl03x_ endpoint
* `rgt`: reference ground track (defaults to all if not specified)
* `cycle`: counter of 91-day repeat cycles completed by the mission (defaults to all if not specified)
* `region`: geographic region for corresponding standard product (defaults to all if not specified)
* `t0`: start time for filtering granules (format %Y-%m-%dT%H:%M:%SZ, e.g. 2018-10-13T00:00:00Z)
* `t1`: stop time for filtering granuels (format %Y-%m-%dT%H:%M:%SZ, e.g. 2018-10-13T00:00:00Z)

### 1.2 Photon-selection Parameters

Once the ATL03 input data are are selected, a set of photon-selection photon parameters are used to select from among the available photons.  At this stage, additional photon-classification algorithms (ATL08, YAPC) may be selected beyond what is available in the ATL03 files.  The criterial described by these parameters are applied together, so that only photons that fulfill all of the requirements are returned.

#### 1.2.1 Native ATL03 Photon Classification

ATL03 contains a set of photon classification values, that are designed to identify signal photons for different surface types with specified confidence:

* `srt`: surface type: 0-land, 1-ocean, 2-sea ice, 3-land ice, 4-inland water
* `cnf`: confidence level for photon selection, can be supplied as a single value (which means the confidence must be at least that), or a list (which means the confidence must be in the list); note - the confidence can be supplied as strings {"atl03_tep", "atl03_not_considered", "atl03_background", "atl03_within_10m", "atl03_low", "atl03_medium", "atl03_high"} or as numbers {-2, -1, 0, 1, 2, 3, 4}.
* `quality_ph`: quality classification based on an ATL03 algorithms that attempt to identify instrumental artifacts, can be supplied as a single value (which means the classification must be exactly that), or a list (which means the classification must be in the list).
* `podppd`: pointing/geolocation degradation mask; each bit in the mask represents a pointing/geolocation solution quality assessment to be included; the bits are 0: nominal, 1: pod_degrade, 2: ppd_degrade, 3: podppd_degrade, 4: cal_nominal, 5: cal_pod_degrade, 6: cal_ppd_degrade, 7: cal_podppd_degrade.

#### 1.2.2 YAPC Classification

The experimental YAPC (Yet Another Photon Classifier) photon-classification scheme assigns each photon a score based on the number of adjacent photons.  YAPC parameters are provided as a dictionary, with entries described below:

* `yapc`: settings for the yapc algorithm; if provided then SlideRule will execute the YAPC classification on all photons
    - `score`: the minimum yapc classification score of a photon to be used in the processing request
    - `knn`: the number of nearest neighbors to use, or specify 0 to allow automatic selection of the number of neighbors (version 2 only)
    - `min_knn`: minimum number of k-nearest neighbors (version 3 only)
    - `win_h`: the window height used to filter the nearest neighbors (overrides calculated value if non-zero)
    - `win_x`: the window width used to filter the nearest neighbors
    - `version`: the version of the YAPC algorithm to use; 0:read from ATL03 granule, 1-3:algorithm version (not supported by `atl03x`)

To run the YAPC algorithm, specify the YAPC settings as a sub-dictionary. Here is an example set of parameters that runs YAPC:

```Python
parms = {
    "cnf": 0,
    "yapc": { "score": 0, "version": 3, "knn": 4 },
    "ats": 10.0,
    "cnt": 5,
    "len": 20.0,
    "res": 20.0
}
```

#### 1.2.3 ATL08 Classification

If ATL08 classification parameters are specified, the ATL08 (vegetation height) files corresponding to the ATL03 files are queried for the more advanced classification scheme available in those files.  Photons are then selected based on the classification values specified.  Note that srt=0 (land) and cnf=0 (no native filtering) should be specified to allow all ATL08 photons to be used.

* `atl08_class`: list of ATL08 classifications used to select which photons are used in the processing (the available classifications are: "atl08_noise", "atl08_ground", "atl08_canopy", "atl08_top_of_canopy", "atl08_unclassified")

#### 1.2.4 ATL24 Classification

If ATL24 classification parameters are specified, the ATL24 (bathymetry) files corresponding to the ATL03 files are queried for the more advanced classification scheme available in those files.  Photons are then selected based on the classification values specified.  Note that srt=-1 (dynamic) and cnf=-1 (no native filtering) should be specified to allow all ATL24 photons to be used.

* `atl24`
  - `class_ph`: list of ATL24 classifications used to select which photons are used in the processing (the available classifications are: "bathymetry", "sea_surface", "unclassified")

:::{note}
ATL24 is typically a release behind the ATL03 standard data product which it is based on.  In order to correlate ATL24 classifications to ATL03, a release of ATL03 must be selected that has a corresponding ATL24 release.
:::

### 1.3 Photon-extent Parameters

Selected photons are divided and aggregated using along-track samples (“extents”) with user-specified length. These extends may or may not align with the original 20-m segments of ATL03 photons.  The _len_ parameter specifies the length of each extent, and the _res_parameter specifies the distance between subsequent extent centers.  If _res_ is less than _len_, subsequent segments will contain duplicate photons.  The API may also select photons based on their along-track distance, or based on the segment-id parameters in the ATL03 product (see the _dist_in_seg_ parameter).

* `len`: length of each extent in meters
* `res`: step distance for successive extents in meters
* `dist_in_seg`: true|false flag indicating that the units of the `len` and `res` are in ATL03 segments (e.g. if true then a len=2 is exactly 2 ATL03 segments which is approximately 40 meters)

Extents are optionally filtered based on the number of photons in each extent and the distribution of those photons.  If the `pass_invalid` parameter is set to _False_, only those extents fulfilling these criteria will be returned.

* `pass_invalid`: true|false flag indicating whether or not extents that fail validation checks are still used and returned in the results
* `ats`: minimum along track spread, which is the distance in meters between the outermost valid photons in the variable length segment
* `cnt`: minimum photon count in segment

### 1.4 Ancillary Data

The ancillary field parameters allow the user to request additional fields from the source datasets being subsetted.  Ancillary data returned from the `atl03x` (as well as the`atl03s` and `atl03sp`) APIs are per-photon values that are read from the ATL03 granules.  No processing is performed on the data read out of the ATL03 granule.  The fields must come from either a per-photon variable (atl03_ph_fields), a per-segment variable (atl03_geo_fields, atl03_corr_fields), or a rate variable (atl03_bckgrd_fields).

Ancillary fields are used to specify additional fields in the ATL03, ATL08, and ATL09 granules to be returned with the photon extent and dowstream customized products. Each field provided by the user will result in a corresponding column added to the returned GeoDataFrame.  Note: if a field is requested that is already present in the default GeoDataFrame, then the name of both fields will be changed to include a _x suffix for the default incusion of the field, and a _y for the ancillary inclusion of the field.  In general, they should have the same value, but in some cases the ancillary field goes through different processing steps and may possibly contain a different value.

* `atl03_bckgrd_fields`: fields in the "bckgrd_atlas" group of the ATL03 granule
* `atl03_geo_fields`: fields in the "geolocation" group of the ATL03 granule
* `atl03_cor_fields`: fields in the "geophys_corr" group of the ATL03 granule
* `atl03_ph_fields`: fields in the "heights" group of the ATL03 granule
* `atl08_fields`: fields in the "land_segments" group of the ATL08 granule
* `atl09_fields`: fields in the "bckgrd_atlas", "high_rate", and "low_rate" groups of the ATL09 granule (must provide the group name in the request, e.g. "low_rate/bsnow_con")

For example:

```Python
parms = {
    "atl03_geo_fields":     ["solar_elevation"],
    "atl03_ph_fields":      ["pce_mframe_cnt"],
    "atl08_fields":         ["asr"]
}
```

### 1.5 ATL06-SR Algorithm

The ATL06-SR algorithm fits a line segment to the photons in each extent, using an iterative selection refinement to eliminate noise photons not correctly identified by the photon classification.  The results are then checked against three parameters : `sigma_r_max`, which eliminates segments for which the robust dispersion of the residuals is too large, and the `ats` and `cnt` parameters described above, which eliminate segments for which the iterative fitting has eliminated too many photons.  The algorithm is run by supplying the `fit` parameter in the processing request, but can also be run via the legacy `atl06` and `atl06p` endpoints.

This algorithm replaces the columns of the source DataFrame with the following columns:
|Field|Description|Units|Notes|
|-----|-----------|-----|-----|
|time_ns|Unix Time|nanoseconds|index column of DataFrame|
|latitude|Fitted latitude of the segment, EPSG:7912|degrees (double)|replaced by geometry column when GeoDataFrame|
|longitude|Fitted longitude of the segment, EPSG:7912|degrees (double)|replaced by geometry column when GeoDataFrame|
|x_atc|Fitted along track distance|meters (double)||
|y_atc|Fitted across track distance|meters (float)||
|photon_start|ATL03 index (per beam) of the first photon in the segment|||
|photon_count|Number of ATL03 photons in the segment|||
|pflags|Processing flags|see [ICESat-2 Processing Flags](./icesat2#3-4-processing-flags)||
|h_mean|Fitted elevation of the segment|meters (float)|vertical datum controlled by parameters, default is ITRF2014|
|dh_fit_dx|Fitted slope of the segment|||
|window_height|Height of window used in final fit|meters||
|rms_misfit||||
|h_sigma||||
|spot|ATLAS detector field of view|1-6|Independent of spacecraft orientation|
|cycle|ATLAS orbit cycle number|||
|region|ATLAS granule region|1-14||
|rgt|Reference Ground Track|||
|gt|Beam|'gt1l', 'gt1r', 'gt2l', 'gt2r', 'gt3l', 'gt3r'|Dependent on spacecraft orientation|

Using the Python client, this service is called via:
```Python
parms = {
  "fit": {}
}
sliderule.run('atl03x', parms)
```

#### 1.5.1 ATL06-SR Parameters

The ATL06-SR parameters are supplied in user requests under the `fit` key and include:
* `fit`:
  - `maxi`: maximum iterations, not including initial least-squares-fit selection
  - `H_min_win`: minimum height to which the refined photon-selection window is allowed to shrink, in meters
  - `sigma_r_max`: maximum robust dispersion in meters

#### 1.5.2 ATL06-SR Ancillary Data

Ancillary data returned from the `fit` algorithm (as well as `atl06` and `atl06p` APIs) come from the ancillary fields specified for ATL03, but instead of being returned as-is, they are processed using the ATL06 least-squares-fit algorithm, and only the result is returned.  In other words, ancillary data points from ATL03 to be included in an ATL06-SR result are treated just like the h_mean, latitude, and longitude variables, and returned as a fitted double-precision floating point value.

### 1.6 PhoREAL Algorithm

The PhoREAL algorithm is a modified version of the ATL08 canopy metrics algorithm developed at the University of Texas at Austin that calculates canopy metrics on a segment of ATL03 photons.  The algorithm is run by supplying the `phoreal` parameter in the `atl03x` request, but can also be accessed via the legacy endpoints `atl08` and `atl08p`.

This algorithm replaces the columns of the source DataFrame with the following columns:
|Field|Description|Units|Notes|
|-----|-----------|-----|-----|
|time_ns|Unix Time|nanoseconds|index column of DataFrame|
|latitude|EPSG:7912|degrees (double)|replaced by geometry column when GeoDataFrame|
|longitude|EPSG:7912|degrees (double)|replaced by geometry column when GeoDataFrame|
|x_atc|Along track distance|meters (double)|dist_ph_along + segment_distance|
|y_atc|Across track distance|meters (float)|dist_ph_across|
|photon_start|ATL03 index (per beam) of the first photon in the segment|||
|photon_count|Number of ATL03 photons in the segment|||
|pflags|Processing flags|see [ICESat-2 Processing Flags](./icesat2#3-4-processing-flags)||
|ground_photon_count|Number of photons classified as ground in the segment|||
|vegetation_photon_count|Number of photons classified as canopy or top of canopy in the segment|||
|landcover|ATL08 land cover flags|||
|snowcover|ATL08 snow cover flags|||
|solar_elevation|Sun elevation as provided in ATL03|degrees (float)||
|h_te_median|Median ellipsoidal height of the ground photons|meters (float)|vertical datum controlled by parameters, default is ITRF2014|
|h_max_canopy|Maximum relief height for canopy photons|meters (float)||
|h_min_canopy|Minimum relief height for canopy photons|meters (float)||
|h_mean_canopy|Mean relief height for canopy photons|meters (float)||
|h_canopy|98th percentile relief height for canopy photons|meters (float)||
|canopy_openness|Standard deviation of relief height for canopy photons|||
|canopy_h_metrics|relief height at given percentile for canopy photons|meters (float)|5th to 95th percentile provided, in increments of 5%, 20 percentiles total|
|spot|ATLAS detector field of view|1-6|Independent of spacecraft orientation|
|cycle|ATLAS orbit cycle number|||
|region|ATLAS granule region|1-14||
|rgt|Reference Ground Track|||
|gt|Beam|'gt1l', 'gt1r', 'gt2l', 'gt2r', 'gt3l', 'gt3r'|Dependent on spacecraft orientation|

Using the Python client, this service is called via:
```Python
parms = {
  "phoreal": {}
}
sliderule.run('atl03x', parms)
```

#### 1.6.1 PhoREAL Parameters

The PhoREAL parameters are supplied in user requests under the `phoreal` key and include:
* `phoreal`:
  - `binsize`: size of the vertical photon bin in meters
  - `geoloc`: algorithm to use to calculate the geolocation (latitude, longitude, along-track distance, and time) of each custom length PhoREAL segment; "mean" - takes the average value across all photons in the segment; "median" - takes the median value across all photons in the segment; "center" - takes the halfway value calculated by the average of the first and last photon in the segment
  - `use_abs_h`: boolean whether the absolute photon heights are used instead of the normalized heights
  - `send_waveform`: boolean whether to send to the client the photon height histograms in addition to the vegetation statistics
  - `above_classifier`: boolean whether to use the ABoVE photon classifier when determining top of canopy photons

#### 1.6.2 ATL08-PhoREAL Ancillary Data

Ancillary data returned from the `atl08` and `atl08p` APIs come from the land_segments group of the ATL08 granules.  The data goes through a series of processing steps before being returned back to the user as per-extent (i.e. variable-length segment) result values.

* When a user requests an ATL08 ancillary field, the ATL08 classifications are automatically enabled with all unclassified photons filtered out (i.e. noise, ground, canopy, and top of canopy are included; unclassified photons are excluded).  If the user is also requesting PhoREAL processing, then noise photons are automatically filtered out as well.  Lastly, if the user manually specifies which ATL08 photon classifications to use, then that manual specification takes precedence and is used.
* If a user manually specifies that unclassified photons are to be included, the value used for an ancillary field for that photon has all 1's in the binary encoding of that value.  For example, if it is an 8-bit unsigned integer, the value would be 255.  If it is a double-precision floating point, the value would be -nan.
* Since the ATL08 APIs return per-extent values and not per-photon values, the set of per-photon ancillary field values must be reduced in some way to a single per-extent value to be returned back to the user.  There are currently two options available for how this reduction occurs.

  1. Nearest Neighbor (Mode): the value that appears most often in the extent is selected.  This is the default method.  For example, if a user specifies `atl08_fields": ["asr"]`` in their parameters, and the PhoREAL extent consists of mostly two-thirds of one ATL08 segment, and a third of the next ATL08 segment (and if the photons rate is consistent across those two ATL08 segments), then the value returned by the nearest neighbor processing will be the ATL08 land_segment value from the first ATL08 segment.
  2. Interpolation (Average): the value that is the average of all the per-photon values provided.  This is only performed when the user appends a "%" to the end of the field's name in the field list parameter.  For example, if a user specifies `atl08_fields": ["asr%"]`` in their parameters, then the returned result is the average of all of the "asr" values in the PhoREAL extent.  Note that the GeoDataFrame returned to the user will include the "%" in the column name for this particular field - that is so a user can request both nearest neighbor and interpolated values for the same field in a single request.

## 2. ATL06 - `atl06x`

The SlideRule `atl06x` endpoint provides a service for ATL06 subsetting and custom processing.  This endpoint queries ATL06 input granules for segment heights and locations based on geographic and temporal ranges.  The resulting extents are typically directly returned to the client, but may be passed to downstream algorithms and custom processing steps like raster sampling.

Using the Python client, this service is called via:
```Python
sliderule.run('atl06x', parms)
```

The default resulting DataFrame from this endpoint contains the following columns:
|Field|Description|Units|Notes|
|-----|-----------|-----|-----|
|time_ns|Unix Time|nanoseconds|index column of DataFrame|
|latitude|EPSG:7912|degrees (double)|replaced by geometry column when GeoDataFrame|
|longitude|EPSG:7912|degrees (double)|replaced by geometry column when GeoDataFrame|
|x_atc|Along track distance|meters (double)|land_ice_segments/ground_track/x_atc|
|y_atc|Across track distance|meters (float)|land_ice_segments/ground_track/y_atc|
|h_li|Median-based height of segment|meters (float)|land_ice_segments/h_li|
|h_li_sigma|Propagated error due to sampling error and FPB correction|meters (float)|land_ice_segments/h_li_sigma|
|sigma_geo_h|Total vertical geolocation error due to PPD and POD|meters (float)|land_ice_segments/sigma_geo_h|
|atl06_quality_summary|Best-quality subset of all ATL06 data|0: no data-quality tests have found a problem with the segment, 1: some potential problem has been found|land_ice_segments/atl06_quality_summary|
|segment_id|Segment ID for the second of the two 20m ATL03 segments included in the 40m ATL06 segment|count|land_ice_segments/segment_id|
|seg_azimuth|Azimuth of the pair-track, east of local north|degrees (float)|land_ice_segments/ground_track/seg_azimuth|
|dh_fit_dx|Along-track slope from along-track segment fit|meters (float)|land_ice_segments/fit_statistics/dh_fit_dx|
|h_robust_sprd|RDE of misfit between PE heights and the along-track segment fit|meters (float)|land_ice_segments/fit_statistics/h_robust_sprd|
|w_surface_window_final|Width of the surface window, top to bottom|meters (float)|land_ice_segments/fit_statistics/w_surface_window_final|
|bsnow_conf|Confidence flag for presence of blowing snow|boolean|land_ice_segments/geophysical/bsnow_conf|
|bsnow_h|Blowing snow layer top height|meters (float)|land_ice_segments/geophysical/bsnow_h|
|r_eff|Effective reflectance, uncorrected for atmospheric effects.|(float)|land_ice_segments/geophysical/r_eff|
|tide_ocean|Ocean tides|meters (float)|land_ice_segments/geophysical/tide_ocean|
|n_fit_photons|Number of PEs used in determining h_li|count|land_ice_segments/fit_statistics/n_fit_photons|
|spot|ATLAS detector field of view|1-6|Independent of spacecraft orientation|
|cycle|ATLAS orbit cycle number|||
|region|ATLAS granule region|1-14||
|rgt|Reference Ground Track|||
|gt|Beam|'gt1l', 'gt1r', 'gt2l', 'gt2r', 'gt3l', 'gt3r'|Dependent on spacecraft orientation|

### 2.1 Ancillary Data

Ancillary data returned from the `atl06x` endpoint (as well as `atl06` and `atl06p` endpoints) come from the **land_ice_segments** group of the ATL06 granules. The data is mostly returned as-is, with one exception.  Double-precision and single-precision floating point variables are checked to see if they contain the maximum value of their respective encodings, and if so, a floating point NaN (not-a-number) is returned instead.  This check is not performed for integer variables because the maximum value of an encoded integer can sometimes be a valid value (e.g. bit masks).

* `atl06_fields`: fields in the "land_ice_segments" group of the ATL06 granule, provided as a list of strings

For example,
```Python
parms = {
    "atl06_fields": ["ground_track/ref_azimuth"],
}
gdf = sliderule.run("atl06x", parms)
```

## 3. ATL08 - `atl08x`

The SlideRule `atl08x` endpoint provides a service for ATL08 subsetting and custom processing.  This endpoint queries ATL08 input granules for segment vegetation statistics and locations based on geographic and temporal ranges.  These statistics are typically directly returned to the client, but may be passed to downstream algorithms and custom processing steps like raster sampling.

Using the Python client, this service is called via:
```Python
sliderule.run('atl08x', parms)
```

The default resulting DataFrame from this endpoint contains the following columns:
|Field|Description|Units|Notes|
|-----|-----------|-----|-----|
|time_ns|Unix Time|nanoseconds|index column of DataFrame|
|latitude|EPSG:7912|degrees (double)|replaced by geometry column when GeoDataFrame|
|longitude|EPSG:7912|degrees (double)|replaced by geometry column when GeoDataFrame|
|x_atc|Along track distance|meters (double)|land_ice_segments/ground_track/x_atc|
|y_atc|Across track distance|meters (float)|land_ice_segments/ground_track/y_atc|
|segment_id_beg|First ATL03 segment used in ATL08 100m segment|count|land_segments/segment_id_beg|
|segment_landcover|UN-FAO Land Cover Surface type classification as reference from Copernicus Land Cover(ANC18) at the 100m resolution||land_segments/segment_landcover|
|segment_snowcover|Daily snow/ice cover from ATL09 at the 25 Hz rate(275m) indicating likely presence of snow and ice within each segment|0: ice free water; 1: snow free land;  2: snow; 3: ice|land_segments/segment_snowcover|
|n_seg_ph|Number of photons within each land segment|count|land_segments/n_seg_ph|
|solar_elevation|Solar elevation at time of measurement|degrees (float)|land_segments/solar_elevation|
|terrain_slope|Along-track slope of the terrain|meters (float)|land_segments/terrain/terrain_slope|
|n_te_photons|Number of terrain (ground) photons|count|land_segments/terrain/n_te_photons|
|h_te_uncertainty|Uncertainty of height of terrian|meters (float)|land_segments/terrain/h_te_uncertainty|
|h_te_median|Median height of the terrain|meters (float)|land_segments/terrain/h_te_median|
|h_canopy|98 percentile height of canopy photons|meters (float)|land_segments/canopy/h_canopy (or land_segments/canopy/h_canopy_abs if use_abs_h is true)|
|h_canopy_uncertainty|Vertical uncertainty of canopy height|meters (float)|land_segments/canopy/h_canopy_uncertainty|
|segment_cover|Average percentage value of the valid Copernicus fractional cover product|scalar|land_segments/canopy/segment_cover|
|n_ca_photons|Number of canopy photons||land_segments/canopy/n_ca_photons|
|h_max_canopy|Maximum canopy height|meters (float)|land_segments/canopy/h_max_canopy (or land_segments/canopy/h_max_canopy_abs if use_abs_h is true)|
|h_min_canopy|Minimum canopy height|meters (float)|land_segments/canopy/h_min_canopy (or land_segments/canopy/h_min_canopy_abs if use_abs_h is true)|
|h_mean_canopy|Mean canopy height|meters (float)|land_segments/canopy/h_mean_canopy (or land_segments/canopy/h_mean_canopy_abs if use_abs_h is true)|
|canopy_openness|Standard Deviation of all canopy photons|meters (float)|land_segments/canopy/canopy_openness|
|canopy_h_metrics|Cumulative distribution of relative canopy heights calculated at the following percentiles: 10, 15, 20, 25, 30, 35, 40, 45, 50, 55, 60, 65, 70, 75, 80, 85, 90, 95|meters (float)|land_segments/canopy/canopy_h_metrics (or land_segments/canopy/canopy_h_metrics_abs if use_abs_h is true)|
|spot|ATLAS detector field of view|1-6|Independent of spacecraft orientation|
|cycle|ATLAS orbit cycle number|||
|region|ATLAS granule region|1-14||
|rgt|Reference Ground Track|||
|gt|Beam|'gt1l', 'gt1r', 'gt2l', 'gt2r', 'gt3l', 'gt3r'|Dependent on spacecraft orientation|

### 3.1 Quality Filter Parameters

The ATL08 data can be filtered based on different quality filters.
* `te_quality_score`: terrain quality score threshold
* `can_quality_score`: canopy quality score threshold

### 3.2 Ancillary Data

Ancillary data returned from the `atl08x` endpoint (as well as `atl08` and `atl08p` endpoints) come from the **{beam}** group of the ATL08 granules.

* `atl08_fields`: fields in the beam group of the ATL08 granule, provided as a list of strings

For example,
```Python
parms = {
    "atl08_fields": ["asr"],
}
gdf = sliderule.run("atl08x", parms)
```

## 4. ATL13 - `atl13x`

The SlideRule `atl13x` endpoint provides a service for ATL13 subsetting and custom processing.  This endpoint queries ATL13 input granules for segment inland lake statistics based on geographic and temporal ranges.  These statistics are typically directly returned to the client, but may be passed to downstream algorithms and custom processing steps like raster sampling.

This endpoint is called via:
```python
sliderule.run('atl13x', parms)
```

The default resulting DataFrame from this API contains the following columns:
|Field|Description|Units|Notes|
|-----|-----------|-----|-----|
|time_ns|Unix Time|nanoseconds|index column of DataFrame|
|latitude|segment coordinate (replaced by geometry column when GeoDataFrame)|degrees (double)|EPSG:7912|
|longitude|segment coordinate (replaced by geometry column when GeoDataFrame)|degrees (double)|EPSG:7912|
|ht_ortho|Orthometric height of the water surface|meters (float)|EGM08|
|ht_water_surf|Ellipsoidal height of the water surface|meters (float)|WGS84|
|stdev_water_surf|Derived standard deviation of water surface|meters (float)||
|water_depth|Depth from the mean water surface to detected bottom|meters (float)||
|spot|ATLAS detector field of view|1-6|Independent of spacecraft orientation|
|cycle|ATLAS orbit cycle number|||
|rgt|Reference Ground Track|||
|gt|Beam|'gt1l', 'gt1r', 'gt2l', 'gt2r', 'gt3l', 'gt3r'|Dependent on spacecraft orientation|

### 4.1 Inland Lake Parameters

Inland lake data can be queried using the following parameters under the `atl13` key:
* `atl13`:
  - `refid`: ATL13 reference id
  - `name`: lake (or body of water) name
  - `coord`: latitude and longitude coordinates contained within the desired body of water|object {"lat": $lat, "lon": $lon}

### 4.2 Ancillary Data

Ancillary data returned from the `atl13x` endpoint comes from the **{beam}** group of the ATL13 granules.

* `atl13_fields`: fields in the beam group of the ATL13 granule, provided as a list of strings

For example,
```Python
parms = {
    "atl08_fields": ["ice_flag"],
}
gdf = sliderule.run("atl13x", parms)
```

## 5. ATL24 - `atl24x`

The SlideRule `atl24x` endpoint provides a service for ATL24 subsetting and custom processing.  This endpoint queries ATL24 input granules for bathymetry data for ATL03 photons based on geographic and temporal ranges.  ATL24 provides bathymetry labels and metrics which are typically directly returned to the client, but may be passed to downstream algorithms and custom processing steps like raster sampling.

This endpoint is called via:
```python
sliderule.run('atl24x', parms)
```

The default resulting DataFrame from this API contains the following columns:
|Field|Description|Units|Notes|
|-----|-----------|-----|-----|
|time_ns|Unix Time|nanoseconds|index column of DataFrame|
|lat_ph|EPSG:7912|degrees (double)|refraction corrected, replaced by geometry column when GeoDataFrame|
|lon_ph|EPSG:7912|degrees (double)|refraction corrected, replaced by geometry column when GeoDataFrame|
|x_atc|Along track distance|meters (double)|not refraction corrected, dist_ph_along + segment_distance|
|y_atc|Across track distance|meters (float)|not refraction corrected, dist_ph_across|
|ortho_h|Orthometric height of photon (elevation above geoid)|meters (float)|EGM08|
|surface_h|Orthometric height of calculated sea surface|meters (float)|EGM08|
|class_ph|Photon classification|0:unclassified, 40: bathymetry, 41:sea surface||
|confidence|Bathymetry confidence|0 to 1.0, higher is more confident (float)||
|ellipse_h|Elliptical height of photon (elevation above ellipse)|meters (float)|ITRF2014, Optional: `compact` set to false|
|invalid_kd|Kd was not able to be retrieved for time and location of photon|0:valid, 1:invalid|used in uncertainty calculation, Optional: `compact` set to false|
|invalid_wind_speed|wind speed was not able to be retrieved for time and location of photon|0:valid, 1:invalid|used in uncertainty calculation, Optional: `compact` set to false|
|low_confidence_Flag|Bathymetry confidence is less than 0.6|0:high confidence, 1:low confidence|Optional: `compact` set to false|
|night_flag|Photon collected at night, solar elevation < 5 degrees|0:day, 1:night|Optional: `compact` set to false|
|sensor_depth_exceeded|Turbidity of water and depth of photon indicate unlikely return|0:valid, 1:exceeded|Optional: `compact` set to false|
|sigma_thu|Total horizontal uncertainty|meters (float)|Optional: `compact` set to false|
|sigma_tvu|Total vertical uncertainty|meters (float)|Optional: `compact` set to false|
|spot|ATLAS detector field of view|1-6|Independent of spacecraft orientation|
|cycle|ATLAS orbit cycle number|||
|region|ATLAS granule region|1-14||
|rgt|Reference Ground Track|||
|gt|Beam|'gt1l', 'gt1r', 'gt2l', 'gt2r', 'gt3l', 'gt3r'|Dependent on spacecraft orientation|

### 5.1 Query Parameters

The following parameters are supported under the `atl24` key for customizing the request to ATL24 and filtering which data is returned.
* `atl24`:
  - `compact`: reduces number of fields to minimal viable set (boolean)
  - `class_ph`: ATL24 classification filter (list; 0:unclassified, 40:bathymetry, 41:sea surface)
  - confidence_threshold|minimal bathymetry confidence score|double; 0 to 1.0|0|
  - `invalid_kd`: invalid kd flag values to allow ("on": includes only photons with invalid kd; "off": includes only photons without invalid kd; defaults to both when not specified)
  - `invalid_wind_speed`: invalid wind speed flag values to allow ("on": includes only photons with invalid wind speed; "off": includes only photons without invalid wind speed; defaults to both when not specified)
  - `low_confidence`: low confidence flag values to allow ("on": includes only low confidence photons; "off": includes only high confidence photons; defaults to both when not specified)
  - `night`: night flag values to allow ("on": includes only photons collected at night; "off": includes only photons collected during the day; defaults to both when not specified)
  - `sensor_depth_exceeded`: sensor depth exceeded flag values to allow ("on": includes only photons at a depth greater than the sensor depth; "off": includes only photons at a depth less then the sensor depth; defaults to both when not specified)

### 5.2 Ancillary Data

Ancillary data returned from the `atl24x` endpoint comes from the **{beam}** group of the ATL24 granules.

* `anc_fields`: fields in the beam group of the ATL24 granule, provided as a list of strings

For example,
```Python
parms = {
    "anc_fields": ["index_ph"],
}
gdf = sliderule.run("atl24x", parms)
```

## Appendix A: Legacy Endpoints

For legacy endpoints, each extent is uniquely identified by an extent ID. The extent ID is analogous to the ATL03 segment ID, and is consistently generated for any extent given the same input parameters.  This means subsequent runs of SlideRule with the same request parameters will return the same extent IDs.

While all data returned from SlideRule for ATL03/06/08 endpoints include the extent ID (as `extent_id`), by default the Python client strips it out when it creates the final GeoDataFrame. There is an option to keep the extend ID by setting the "keep_id" argument in the atl03/06/08 group of Python functions to True.  This is useful when performing merges on GeoDataFrames from multiple APIs (for example, you can combine results from atl06 and atl08 endpoints and created a single GeoDataFrame with both elevation and vegatation data in it).

Result times returned by SlideRule for legacy endpoints are in standard Unix nanoseconds, while times provided in the ICESat-2 standard data products are in seconds from the ATLAS Epoch. The server-side code performs this conversion for you.  But if you are using ICESat-2 products direction in addition to SlideRule results, and need to convert between them, you must first convert the unix time to standard GPS time, and then you need to subtract the number of seconds between the GPS epoch which is January 6, 2018 at midnight (1980-01-06T00:00:00.000000Z) and the ATLAS SDP epoch of January 1, 2018 at midnight (2018-01-01T00:00:00.000000Z). That number is `1198800018` seconds.

### A.1 Segmented Photon Data - `atl03sp`

The photon data is stored as along-track segments inside the ATL03 granules, which is then broken apart by SlideRule and re-segmented according to processing
parameters supplied at the time of the request. The new segments are called **extents**.  When the length of an extent is 40 meters, and the step size is 20 meters, the extent matches the ATL06 segments.

Most of the time, the photon extents are kept internal to SlideRule and not returned to the user.  But there are some APIs that do return raw photon extents for the user to process on their own.

Even though this offloads processing on the server, the API calls can take longer since more data needs to be returned to the user, which can bottleneck over the network.

Photon extents are returned as GeoDataFrames where each row is a photon.  Each extent represents the data that the ATL06 algorithm uses to generate a single ATL06 elevation.
When the step size is shorter than the length of the extent, the extents returned overlap each other which means that each photon is being returned multiple times and will be duplicated in the resulting GeoDataFrame.

The GeoDataFrame for each photon extent has the following columns:

- `track`: reference pair track number (1, 2, 3)
- `sc_orient`: spacecraft orientation (0: backwards, 1: forwards)
- `rgt`: reference ground track
- `cycle`: cycle
- `segment_id`: segment ID of first ATL03 segment in result
- `segment_dist`: along track distance from the equator to the center of the extent (in meters)
- `count`: the number of photons in the segment
- `time`: nanoseconds from Unix epoch (January 1, 1970) without leap seconds
- `latitude`: latitude (-90.0 to 90.0)
- `longitude`: longitude (-180.0 to 180.0)
- `x_atc`: along track distance of the photon in meters (with respect to the center of the segment)
- `y_atc`: across track distance of the photon in meters
- `across`: across track distance of the photon in meters
- `height`: height of the photon in meters
- `solar_elevation`: solar elevation from ATL03 at time of measurement, in degrees
- `background_rate`: background photon counts per second
- `atl08_class`: the photon's ATL08 classification (0: noise, 1: ground, 2: canopy, 3: top of canopy, 4: unclassified)
- `atl03_cnf`: the photon's ATL03 confidence level (-2: TEP, -1: not considered, 0: background, 1: within 10m, 2: low, 3: medium, 4: high)
- `quality_ph`: the photon's quality classification (0: nominal, 1: possible after pulse, 2: possible impulse responpse effect, 3: possible tep)
- `yapc_score`: the photon's YAPC classification (0 - 255, the larger the number the higher the confidence in surface reflection)

Note: when PhoREAL is enabled, the ATL03 extent records (_atl03rec_) are enhanced to include the following populated fields:

- `relief`: ATL08 normalized photon heights
- `landcover`: ATL08 landcover flags
- `snowcover`: ATL08 snowcover flags

### A.2 Elevations - `atl06p`

The primary result returned by SlideRule for ICESat-2 ATL06-SR processing requests is a set of geolocated elevations corresponding to a geolocated ATL03 along-track segment. The elevations are contained in a GeoDataFrame where each row represents a calculated elevation.

The elevation GeoDataFrame has the following columns:

- `extent_id`: unique ID associated with custom ATL03 segment (removed from final GeoDataFrame by default)
- `segment_id`: segment ID of first ATL03 segment in result
- `n_fit_photons`: number of photons used in final calculation
- `pflags`: processing flags (0x1 - spread too short; 0x2 - too few photons; 0x4 - max iterations reached)
- `rgt`: reference ground track
- `cycle`: cycle
- `region`: region of source granule
- `spot`: laser spot 1 to 6
- `gt`: ground track (10: GT1L, 20: GT1R, 30: GT2L, 40: GT2R, 50: GT3L, 60: GT3R)
- `x_atc`: along track distance from the equator in meters
- `time`: nanoseconds from Unix epoch (January 1, 1970) without leap seconds
- `lat`: latitude (-90.0 to 90.0)
- `lon`: longitude (-180.0 to 180.0)
- `h_mean`: elevation in meters from ellipsoid
- `dh_fit_dx`: along-track slope
- `y_atc`: across-track distance
- `w_surface_window_final`: width of the window used to select the final set of photons used in the calculation
- `rms_misfit`: measured error in the linear fit of the surface
- `h_sigma`: error estimate for the least squares fit model

### A.3 Vegetation Metrics (PhoREAL) - `atl08p`

The primary result returned by SlideRule for ICESat-2 PhoREAL processing requests is a set of geolocated vegetation metrics corresponding to a geolocated ATL03 along-track segment. The metrics are contained in a GeoDataFrame where each row represents a segment.

The vegetation GeoDataFrame has the following columns:

- `extent_id`: unique ID associated with custom ATL03 segment (removed from final GeoDataFrame by default)
- `segment_id`: segment ID of first ATL03 segment in result
- `rgt`: reference ground track
- `cycle`: cycle
- `region`: region of source granule
- `spot`: laser spot 1 to 6
- `gt`: ground track (10: GT1L, 20: GT1R, 30: GT2L, 40: GT2R, 50: GT3L, 60: GT3R)
- `ph_count`: total number of photons used by PhoREAL algorithm for this extent
- `gnd_count`: number of ground photons used by PhoREAL algorithm for this extent
- `veg_count`: number of vegetation (canopy and top of canopy) photons used by PhoREAL algorithm for this extent
- `landcover`: flag indicating if segment includes land surfaces
- `snowcover`: flag indicating if snow is present in the segment
- `time`: nanoseconds from Unix epoch (January 1, 1970) without leap seconds
- `lat`: latitude (-90.0 to 90.0)
- `lon`: longitude (-180.0 to 180.0)
- `x_atc`: along track distance from the equator in meters
- `solar_elevation`: solar elevation from ATL03 at time of measurement, in degrees
- `h_te_median`: median terrain elevation in meters (absolute heights)
- `h_max_canopy`: maximum relief height for canopy photons
- `h_min_canopy`: minimum relief height for canopy photons
- `h_mean_canopy`: average relief height for canopy photons
- `h_canopy`: 98th percentile relief height for canopy photons
- `canopy_openness`: standard deviation of relief height for canopy photons
- `canopy_h_metrics`: relief height at given percentile for canopy photons

### A.4 Callbacks

For large processing requests, it is possible that the data returned from the API is too large or impractical to fit in the local memory of the Python interpreter making the request.
In these cases, certain APIs in the SlideRule Python client allow the calling application to provide a callback function that is called for every result that is returned by the servers.

If a callback is supplied, the API will not return back to the calling application anything associated with the supplied record types, but assumes the callback fully handles processing the data.

The callback function takes the following form `callback(record, session)` and handles processing streamed records for a request. The _record_ parameter is the passback variable that contains the streamed record.  The _session_ parameter is the passback variable holding the current SlideRule client session.

Here is an example of a callback being used for the `atl03sp` function:
```Python
rec_cnt = 0
ph_cnt = 1

def atl03rec_cb(rec, session):
    global rec_cnt, ph_cnt
    rec_cnt += 1
    ph_cnt += rec["count"][0] + rec["count"][1]
    print("{} {}".format(rec_cnt, ph_cnt), end='\r')

gdf = icesat2.atl03sp({}, callbacks = {"atl03rec": atl03rec_cb})
```
