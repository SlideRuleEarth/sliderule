## Install GDAL Library

For more information on the GDAL library: http://gdal.org

For Ubuntu 20.04
```bash
$ sudo apt install libgdal-dev libuuid-dev libtiff-dev \
```

## Development Notes 

### Managing libtiff Dependencies for GDAL

#### Dependencies:
- Both PROJ and libgeotiff depend on libtiff, which is typically installed by default.

#### Build Process:
1. Consistent libtiff Version:
   - Ensure that PROJ and libgeotiff are built using the system-installed version of libtiff.
   - Verify the libtiff version before proceeding with GDAL compilation to ensure all libraries are
     using the same version.

2. GDAL Build:
   - GDAL should automatically use the system version of libtiff if it is present. However, it sometimes
     defaults to using an older, internal version of libtiff.
   - This discrepancy can cause GDAL to pass all build tests but fail sliderule raster tests.

#### Troubleshooting:
- Version Conflicts:
  - If GDAL is linked against multiple versions of libtiff (check with `ldd` on the GDAL binaries), this
    can lead to runtime errors and failed tests without explicit errors during the build.

- System Cleanup:
  - Clean up the development environment to remove any residual or conflicting libtiff installations.
  - Rebuild PROJ, libgeotiff, and GDAL ensuring that only one version of libtiff is accessible and used
    during the build process.