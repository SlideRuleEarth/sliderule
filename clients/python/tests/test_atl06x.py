"""Tests for atl06x"""

import numpy as np
import geopandas as gpd
from pathlib import Path
from sliderule import sliderule, icesat2, earthdata

TESTDIR = Path(__file__).parent
GRANULE = "ATL06_20200303180710_10390603_007_01.h5"

class TestAtl06x:
    def test_nominal(self, init):
        parms = {
            "cnf": 4,
            "srt": icesat2.SRT_LAND_ICE,
            "beams": "gt1l",
        }
        gdf = sliderule.run("atl06x", parms, resources=[GRANULE])
        assert init
        assert len(gdf) == 114336
        assert len(gdf.keys()) >= 20

        row = gdf.iloc[15]  # First row with all valid fields
        time_ns = row.name.value  # datetime index → ns since epoch
        lat_val = row.geometry.y
        lon_val = row.geometry.x
        np.testing.assert_allclose(
            [
                time_ns,
                lat_val,
                lon_val,
                row["x_atc"],
                row["y_atc"],
                row["h_li"],
                row["h_li_sigma"],
                row["sigma_geo_h"],
                row["atl06_quality_summary"],
                row["segment_id"],
                row["seg_azimuth"],
                row["dh_fit_dx"],
                row["h_robust_sprd"],
                row["w_surface_window_final"],
                row["bsnow_conf"],
                row["bsnow_h"],
                row["r_eff"],
                row["tide_ocean"],
                row["n_fit_photons"],
            ],
            [
                1583258831569810944,
                59.529772793835,
                -44.325544216762,
                6633831.0618938,
                3260.5561523438,
                40.553344726562,
                0.41705507040024,
                0.28224530816078,
                0,
                330916,
                -5.8324255943298,
                -0.047380782663822,
                0.50711327791214,
                3.0426795482635,
                127,
                29.979246139526,
                0.10466815531254,
                -0.25118598341942,
                14,
            ],
            rtol=0,
            atol=1e-5,
        )

        assert gdf["spot"].unique().tolist() == [6]
        assert gdf["cycle"].unique().tolist() == [6]
        assert gdf["region"].unique().tolist() == [3]
        assert gdf["rgt"].unique().tolist() == [1039]
        assert gdf["gt"].unique().tolist() == [10]


    def test_ancillary(self, init):
        parms = {
            "cnf": 4,
            "srt": icesat2.SRT_LAND_ICE,
            "atl06_fields": ["dem/dem_flag", "dem/dem_h"],
            "beams": "gt2r"
        }
        gdf = sliderule.run("atl06x", parms, resources=[GRANULE])
        assert init
        assert len(gdf) == 114605
        assert len(gdf.keys()) >= 22

        row = gdf.iloc[0]
        np.testing.assert_allclose(
            [
                row["dem/dem_flag"],
                row["dem/dem_h"],
            ],
            [
                3,
                42.131591796875,
            ],
            rtol=0,
            atol=1e-6,
        )

    def test_atl06_reader_vs_dataframe(self, init):
        parms = {
            "cnf": 4,
            "srt": icesat2.SRT_LAND_ICE,
            "beams": "gt1l",
        }

        # Run both implementations on the same granule
        gdf_x = sliderule.run("atl06x", parms.copy(), resources=[GRANULE])
        gdf_s = icesat2.atl06sp(parms.copy(), resources=[GRANULE], keep_id=True)

        assert init
        assert len(gdf_x) == len(gdf_s)

        def normalize(gdf):
            # The two APIs return identical rows but with different layouts: atl06x comes back as a GeoDataFrame
            # with time as the index, while atl06s arrives as plain records that get flattened into a DataFrame,
            # so row order can differ. Sorting on extent_id and preserving the original time index lets us compare
            # the same extents deterministically regardless of stream ordering or column/index naming differences.
            sorted_gdf = gdf.sort_values("extent_id")
            time_ns = sorted_gdf.index.astype("int64")
            df = sorted_gdf.reset_index(drop=True)
            df["time_ns"] = time_ns
            df["longitude"] = df.geometry.x.to_numpy()
            df["latitude"] = df.geometry.y.to_numpy()
            return df

        df_x = normalize(gdf_x)
        df_s = normalize(gdf_s)

        int_fields = [
            "extent_id",
            "segment_id",
            "atl06_quality_summary",
            "bsnow_conf",
            "n_fit_photons",
            "spot",
            "gt",
            "cycle",
            "rgt",
        ]
        for field in int_fields:
            np.testing.assert_array_equal(df_x[field].to_numpy(), df_s[field].to_numpy())

        np.testing.assert_array_equal(df_x["time_ns"].to_numpy(), df_s["time_ns"].to_numpy())

        float_fields = [
            "x_atc",
            "y_atc",
            "h_li",
            "h_li_sigma",
            "sigma_geo_h",
            "seg_azimuth",
            "dh_fit_dx",
            "h_robust_sprd",
            "w_surface_window_final",
            "bsnow_h",
            "r_eff",
            "tide_ocean",
            "latitude",
            "longitude",
        ]
        np.testing.assert_allclose(
            df_x[float_fields].to_numpy(),
            df_s[float_fields].to_numpy(),
            rtol=0,
            atol=1e-9,
            equal_nan=True,
        )

    def test_stl_poly_granules(self, init):
        poly = [
            {"lon": -90.15177435697, "lat": 38.59406753389},
            {"lon": -90.15177435697, "lat": 38.631128372239},
            {"lon": -90.21794107665, "lat": 38.631128372239},
            {"lon": -90.21794107665, "lat": 38.59406753389},
            {"lon": -90.15177435697, "lat": 38.59406753389},
        ]
        granules = earthdata.search({"asset": "icesat2-atl06", "poly": poly})
        assert init
        assert len(granules) >= 48

    def test_stl_poly_output_parquet(self, init, tmp_path):
        poly = [
            {"lon": -90.15177435697, "lat": 38.59406753389},
            {"lon": -90.15177435697, "lat": 38.631128372239},
            {"lon": -90.21794107665, "lat": 38.631128372239},
            {"lon": -90.21794107665, "lat": 38.59406753389},
            {"lon": -90.15177435697, "lat": 38.59406753389},
        ]
        output_path = tmp_path / "atl06x_stl.parquet"
        parms = {
            "poly": poly,
            "t0": "2021-01-01",
            "t1": "2021-08-31",
            "output": {
                "format": "parquet",
                "as_geo": True,
                "path": str(output_path),
                "with_checksum": False,
            },
        }

        output_file = sliderule.run("atl06x", parms)
        assert init
        assert output_file == str(output_path)
        gdf = gpd.read_parquet(output_file)
        assert "geometry" in gdf.columns
        assert len(gdf) == 366
        minx, miny, maxx, maxy = gdf.total_bounds.tolist()
        assert minx >= -90.18908049319768
        assert maxx <= -90.18347588914258
        assert miny >= 38.594123857961385
        assert maxy <= 38.63107307741707
