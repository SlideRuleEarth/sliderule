# Release v3.2.0

2023-05-08

Version description of the v3.2.0 release of ICESat-2 SlideRule.

## Major Changes

- GEDI APIs support raster sampling (`gedi04a`, `gedi02a`, `gedi01b`)

## Development Updates

- The `use_poi_time` parameter was added to the raster sampling requests; when set to true, the only raster sampled is the one closest in time to the icesat2 or gedi data point.  This is different than the closest_time parameter which is a single fixed time that is provided in the request.  For the use_poi_time, the time for each row in the DataFrame is used to select the raster closest in time to that row.

- There were significant optimizations in retrieving rasters that intersect a point of interest

- GEDI beam selection can be provided as a list

- GEDU beam sensitivity returned as part of the GeoDataFrame (for `gedi04a` and `gedi02a`)

- Raster sampling documentation updated with more details

## Issues Resolved

- GEDI L01B waveforms fixed (they were being corrupted later in the GeoDataFrame)

## Getting This Release

[https://github.com/ICESat2-SlideRule/sliderule/releases/tag/v3.2.0](https://github.com/ICESat2-SlideRule/sliderule/releases/tag/v3.2.0)

[https://github.com/ICESat2-SlideRule/sliderule-icesat2/releases/tag/v3.2.0](https://github.com/ICESat2-SlideRule/sliderule-icesat2/releases/tag/v3.2.0)

[https://github.com/ICESat2-SlideRule/sliderule-python/releases/tag/v3.2.0](https://github.com/ICESat2-SlideRule/sliderule-python/releases/tag/v3.2.0)

