# Including Ancillary Fields

2022-12-06


## Overview

This tutorial walks you through the steps necessary to include ancillary fields in the data returned for `atl03sp` and `atl06p` requests.  Ancillary fields are fields present in the ATL03 ICESat-2 Standard Product, but are not included in the base results returned by SlideRule.

**Prerequisites**: This walk-through assumes you have already installed the SlideRule Python client and familiar with how to use it. See the [installation](../../getting_started/Install.html) instructions in the reference documentation if you need help installing the SlideRule Python client.  See the [Making Your First Request](./first_request.html) tutorial if you've never made a SlideRule processing request before.


## Background

The ATL03 granules include data associated with the photons in different subgroups inside the HDF5 file.  SlideRule currently supports including ancillary fields from three subgroups inside those granules:
* gtxx/geolocation
* gtxx/geophys_corr
* gtxx/heights

Support for additional subgroups as well as fields present in other data products is forthcoming but unavailable at this time.

When an `atl03sp` or `at06p` processing request specifies ancillary fields, SlideRule reads those fields from the ATL03 granules, subsets them to the region of interest, and correlates them to the dynamically generated photon segment (called and "extent" in the code) they belong to.  The result is a GeoDataFrame with a column for each ancillary field populated by the value associated with each photon for `atl03sp` requests, and elevation for `atl06p` requests.

Note, including ancillary fields in a processing request will increase the amount of time it takes for the request to be processed, and also the amount of data returned, so it should only be used when the fields are needed by the end user.


## Including an Ancillary Field in an `atl06p` request

The __"atl03_geo_fields"__ parameter is used to request ancillary fields be included in `atl06p` responses.  These fields must come from either the __"gtxx/geolocation"__ or __"gtxx/geophys_corr"__ subgroups.

__Step 1__: Import and initialize the SlideRule Python package for ICESat-2.
```python
>>> from sliderule import sliderule, icesat2
>>> icesat2.init("slideruleearth.io", verbose=True)
```

__Step 2__: Create parameters for a typical `atl06p` processing request.
```python
>>> grand_mesa = sliderule.toregion('grandmesa.geojson')
>>> parms = {
    "poly": grand_mesa["poly"],
    "srt": icesat2.SRT_LAND,
    "cnf": icesat2.CNF_SURFACE_HIGH,
    "len": 40.0,
    "res": 20.0,
    "maxi": 1
}
```
The **grandmesa.geojson** file used in this example can be downloaded by navigating to our [downloads](/rtd/tutorials/downloads.html) page; alternatively, you can create your own GeoJSON file at [geojson.io](https://geojson.io).

__Step 3__: Add ancillary fields to the request.
```python

>>> parms["atl03_geo_fields"] = ["ref_azimuth", "ref_elev"]
```

__Step 4__: Issue the processing request to SlideRule.
```python
>>> gdf = icesat2.atl06p(parms)
```
When this completes (~45 seconds), the _gdf_ variable should now contain all of the results of the elevations calculated by SlideRule with corresponding columns for the "ref_azimuth" and "ref_elev" fields.

__Step 5__: Display a summary of the results.
```python
>>> print(gdf)
                               segment_id  w_surface_window_final  dh_fit_dy   rgt       h_mean  cycle  dh_fit_dx  ...  spot  rms_misfit  gt   h_sigma                     geometry  ref_azimuth  ref_elev
time                                                                                                               ...
2018-10-17 22:31:17.350047904      215745                3.000000        0.0   295  1826.151552      1   0.019818  ...     1    0.287631  60  0.098879  POINT (-108.28629 38.88959)    -1.608736  1.558924
2018-10-17 22:31:17.352875032      215746                3.000000        0.0   295  1826.569174      1   0.021436  ...     1    0.244501  60  0.028634  POINT (-108.28631 38.88977)    -1.608745  1.558925
2018-10-17 22:31:17.355689608      215747                3.000000        0.0   295  1827.168388      1   0.034429  ...     1    0.235445  60  0.026206  POINT (-108.28633 38.88995)    -1.608754  1.558924
2018-10-17 22:31:17.358488680      215748                3.000000        0.0   295  1827.804630      1   0.027981  ...     1    0.223318  60  0.026843  POINT (-108.28636 38.89013)    -1.608765  1.558924
2018-10-17 22:31:17.361279056      215749                3.000000        0.0   295  1827.841449      1  -0.013322  ...     1    0.243411  60  0.032435  POINT (-108.28638 38.89031)    -1.608776  1.558925
...                                   ...                     ...        ...   ...          ...    ...        ...  ...   ...         ...  ..       ...                          ...          ...       ...
2022-09-07 02:41:09.988086880      217426                5.096439        0.0  1179  2207.926466     16  -0.024542  ...     3    0.955833  40  0.130097  POINT (-107.93220 39.18154)     1.723527  1.557852
2022-09-07 02:41:09.989714816      217421               15.425887        0.0  1179  2129.502952     16   0.009586  ...     5    2.088458  20  0.229026  POINT (-107.96871 39.17788)     1.622407  1.550931
2022-09-07 02:41:09.990888256      217427               12.242074        0.0  1179  2206.743063     16  -0.100393  ...     3    2.971743  40  0.398425  POINT (-107.93223 39.18172)     1.723528  1.557852
2022-09-07 02:41:09.992528448      217422               13.004639        0.0  1179  2129.035904     16  -0.031298  ...     5    1.871471  20  0.212911  POINT (-107.96874 39.17806)     1.622411  1.550931
2022-09-07 02:41:09.995344544      217423                3.617743        0.0  1179  2127.671963     16  -0.086885  ...     5    1.343889  20  0.140299  POINT (-107.96876 39.17824)     1.622411  1.550931

[250448 rows x 18 columns]
```
For a full description of all of the fields returned from the `atl06p` function, see the [elevations](../../user_guide/ICESat-2.html#elevations) documentation.


## Including an Ancillary Field in an `atl03sp` request

The __"atl03_ph_fields"__ parameter can be used to request ancillary fields be included in `atl03sp` responses.  These fields must come from the __"gtxx/heights"__ subgroup. The __"atl03_geo_fields"__ parameter can also be used - but note that when it is used, the resulting data will expand so that each photon row in the GeoDataFrame will have the value of the ancillary field corresponding to the segment that the photon is in.

__Step 1__: Import and initialize the SlideRule Python package for ICESat-2.
```python
>>> from sliderule import sliderule, icesat2
>>> icesat2.init("slideruleearth.io", verbose=True)
```

__Step 2__: Create parameters for a typical `atl06p` processing request.
```python
>>> grand_mesa = sliderule.toregion('grandmesa.geojson')
>>> parms = {
    "poly": grand_mesa["poly"],
    "srt": icesat2.SRT_LAND,
    "cnf": icesat2.CNF_SURFACE_HIGH,
    "len": 40.0,
    "res": 20.0,
    "maxi": 1
}
```
The **grandmesa.geojson** file used in this example can be downloaded by navigating to our [downloads](/rtd/tutorials/downloads.html) page; alternatively, you can create your own GeoJSON file at [geojson.io](https://geojson.io).

__Step 3__: Add ancillary fields to the request.
```python

>>> parms["atl03_geo_fields"] = ["ref_azimuth", "ref_elev"]
>>> parms["atl03_ph_fields"] = ["ph_id_channel"]
```

__Step 4__: Issue the processing request to SlideRule.
```python
>>> gdf = icesat2.atl03sp(parms, resources=['ATL03_20181017222812_02950102_005_01.h5'])
```
When this completes (~45 seconds), the _gdf_ variable should now contain all of the results of the photons read by SlideRule with corresponding columns for the "ph_id_channel", "ref_azimuth". and "ref_elev" fields.

__Step 5__: Display a summary of the results.
```python
>>> print(gdf)
                               segment_dist  cycle  segment_id  sc_orient  rgt  track    delta_time  quality_ph  ...   distance       height  ref_azimuth  ref_elev  ph_id_channel  pair                     geometry  spot
time                                                                                                             ...
2018-10-17 22:31:17.349347396  4.326639e+06      1      215745          1  295      3  2.505068e+07           0  ...  -4.955579  1825.912354    -1.608736  1.558924             72     1  POINT (-108.28629 38.88954)     1
2018-10-17 22:31:17.350147408  4.326659e+06      1      215746          1  295      3  2.505068e+07           0  ... -19.340409  1825.895508    -1.608745  1.558925             67     1  POINT (-108.28629 38.88959)     1
2018-10-17 22:31:17.350147408  4.326659e+06      1      215746          1  295      3  2.505068e+07           0  ... -19.340409  1825.972046    -1.608745  1.558925             71     1  POINT (-108.28629 38.88959)     1
2018-10-17 22:31:17.350147408  4.326639e+06      1      215745          1  295      3  2.505068e+07           0  ...   0.705489  1825.972046    -1.608736  1.558924             71     1  POINT (-108.28629 38.88959)     1
2018-10-17 22:31:17.350147408  4.326639e+06      1      215745          1  295      3  2.505068e+07           0  ...   0.705489  1825.895508    -1.608736  1.558924             67     1  POINT (-108.28629 38.88959)     1
...                                     ...    ...         ...        ...  ...    ...           ...         ...  ...        ...          ...          ...       ...            ...   ...                          ...   ...
2018-10-17 22:31:19.581347468  4.342496e+06      1      216536          1  295      3  2.505068e+07           0  ...  11.597862  1798.604248    -1.608565  1.558929             15     1  POINT (-108.30368 39.03187)     1
2018-10-17 22:31:19.581447440  4.342496e+06      1      216536          1  295      3  2.505068e+07           0  ...  12.314271  1798.765015    -1.608565  1.558929              5     1  POINT (-108.30368 39.03187)     1
2018-10-17 22:31:19.581747440  4.342496e+06      1      216536          1  295      3  2.505068e+07           0  ...  14.462840  1798.790894    -1.608565  1.558929             67     1  POINT (-108.30368 39.03189)     1
2018-10-17 22:31:19.581847440  4.342496e+06      1      216536          1  295      3  2.505068e+07           0  ...  15.177260  1798.554688    -1.608565  1.558929             71     1  POINT (-108.30368 39.03190)     1
2018-10-17 22:31:19.581947444  4.342496e+06      1      216536          1  295      3  2.505068e+07           0  ...  15.894337  1798.709229    -1.608565  1.558929              7     1  POINT (-108.30368 39.03190)     1

[20060 rows x 19 columns]
```
For a full description of all of the fields returned from the `atl03sp` function, see the [photons](../../user_guide/ICESat-2.html#segmented-photon-data) documentation.


## Next Steps

For more information on the ancillary field parameters, see the [User's Guide](../../user_guide/ICESat-2.html#ancillary-field-parameters).
