# Release v1.0.3

2021-04-16

Version description of the v1.0.3 release of ICESat-2 SlideRule

* The ATL06 response records now include `n_fit_photons`, `rms_misfit`, and `w_surface_window_final` fields.

* There is now a default upper limit on the number of granules that can be processed by a single request

* There is a dedicated ATL03 endpoint `atl03s` that will return segmented photon data within a polygon

* A `version` endpoint was added that returns version information for the running system (things like version number, commit id, packages loaded, launch time)

* The `h5p` endpoint was added to support reading multiple datasets concurrently from a single h5 file

## Getting this release

[https://github.com/ICESat2-SlideRule/sliderule/releases/tag/v1.0.3](https://github.com/ICESat2-SlideRule/sliderule/releases/tag/v1.0.3)


