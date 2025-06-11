# DataFrames

SlideRule includes a set of APIs that are specifically geared for generating and processing DataFrames.  These APIs were made public in early 2025 starting with version 4.11.0, and have a common methodology for processing the data which makes interfacing to them consistent across multiple datasets.

DataFrame APIs are accessed via the following ***sliderule*** function and always produce a __GeoDataFrame__:
```python
gdf = sliderule.run(api, parms, aoi=None, resources=None)
```
where:
* `api` is a string that specifies which SlideRule API is being called
* `parms` is the request parameter dictionary
* `aoi` is the polygon defining the area of interest (provided for convenience only; if not supplied, the area of interest is taken directly from __parms__)
* `resources` is the list of granules to process (provided for convenience only; if not supplied, the resources are taken directly from __parms__)

The columns in the returned __GeoDataFrame__ depend on the contents of the `parms` structure passed to the `api`.  Typically, there are a base set of columns defined for each `api`.  If __algorithms__ are specified in the `parms`, then the columns may be completely different depending upon the output of the __algorithms__.  If __ancillary fields__ or __raster samplers__ are specified, then there may be additional columns.

Metadata returned with the results of the API is placed in the `attrs` attribute of the __GeoDataFrame__.  There are three groups of metadata:
* `meta`: the metadata provided in the server-side dataframe; typically includes the `endpoint`, and the `srctbl` which correlates the `srcid` column values to source granules
* `sliderule`: the server-side parameters used in processing the request along with versioning information
* `recordinfo`: identifies special columns in the dataframe for generalized processing (e.g. time, x, y, z)

Here is an example for accessing the source granule of an entry in the dataframe with a given `srcid`.
```python
gdf = sliderule.run(api, parms)
gdf.attrs['meta']['srctbl'][f'{srcid}']
```
## APIs

The following APIs support the DataFrame interface.

### atl03x

This API is used to subset and operate on the ICESat-2 ATL03 photon cloud data and is called via:
```python
sliderule.run('atl03x', parms)
```
The default resulting DataFrame from this API contains the following columns:
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

### atl13x

This API is used to subset and operate on the ICESat-2 ATL13 inland lake data and is called via:
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

### atl24x

This API is used to subset and operate on the ICESat-2 ATL24 bathymetry classified photons and is called via:
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

## Algorithms

When an __algorithm__ is run, it is run on the default dataframe produced by the `api` and either generates a new dataframe, or appends additional columns onto the dataframe.

### Sampler

The sampler algorithm uses the ___latitude___ and ___longitude___ of each element (or row) of the DataFrame and __samples__ the specified raster dataset, and appends the results to the DataFrame.  In some cases, the ___height___ field is also used to calculate and apply a vertical offset.  For more information, see [Raster Sampling](./raster_sampling).

Here are the additional columns added to the DataFrame:
|Field|Description|Units|Notes|
|-----|-----------|-----|-----|
|value|the sampled value from the raster|(double)||
|time|the best time provided by the raster dataset for when the sampled value was measured|Unix nanoseconds (double)||
|file_id|a number used to identify the name of the file the sample value came from|||
|flags|any flags that acompany the sampled data in the raster it was read from||Optionally populated: must enable `with_flags`|
|count|number of pixels read to calculate sample value||Optional: must enable `zonal_stats`|
|min|minimum pixel value of pixels that contributed to sample value||Optional: must enable `zonal_stats`|
|max|maximum pixel value of pixels that contributed to sample value||Optional: must enable `zonal_stats`|
|mean|average/mean pixel value of pixels that contributed to sample value||Optional: must enable `zonal_stats`|
|median|average/median pixel value of pixels that contributed to sample value||Optional: must enable `zonal_stats`|
|stdev|standard deviation of pixel values of pixels that contributed to sample value||Optional: must enable `zonal_stats`|
|mad|median absolute deviation of pixel values of pixels that contributed to sample value||Optional: must enable `zonal_stats`|

In addition to the above columns, a `file_directory` is added to the metadata of the DataFrame providing a mapping between the `file_id` and the name of the raster file sampled.

### Surface Fitter

The surface fitting algorithm is a modified version of the ATL06 algorithm developed at the University of Washington that fits a line to a segment of photons and returns a representative height and slope for the segment.

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

### PhoREAL

The PhoREAL algorithm is a modified version of the ATL08 canopy metrics algorithm developed at the University of Texas at Austin that calculates canopy metrics on a segment of ATL03 photons.

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

## Parameters

The parameter structure in SlideRule is hierarchical, in that mission specific parameter sets inherit all of the parameters from the packages they support (including the core package in all cases).

To retrieve a full set of all parameters supported by SlideRule, and to also see the default values used by the server-side code, you can issue the following API call from a terminal:
```bash
curl https://sliderule.slideruleearth.io/source/defaults
```

### Core Request Fields

|Parameter|Description|Units|Default|
|---------|-----------|-----|-------|
|asset|Source data asset, see [asset directory](https://github.com/SlideRuleEarth/sliderule/blob/main/targets/slideruleearth-aws/asset_directory.csv)|string|api specific|
|resources|List of granule/tile file names to process|list of strings||
|poly|List of latitudes and longitudes defining the area of interest|list of lat/lon dictionaries||
|proj|Projection of area of interest|0:North Polar, 1:South Polar, 2:Plate Carree|Plate Carree|
|datum|Vertical datum returned heights are in|1:ITRF2014, 2:EGM08|ITRF2014|
|timeout|Global timeout used for all aspects of processing request|seconds|600|
|rqst_timeout|Maximum duration of proxied request|seconds|600|
|node_timeout|Maximum duration of a single node processing|seconds|600|
|read_timeout|Maximum duration of S3 read|seconds|600|
|cluster_size_hint|Suggest to proxy how many nodes to use in the cluster|||
|region_mask|Complex area of interest, see [region mask](#region-mask)|||
|output|Arrow output specification, see [arrow output](./arrow_output)|||
|samples|Raster sampling specification, see [raster sampling](./raster_sampling)|||

#### Region Mask

|Parameter|Description|Units|Default|
|---------|-----------|-----|-------|
|geojson|Geometry of area of interest|geojson string||
|cellsize|Resolution of rasterized area of interest|degrees||

### ICESat-2 Request Fields

|Parameter|Description|Units|Default|
|---------|-----------|-----|-------|
|srt|Surface reference type to use when assigning ATL03 Confidence|-1:dynamic, 0:land, 1:ocean, 2:sea ice, 3:land ice, 4:inland water|dynamic|
|pass_invalid|Include results that fail quality checks|boolean|false|
|dist_in_seg|Treat all distance parameters as being expressed in segments|boolean|false|
|cnf|ATL03 confidence filter, when supplied as single value, used as minimum|list/int; -2:tep, -1:not considered, 0:background, 1:within 10m, 2:low, 3:medium, 4:high|2|
|quality_ph|ATL03 quality filter|list; 0:nominal, 1:afterpulse, 2:impulse, 3:tep|0|
|atl08_class|ATL08 classification filter|list; 0:noise, 1:ground, 2:canopy, 3:top of canopy, 4:unclassified|when using `phoreal` and not specified, will default to ground, canopy, and top of canopy|
|beams|Beam selection|list; 'gt1l', 'gt1r', 'gt2l', 'gt2r', 'gt3l', 'gt3r'|all|
|track|Track selection|list; 1, 2, 3|all|
|len|Length of photon segment|meters|40.0|
|res|Resolution or step size of photon segment|meters|20.0|
|ats|Minimum allowed along track spread of a photon segment|meters|20.0|
|cnt|Minimum allowed number of photons in a segment||10|
|fit|Surface fitting algorithm parameters, see [surface fit](#surface-fit)||
|yapc|YAPC algorithm parameters, see [yapc](#yapc)||
|phoreal|PhoREAL algorithm parameters, see [phoreal](#phoreal)||
|atl24|ATL24 algorithm parameters, see [atl24](#atl24)||
|atl03_geo_fields|ATL03 ancillary fields from the __geolocation__ group|list of strings||
|atl03_corr_fields|ATL03 ancillary fields from the __geophys_corr__ group|list of strings||
|atl03_ph_fields|ATL03 ancillary fields from the __heights__ group|list of strings||
|atl06_fields|ATL06 ancillary fields|list of strings|path from beam group must be supplied|
|atl08_fields|ATL08 ancillary fields|list of strings|path from beam group must be supplied|
|atl13_fields|ATL13 ancillary fields|list of strings||
|granule|ATL03 granule name filters, see [granule](#granule)|string||

#### Surface Fit

|Parameter|Description|Units|Default|
|---------|-----------|-----|-------|
|maxi|Maximum iterations on least squares fit|int|5|
|h_win|Minimum window|double; meters|3.0|
|sigma_r|Maximum robust dispersion|double; meters|5.0|

#### YAPC

|Parameter|Description|Units|Default|
|---------|-----------|-----|-------|
|score|Minimum allowed weight of photon|int|0|
|version|YAPC algorithm version|0:read from ATL03 granule, 1-3:algorithm version (not supported by `atl03x`)|0|
|knn|k-nearest neighbors (version 2 only)|int|0|
|min_knn|minimum number of k-nearest neighbors (version 3 only)|int|5|
|win_h|window height (overrides calculated value if non-zero)|double|6.0|
|win_x|window width|double|15.0|

#### PhoREAL

|Parameter|Description|Units|Default|
|---------|-----------|-----|-------|
|binsize|size of photon height bin|double; meters|1.0|
|geoloc|how geolocation statistics are calculated|0:mean, 1:median, 2:center|median|

#### ATL13

|Parameter|Description|Units|Default|
|---------|-----------|-----|-------|
|refid|ATL13 reference id|integer||
|name|lake (or body of water) name|string||
|coord|latitude and longitude coordinates contained within the desired body of water|object {"lat": $lat, "lon": $lon}||
|anc_fields|ATL13 ancillary fields|list of strings||

#### ATL24

|Parameter|Description|Units|Default|
|---------|-----------|-----|-------|
|compact|reduces number of fields to minimal viable set|boolean|true|
|class_ph|ATL24 classification filter|list; 0:unclassified, 40:bathymetry, 41:sea surface|bathymetry|
|confidence_threshold|minimal bathymetry confidence score|double; 0 to 1.0|0|
|invalid_kd|invalid kd flag values to allow|"on": includes only photons with invalid kd; "off": includes only photons without invalid kd; defaults to both when not specified|all|
|invalid_wind_speed|invalid wind speed flag values to allow|"on": includes only photons with invalid wind speed; "off": includes only photons without invalid wind speed; defaults to both when not specified|all|
|low_confidence|low confidence flag values to allow|"on": includes only low confidence photons; "off": includes only high confidence photons; defaults to both when not specified|all|
|night|night flag values to allow|"on": includes only photons collected at night; "off": includes only photons collected during the day; defaults to both when not specified|all|
|sensor_depth_exceeded|sensor depth exceeded flag values to allow|"on": includes only photons at a depth greater than the sensor depth; "off": includes only photons at a depth less then the sensor depth; defaults to both when not specified|all|
|anc_fields|ATL24 ancillary fields|list of strings||

#### Granule

|Parameter|Description|Units|Default|
|---------|-----------|-----|-------|
|rgt|Reference ground track|int|all|
|cycle|Orbit cycle|int|all|
|region|ATL03 region|int; 1-14|all|
|version|ATL03 release version|latest supported release|
