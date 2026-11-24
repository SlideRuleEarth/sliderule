"""Tests for sliderule user URL raster support against public AWS-hosted data."""

import pytest
import sliderule
from sliderule import raster

vrtLon = -108.0
vrtLat = 39.0

# VRT file hosted on public AWS S3 bucket, with a single band containing elevation data.
vrtUrl = "https://prd-tnm.s3.amazonaws.com/StagedProducts/Elevation/13/TIFF/USGS_Seamless_DEM_13.vrt"
vrtFile = "/vsicurl/" + vrtUrl
vrtValue = 2978.8788307449


class TestUserUrlPublicAws:
    def test_samples(self, init):
        rqst = {
            "samples": {
                "asset": "user-url-raster",
                "url": vrtUrl,
                "elevation_bands": ["1"]
            },
            "coordinates": [[vrtLon, vrtLat]]
        }
        rsps = sliderule.source("samples", rqst)

        assert init
        assert rsps["errors"][0] == 0
        assert len(rsps["samples"][0]) == 1

        sample = rsps["samples"][0][0]
        assert sample["value"] == pytest.approx(vrtValue)
        assert sample["file"] == vrtFile

    def test_raster_sample_api(self, init):
        gdf = raster.sample(
            "user-url-raster",
            [[vrtLon, vrtLat]],
            parms={"url": vrtUrl, "elevation_bands": ["1"]}
        )

        assert init
        assert len(gdf) == 1
        assert gdf["value"].iat[0] == pytest.approx(vrtValue)
        assert gdf["file"].iat[0] == vrtFile
