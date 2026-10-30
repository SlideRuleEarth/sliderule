# sliderule-atl24

ICESat-2 Near-Shore Bathymetry Standard Data Product

## Prerequisites

* `https://gitlab.com/3dgl/atl24_v2_algorithms`: the ATL24 algorithm repository must be checked out at the same directory level as the `sliderule` repository.
* `ATL24_Mask_v5_Raster.tif`: the ATL24 search mask must reside in the `/data/` directory.
* `cop_rep_ANNUAL_meanRI_d00.tif`: the ATL24 refraction index must reside in the `/data/` directory.

## Dependencies

* cmake >= 3.13.0
* C++23
* XGBoost
* LibArchive
* UUID Library (libuuid)
* Readline
* Lua >= 5.3
* OpenSSL
* LibTIFF
* zlib
* Zstandard (zstd)
* cURL

## Build Instructions

1. Build a local release of the SlideRule node application.

```bash
cd sliderule/targets/slideruleearth
make distclean
make config-release
make
```

2. Build and install a local release of the ATL24 plugin.

```bash
cd sliderule/targets/atl24
make config-state-release
make
```