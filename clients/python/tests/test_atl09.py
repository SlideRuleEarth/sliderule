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
        high_rate_df = h5.h5x(["delta_time", "dem_h"], "ATL09_20200731150047_05540801_007_01.h5",  "icesat2-atl09", ["/profile_2/high_rate"])
        low_rate_df = h5.h5x(["delta_time", "met_t10m"], "ATL09_20200731150047_05540801_007_01.h5",  "icesat2-atl09", ["/profile_2/low_rate"])

        # helper function
        def compare(gdf, field, df, col):
            for i in range(len(gdf)):
                t = io.convert_datetime(gdf.index[i])
                idx = (df["delta_time"] > t).idxmax()
                row = df.loc[idx] if (df["delta_time"] > t).any() else None
                assert gdf[field].iloc[i] == row[col], f"miscompare on {field}[{i}] = {gdf[field].iloc[i]} to {col}[{idx}] = {row[col]}"

        # compare results
        compare(gdf, "bckgrd_atlas/bckgrd_counts", bckgrd_atlas_df, "bckgrd_counts")
        compare(gdf, "high_rate/dem_h", high_rate_df, "dem_h")
        compare(gdf, "low_rate/met_t10m", low_rate_df, "met_t10m")