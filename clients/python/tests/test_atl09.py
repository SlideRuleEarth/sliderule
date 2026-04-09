"""Tests for atl09 sampler"""

from sliderule import sliderule, h5, io


region = [
    {"lon":-105.82971551223244, "lat": 39.81983728534918},
    {"lon":-105.30742121965137, "lat": 39.81983728534918},
    {"lon":-105.30742121965137, "lat": 40.164048017973755},
    {"lon":-105.82971551223244, "lat": 40.164048017973755},
    {"lon":-105.82971551223244, "lat": 39.81983728534918}
]

class TestAtl09:
    def test_correctness(self, init):
        assert init

        # make atl06x atmospheric sampler request
        parms = {
            "poly": region,
            "rgt": 554,
            "cycle": 8,
            "region": 2,
            "beams": ["gt2l"],
            "atl09_fields": [
                "bckgrd_atlas/bckgrd_counts",
                "high_rate/dem_h",
                "low_rate/met_t10m"
            ]
        }
        gdf = sliderule.run("atl06x", parms)
        assert gdf.attrs['meta']['srctbl']['0'] == 'ATL06_20200731150749_05540802_007_01.h5'

        # make direct h5x request for same data
        bckgrd_atlas_df = h5.h5x(["delta_time", "bckgrd_counts"], "ATL09_20200731150047_05540801_007_01.h5",  "icesat2-atl09", ["/profile_2/bckgrd_atlas"])
        high_rate_df = h5.h5x(["segment_id", "dem_h"], "ATL09_20200731150047_05540801_007_01.h5",  "icesat2-atl09", ["/profile_2/high_rate"])
        low_rate_df = h5.h5x(["segment_id", "met_t10m"], "ATL09_20200731150047_05540801_007_01.h5",  "icesat2-atl09", ["/profile_2/low_rate"])

        # helper function for time comparison
        def compare_time(gdf, field, df, col):
            for i in range(len(gdf)):
                t = io.convert_datetime(gdf.index[i])
                mask = df["delta_time"] <= t
                row = df[mask].iloc[-1] if mask.any() else None
                assert row is not None, f"No delta_time less than {t} found in df"
                assert gdf[field].iloc[i] == row[col], f"miscompare on {field}[{i}] = {gdf[field].iloc[i]} to {col} = {row[col]}"

        # helper function for segment comparison
        def compare_segment(gdf, field, df, col):
            for i in range(len(gdf)):
                seg = gdf["segment_id"].iloc[i]
                mask = df["segment_id"] <= seg
                row = df[mask].iloc[-1] if mask.any() else None
                assert row is not None, f"No segment_id less than {seg} found in df"
                assert gdf[field].iloc[i] == row[col], f"miscompare on {field}[{i}] = {gdf[field].iloc[i]} to {col} = {row[col]}"

        # compare results
        compare_time(gdf, "bckgrd_atlas/bckgrd_counts", bckgrd_atlas_df, "bckgrd_counts")
        compare_segment(gdf, "high_rate/dem_h", high_rate_df, "dem_h")
        compare_segment(gdf, "low_rate/met_t10m", low_rate_df, "met_t10m")

    def test_atl03x(self, init):
        parms = {
            "poly": region,
            "rgt": 554,
            "cycle": 8,
            "region": 2,
            "beams": ["gt2l"],
            "atl09_fields": [
                "bckgrd_atlas/bckgrd_counts",
                "high_rate/dem_h",
                "low_rate/met_t10m"
            ]
        }
        gdf = sliderule.run("atl03x", parms)
        assert init
        assert gdf.attrs['meta']['srctbl']['0'] == 'ATL03_20200731150749_05540802_007_01.h5'
        assert len(gdf["bckgrd_atlas/bckgrd_counts"]) == 42267
        assert len(gdf["high_rate/dem_h"]) == 42267
        assert len(gdf["low_rate/met_t10m"]) == 42267
        assert len(gdf) == 42267

    def test_atl06x(self, init):
        parms = {
            "poly": region,
            "rgt": 554,
            "cycle": 8,
            "region": 2,
            "beams": ["gt2l"],
            "atl09_fields": [
                "low_rate/met_ps",
                "low_rate/met_t10m"
            ]
        }
        gdf = sliderule.run("atl06x", parms)
        assert init
        assert gdf.attrs['meta']['srctbl']['0'] == 'ATL06_20200731150749_05540802_007_01.h5'
        assert len(gdf["low_rate/met_ps"]) == 1049
        assert len(gdf["low_rate/met_t10m"]) == 1049
        assert len(gdf) == 1049

    def test_atl08x(self, init):
        parms = {
            "poly": region,
            "rgt": 554,
            "cycle": 8,
            "region": 2,
            "beams": ["gt2l"],
            "atl09_fields": [
                "bckgrd_atlas/bckgrd_counts",
                "bckgrd_atlas/bckgrd_hist_top"
            ]
        }
        gdf = sliderule.run("atl08x", parms)
        assert init
        assert gdf.attrs['meta']['srctbl']['0'] == 'ATL08_20200731150749_05540802_007_01.h5'
        assert len(gdf["bckgrd_atlas/bckgrd_counts"]) == 207
        assert len(gdf["bckgrd_atlas/bckgrd_hist_top"]) == 207
        assert len(gdf) == 207

    def test_atl13x(self, init):
        parms = {
            "atl13": {
                "refid": 5952002394
            },
            "atl09_fields": [
                "bckgrd_atlas/bckgrd_counts",
                "bckgrd_atlas/bckgrd_hist_top"
            ]
        }
        gdf = sliderule.run("atl13x", parms)
        assert init
        assert 'ATL13_20200111053122_02370601_006_01.h5' in [gdf.attrs['meta']['srctbl'][k] for k in gdf.attrs['meta']['srctbl']]
        assert len(gdf["bckgrd_atlas/bckgrd_counts"]) == 5032
        assert len(gdf["bckgrd_atlas/bckgrd_hist_top"]) == 5032
        assert len(gdf) == 5032

    def test_atl03x_surface(self, init):
        parms = {
            "poly": region,
            "rgt": 554,
            "cycle": 8,
            "region": 2,
            "beams": ["gt2l"],
            "fit": {},
            "atl09_fields": [
                "bckgrd_atlas/bckgrd_counts",
                "high_rate/dem_h",
                "low_rate/met_t10m"
            ]
        }
        gdf = sliderule.run("atl03x", parms)
        assert init
        assert gdf.attrs['meta']['srctbl']['0'] == 'ATL03_20200731150749_05540802_007_01.h5'
        assert len(gdf["bckgrd_atlas/bckgrd_counts"]) == 1013
        assert len(gdf["high_rate/dem_h"]) == 1013
        assert len(gdf["low_rate/met_t10m"]) == 1013
        assert len(gdf) == 1013

    def test_atl03x_phoreal(self, init):
        parms = {
            "poly": region,
            "rgt": 554,
            "cycle": 8,
            "region": 2,
            "beams": ["gt2l"],
            "phoreal": {},
            "atl08_fields": ["sigma_topo"],
            "atl09_fields": [
                "bckgrd_atlas/bckgrd_counts",
                "high_rate/dem_h",
                "low_rate/met_t10m"
            ]
        }
        gdf = sliderule.run("atl03x", parms)
        assert init
        assert gdf.attrs['meta']['srctbl']['0'] == 'ATL03_20200731150749_05540802_007_01.h5'
        assert len(gdf["bckgrd_atlas/bckgrd_counts"]) == 996
        assert len(gdf["high_rate/dem_h"]) == 996
        assert len(gdf["low_rate/met_t10m"]) == 996
        assert len(gdf) == 996
