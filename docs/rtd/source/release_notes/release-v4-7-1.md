# Release v4.7.1

2024-09-30

Version description of the v4.7.1 release of SlideRule Earth.

Bathy Version #4.

## ATL24 Changes

* Fixed default for parameter `thresh` in C-Shelph.
* Fixed calculation of `min_surface_photons_per_window` and `min_bathy_photons_per_window` in OpenOceans++
* Updated models to latest for QTrees, CoastNet, and Ensemble
* Implemented use of `geoid_corr_h` for all classifiers except for the Ensemble.  The Ensemble will be updated once it is retrained on the latest round of test results that incorporate the `geoid_corr_h` field.  Previously, all classifiers were using non-refraction corrected height, but the ensemble was being trained using the refraction corrected height.  Also, any post-test-run validation of the results only had the refraction corrected heights in the output.  So the `geoid_corr_h` field was added so that it can clearly represent non-refraction corrected heights, while `ortho_h` includes the refraction correction (and any future corrections we might add).
* QTrees, CoastNet, and OpenOceans++ have been updated with the latest code and are now compiled in as header only libraries.
* MedianFilter and C-Shelph are pulled directly from the ut-ATL24-medianfilter and ut-ATL24-C-shelph repositories
* Version information is included in for all external classifiers (CoastNet, QTrees, OpenOceans++, MedianFilter, C-Shelph)
* Metadata is now provided inside the Parquet and HDF5 files instead of as separate json files.
* Uncertainty calculation fixed (migration from python to C and the update to a linear model introduced a few regressions)

## Known Issues and Remaining Tasks

* Current ensemble model is trained on refraction corrected heights though the inputs are not corrected
* Pointnet is identifying very low rates of bathymetry with respect to the other classifiers - we are still working through possible causes

## Development Updates

* The ATL03 photons are stored in a table in memory and operated on in place instead of streamed
* The Python classifiers are all executed from a single Python script that uses multiprocessing instead of having the data transferred to and from disk

## Getting This Release

[https://github.com/SlideRuleEarth/sliderule/releases/tag/v4.7.1](https://github.com/SlideRuleEarth/sliderule/releases/tag/v4.7.1)

## ATL24 Request Parameters

## Benchmarks
