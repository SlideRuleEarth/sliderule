"""Tests for sliderule user URL raster support."""

import sliderule
import pytest
from sliderule import raster

sigma = 1.0e-9

vrtLon = -108.1
vrtLat = 39.1

vrtValueRaw         = 2653.0119772144   # The raw value from the raster
vrtValueWithZOffset = 2637.891093719017 # The value with the Z offset applied via proj transform in the VRT
vrtUrl = "https://opentopography.s3.sdsc.edu/raster/COP30/COP30_hh.vrt"
vrtFile = "/vsicurl/" + vrtUrl

targetCRS = "EPSG:9055+3855"

pipeline = "+proj=pipeline \
            +step +proj=axisswap +order=2,1 \
            +step +proj=unitconvert +xy_in=deg +xy_out=rad \
            +step +proj=cart +ellps=GRS80 \
            +step +inv +proj=helmert +x=-0.0007 +y=-0.0012 +z=0.0261 +rx=0 +ry=0 +rz=0 \
                    +s=-0.00212 +dx=-0.0001 +dy=-0.0001 +dz=0.0019 +drx=0 +dry=0 +drz=0 \
                    +ds=-0.00011 +t_epoch=2010 +convention=position_vector \
            +step +inv +proj=cart +ellps=WGS84 \
            +step +inv +proj=vgridshift +grids=us_nga_egm08_25.tif +multiplier=1 \
            +step +proj=unitconvert +xy_in=rad +xy_out=deg \
            +step +proj=axisswap +order=2,1"

class TestUserDemRaster:
    def test_samples(self, init):
        rqst = {
            "samples": {
                "asset": "user-url-raster",
                "url": vrtUrl
            },
            "coordinates": [[vrtLon, vrtLat]]
        }
        rsps = sliderule.source("samples", rqst)
        assert init
        assert abs(rsps["samples"][0][0]["value"] - vrtValueRaw) < sigma
        assert rsps["samples"][0][0]["file"] == vrtFile

    def test_raster_sample_api(self, init):
        gdf = raster.sample("user-url-raster", [[vrtLon, vrtLat]], parms={"url": vrtUrl})
        assert init
        assert len(gdf) == 1
        assert abs(gdf["value"].iat[0] - vrtValueRaw) < sigma
        assert gdf["file"].iat[0] == vrtFile

    def test_with_proj_pipeline(self, init):
        gdf = raster.sample(
            "user-url-raster",
            [[vrtLon, vrtLat]],
            parms={"url": vrtUrl, "proj_pipeline": pipeline}
        )
        assert init
        assert len(gdf) == 1
        assert abs(gdf["value"].iat[0] - vrtValueWithZOffset) < sigma
        assert gdf["file"].iat[0] == vrtFile

    def test_with_target_crs(self, init):
        gdf = raster.sample(
            "user-url-raster",
            [[vrtLon, vrtLat]],
            parms={"url": vrtUrl, "target_crs": targetCRS}
        )

        assert init
        assert len(gdf) == 1
        assert abs(gdf["value"].iat[0] - vrtValueWithZOffset) < sigma
        assert gdf["file"].iat[0] == vrtFile

    def test_with_proj_pipeline_and_target_crs(self, init):
        gdf = raster.sample(
            "user-url-raster",
            [[vrtLon, vrtLat]],
            parms={"url": vrtUrl, "proj_pipeline": pipeline, "target_crs": targetCRS}
        )
        assert init
        assert len(gdf) == 1
        assert abs(gdf["value"].iat[0] - vrtValueWithZOffset) < sigma
        assert gdf["file"].iat[0] == vrtFile

    @pytest.mark.xfail(strict=True, reason="Bad target CRS currently yields empty samples/error response")
    def test_with_bad_target_crs(self, init):
        gdf = raster.sample("user-url-raster", [[vrtLon, vrtLat]], parms={"url": vrtUrl, "target_crs": "EPSG:BADCRS"})

        assert init
        assert rsps["errors"][0] != 0
        assert len(rsps["samples"][0]) == 0
        assert len(gdf) == 0

    @pytest.mark.xfail(strict=True, reason="Bad proj pipeline currently yields empty samples/error response")
    def test_with_bad_proj_pipeline(self, init):
        gdf = raster.sample("user-url-raster", [[vrtLon, vrtLat]], parms={"url": vrtUrl, "proj_pipeline": "bad-pipeline"})

        assert init
        assert rsps["errors"][0] != 0
        assert len(rsps["samples"][0]) == 0
        assert len(gdf) == 0
