# Release v4.3.2

2024-04-04

Version description of the v4.3.2 release of SlideRule Earth.

## New Features


## Issues Resolved


## Development Updates


## Getting This Release

[https://github.com/ICESat2-SlideRule/sliderule/releases/tag/v4.3.2](https://github.com/ICESat2-SlideRule/sliderule/releases/tag/v4.3.2)

## Benchmarks

```
atl06_aoi
        output:              625445 x 16 elements
        total:               26.511650 secs
        gdf2poly:            0.000324 secs
        toregion:            0.094860 secs
        __parse_native:      21.172510 secs
        todataframe:         0.391770 secs
        merge:               0.000002 secs
        flatten:             2.783902 secs
        atl06p:              26.266139 secs
atl06_ancillary
        output:              1180 x 17 elements
        total:               3.049564 secs
        gdf2poly:            0.000324 secs
        toregion:            0.094860 secs
        __parse_native:      0.690858 secs
        todataframe:         0.005812 secs
        merge:               0.013563 secs
        flatten:             0.030884 secs
        atl06p:              2.857920 secs
atl03_ancillary
        output:              1180 x 17 elements
        total:               3.047120 secs
        gdf2poly:            0.000324 secs
        toregion:            0.094860 secs
        __parse_native:      0.158048 secs
        todataframe:         0.005754 secs
        merge:               0.013669 secs
        flatten:             0.031304 secs
        atl06p:              3.045915 secs
atl06_parquet
        output:              1577 x 17 elements
        total:               2.755643 secs
        gdf2poly:            0.000324 secs
        toregion:            0.094860 secs
        __parse_native:      0.209622 secs
        todataframe:         0.005754 secs
        merge:               0.013669 secs
        flatten:             0.044833 secs
        atl06p:              2.755176 secs
atl03_parquet
        output:              22833 x 22 elements
        total:               1.633988 secs
        gdf2poly:            0.000324 secs
        toregion:            0.094860 secs
        __parse_native:      0.011271 secs
        todataframe:         0.005754 secs
        merge:               0.013669 secs
        flatten:             0.044833 secs
        atl06p:              2.755176 secs
        atl03sp:             1.607882 secs
atl06_sample_landsat
        output:              914 x 20 elements
        total:               7.073046 secs
        gdf2poly:            0.000324 secs
        toregion:            0.094860 secs
        __parse_native:      0.370676 secs
        todataframe:         0.006028 secs
        merge:               0.012087 secs
        flatten:             0.031790 secs
        atl06p:              4.672313 secs
        atl03sp:             1.607882 secs
atl06_sample_zonal_arcticdem
        output:              1651 x 27 elements
        total:               6.384218 secs
        gdf2poly:            0.000320 secs
        toregion:            0.006032 secs
        __parse_native:      0.319357 secs
        todataframe:         0.005968 secs
        merge:               0.020238 secs
        flatten:             0.045042 secs
        atl06p:              6.375683 secs
        atl03sp:             1.607882 secs
atl06_sample_nn_arcticdem
        output:              1651 x 20 elements
        total:               4.758471 secs
        gdf2poly:            0.000278 secs
        toregion:            0.005407 secs
        __parse_native:      0.091373 secs
        todataframe:         0.005742 secs
        merge:               0.018872 secs
        flatten:             0.038760 secs
        atl06p:              4.751282 secs
        atl03sp:             1.607882 secs
atl06_msample_nn_arcticdem
        output:              1596050 x 20 elements
        total:               143.910649 secs
        gdf2poly:            0.000268 secs
        toregion:            0.005141 secs
        __parse_native:      89.897940 secs
        todataframe:         0.975277 secs
        merge:               15.259323 secs
        flatten:             29.145164 secs
        atl06p:              142.751770 secs
        atl03sp:             1.607882 secs
atl06_no_sample_arcticdem
        output:              1596050 x 16 elements
        total:               69.063557 secs
        gdf2poly:            0.000306 secs
        toregion:            0.006468 secs
        __parse_native:      53.275093 secs
        todataframe:         0.964930 secs
        merge:               0.000003 secs
        flatten:             7.419176 secs
        atl06p:              67.920378 secs
        atl03sp:             1.607882 secs
atl03_rasterized_subset
        output:              51968 x 22 elements
        total:               4.176219 secs
        gdf2poly:            0.000306 secs
        toregion:            0.006468 secs
        __parse_native:      1.858839 secs
        todataframe:         0.042089 secs
        merge:               0.000003 secs
        flatten:             0.371834 secs
        atl06p:              67.920378 secs
        atl03sp:             3.623235 secs
atl03_polygon_subset
        output:              50799 x 22 elements
        total:               3.140495 secs
        gdf2poly:            0.000306 secs
        toregion:            0.006468 secs
        __parse_native:      1.370758 secs
        todataframe:         0.041492 secs
        merge:               0.000003 secs
        flatten:             0.359666 secs
        atl06p:              67.920378 secs
        atl03sp:             3.114903 secs
```