#
# Imports
#
import earthaccess
from sliderule import sliderule, s2tile
from h5coro import h5coro, s3driver
import geopandas as gpd
import argparse
import time
import h5py
import sys
import numpy as np
import pandas as pd
from shapely import contains_xy
from shapely.geometry import Polygon

#
# AOIs
#
greenland_poly_50km2 = [
    {"lon": -27.25979597144949,     "lat": 73.49716901328901},
    {"lon": -27.25979597144949,     "lat": 73.56076480273342},
    {"lon": -27.48407173814204,     "lat": 73.56076480273342},
    {"lon": -27.48407173814204,     "lat": 73.49716901328901},
    {"lon": -27.25979597144949,     "lat": 73.49716901328901}
]
greenland_poly_300km2 = [
    {"lon": -27.219435431853217,    "lat": 73.49818164811984},
    {"lon": -27.219435431853217,    "lat": 73.64424276904958},
    {"lon": -27.80812065118551,     "lat": 73.64424276904958},
    {"lon": -27.80812065118551,     "lat": 73.49818164811984},
    {"lon": -27.219435431853217,    "lat": 73.49818164811984}
]
greenland_convex_poly_450km2 = [
    {"lon": -29.127476024254214,    "lat": 73.73869799255206},
    {"lon": -28.57131232009959,     "lat": 73.68944445330746},
    {"lon": -28.398006201999944,    "lat": 73.76409914046496},
    {"lon": -28.436079389819962,    "lat": 73.850847588515},
    {"lon": -28.789962359255153,    "lat": 73.91373599019127},
    {"lon": -29.256954649831314,    "lat": 73.86000697128824},
    {"lon": -29.127476024254214,    "lat": 73.73869799255206}
]
greenland_poly_5000km2 = [
    {"lon": -27.293532869741416,    "lat": 73.48761834293555},
    {"lon": -27.293532869741416,    "lat": 74.0798487786565},
    {"lon": -29.741987353437068,    "lat": 74.0798487786565},
    {"lon": -29.741987353437068,    "lat": 73.48761834293555},
    {"lon": -27.293532869741416,    "lat": 73.48761834293555}
]
aois = [greenland_poly_50km2, greenland_poly_300km2, greenland_convex_poly_450km2, greenland_poly_5000km2]
names = ["Bbox50km2", "Bbox300km2", "Convex450km2", "Bbox5000km2"]
regions = {names[i]: sliderule.toregion(aois[i]) for i in range(len(aois))}

#
# EarthAccess
#
def EarthAccessSubset(name, aoi, start_time=None, end_time=None):
    start = time.time()
    t0, t1 = [None if t is None else pd.Timestamp(t).tz_localize(None) if pd.Timestamp(t).tzinfo is None
              else pd.Timestamp(t).tz_convert("UTC").tz_localize(None) for t in (start_time, end_time)]
    aoi_polygon = Polygon([(p["lon"], p["lat"]) for p in aoi])
    lon_min, lat_min, lon_max, lat_max = aoi_polygon.bounds
    search_kwargs = {"temporal": (start_time, end_time)} if start_time or end_time else {}
    granules = earthaccess.search_data(short_name="ATL06", polygon=[(p["lon"], p["lat"]) for p in aoi], **search_kwargs)
    print(f"Found {len(granules)} ATL06 granules")
    atlas_epoch = np.datetime64("2018-01-01T00:00:00")
    variables = ["h_li", "h_li_sigma", "atl06_quality_summary", "segment_id", "delta_time"]
    frames = []
    print(f"EarthAccess opening {len(granules)} granules")
    for fh in earthaccess.open(granules):
        sys.stdout.write("*")
        sys.stdout.flush()
        try:
            with h5py.File(fh, "r") as h5:
                for gt in ["gt1l", "gt1r", "gt2l", "gt2r", "gt3l", "gt3r"]:
                    sys.stdout.write(".")
                    sys.stdout.flush()
                    if f"{gt}/land_ice_segments" not in h5:
                        continue
                    seg = h5[f"{gt}/land_ice_segments"]
                    lat = seg["latitude"][:]
                    lon = seg["longitude"][:]
                    # cheap bbox prefilter before the exact polygon test
                    idx = np.flatnonzero((lat >= lat_min) & (lat <= lat_max) & (lon >= lon_min) & (lon <= lon_max))
                    idx = idx[contains_xy(aoi_polygon, lon[idx], lat[idx])]
                    if idx.size == 0:
                        continue
                    # read only the contiguous span covering the AOI
                    span = slice(idx[0], idx[-1] + 1)
                    df = pd.DataFrame({v: seg[v][span] for v in variables})
                    df["latitude"] = lat[span]
                    df["longitude"] = lon[span]
                    frames.append(df.iloc[idx - span.start])
        finally:
            fh.close()
    df = pd.concat(frames, ignore_index=True)
    df["time"] = atlas_epoch + (df.pop("delta_time") * 1e9).astype("timedelta64[ns]")
    atl06 = gpd.GeoDataFrame(df, geometry=gpd.points_from_xy(df.longitude, df.latitude), crs="EPSG:4326")
    gdf = atl06.set_index("time").sort_index().loc[t0:t1]
    print(f"EarthAccess subsetted {name} / {len(gdf)} rows in {time.time() - start:.2f} seconds")

#
# H5Coro
#
def H5CoroSubset(name, aoi, start_time=None, end_time=None):
    start = time.time()
    t0, t1 = [None if t is None else pd.Timestamp(t).tz_localize(None) if pd.Timestamp(t).tzinfo is None
            else pd.Timestamp(t).tz_convert("UTC").tz_localize(None) for t in (start_time, end_time)]
    aoi_polygon = Polygon([(p["lon"], p["lat"]) for p in aoi])
    lon_min, lat_min, lon_max, lat_max = aoi_polygon.bounds
    search_kwargs = {"temporal": (start_time, end_time)} if start_time or end_time else {}
    granules = earthaccess.search_data(short_name="ATL06", polygon=[(p["lon"], p["lat"]) for p in aoi], **search_kwargs)
    print(f"Found {len(granules)} ATL06 granules")
    credentials = earthaccess.get_s3_credentials(daac="NSIDC")
    atlas_epoch = np.datetime64("2018-01-01T00:00:00")
    beams = ["gt1l", "gt1r", "gt2l", "gt2r", "gt3l", "gt3r"]
    variables = ["h_li", "h_li_sigma", "atl06_quality_summary", "segment_id", "delta_time"]
    frames = []
    for granule in granules:
        resource = granule.data_links(access="direct")[0].removeprefix("s3://")
        with h5coro.H5Coro(resource, s3driver.S3Driver, credentials=credentials) as h5obj:
            geo = h5obj.readDatasets([f"{gt}/land_ice_segments/{c}" for gt in beams for c in ["latitude", "longitude"]])
            selections = {}
            for gt in beams:
                lat = geo[f"{gt}/land_ice_segments/latitude"]
                lon = geo[f"{gt}/land_ice_segments/longitude"]
                # missing beams come back as None
                if lat is None or lon is None:
                    continue
                # cheap bbox prefilter before the exact polygon test
                idx = np.flatnonzero((lat >= lat_min) & (lat <= lat_max) & (lon >= lon_min) & (lon <= lon_max))
                idx = idx[contains_xy(aoi_polygon, lon[idx], lat[idx])]
                if idx.size == 0:
                    continue
                selections[gt] = (lat, lon, idx, int(idx[0]), int(idx[-1]) + 1)
            if not selections:
                continue
            # read only the contiguous span covering the AOI
            data = h5obj.readDatasets([
                {"dataset": f"{gt}/land_ice_segments/{v}", "hyperslice": [[s0, s1]]}
                for gt, (_, _, _, s0, s1) in selections.items() for v in variables
            ])
            for gt, (lat, lon, idx, s0, s1) in selections.items():
                df = pd.DataFrame({v: data[f"{gt}/land_ice_segments/{v}"] for v in variables})
                df["latitude"] = lat[s0:s1]
                df["longitude"] = lon[s0:s1]
                frames.append(df.iloc[idx - s0])
    df = pd.concat(frames, ignore_index=True)
    df["time"] = atlas_epoch + (df.pop("delta_time") * 1e9).astype("timedelta64[ns]")
    atl06 = gpd.GeoDataFrame(df, geometry=gpd.points_from_xy(df.longitude, df.latitude), crs="EPSG:4326")
    gdf = atl06.set_index("time").sort_index().loc[t0:t1]
    print(f"H5Coro subsetted {name} / {len(gdf)} rows in {time.time() - start:.2f} seconds")

#
# SlideRule
#
def SlideRuleSubset(domain, cluster, name, region, start_time=None, end_time=None):
    session = sliderule.create_session(domain=domain, cluster=cluster)
    start = time.time()
    parms = {
        "max_resources": 100000,
        "output": {
            "fields": ["h_li", "h_li_sigma", "atl06_quality_summary", "segment_id"]
        }
    }
    if start_time: parms["t0"] = start_time
    if end_time: parms["t1"] = end_time
    gdf = sliderule.run("atl06x", parms, aoi=region["poly"], session=session)
    print(f"SlideRule subsetted {name} / {len(gdf)} rows in {time.time() - start:.2f} seconds")

#
# S2Tile
#
def S2TileSubset(reader, name, aoi, start_time=None, end_time=None):
    start = time.time()
    gdf = reader.read_tiles(aoi, start_time, end_time)
    print(f"S2TileReader subsetted {name} / {len(gdf)} rows in {time.time() - start:.2f} seconds")

#
# Benchmarks
#
parser = argparse.ArgumentParser(description="""ATL06 Tile Builder""")
parser.add_argument('--domain',         type=str,               default="slideruleearth.io")
parser.add_argument('--cluster',        type=str,               default="developers")
parser.add_argument('--bucket',         type=str,               default="sliderule-public")
parser.add_argument('--prefix',         type=str,               default="atl06tv2")
parser.add_argument('--earthaccess',    action='store_true',    default=False)
parser.add_argument('--h5coro',         action='store_true',    default=False)
parser.add_argument('--sliderule',      action='store_true',    default=False)
parser.add_argument('--s2tile',         action='store_true',    default=False)
parser.add_argument('--local',          action='store_true',    default=False)
parser.add_argument('--time_spans',     action='store_true',    default=False)
parser.add_argument('--aois',           nargs='+', type=int,    default=list(range(len(aois))))
args = parser.parse_args()

# EarthAccess - Spatial (can only be run one at a time due to memory)
if args.earthaccess:
    earthaccess.login()
    for i in args.aois:
       EarthAccessSubset(names[i], regions[names[i]]["poly"])

# EarthAccess - Temporal
if args.earthaccess and args.time_spans:
    earthaccess.login()
    EarthAccessSubset(names[1], regions[names[1]]["poly"], "2020-08-27", "2020-08-28")
    EarthAccessSubset(names[1], regions[names[1]]["poly"], "2020-08-27", "2020-11-28")
    EarthAccessSubset(names[1], regions[names[1]]["poly"], "2020-08-27", "2021-08-28")

# H5Coro - Spatial
if args.h5coro:
    for i in args.aois:
        H5CoroSubset(names[i], regions[names[i]]["poly"])

# H5Coro - Temporal
if args.h5coro and args.time_spans:
    H5CoroSubset(names[1], regions[names[1]]["poly"], "2020-08-27", "2020-08-28")
    H5CoroSubset(names[1], regions[names[1]]["poly"], "2020-08-27", "2020-11-28")
    H5CoroSubset(names[1], regions[names[1]]["poly"], "2020-08-27", "2021-08-28")

# SlideRule Service - Spatial
if args.sliderule:
    for i in args.aois:
        SlideRuleSubset(args.domain, args.cluster, names[i], regions[names[i]])

# SlideRule Local - Spatial
if args.sliderule and args.local:
    for i in args.aois:
        SlideRuleSubset("localhost", None, names[i], regions[names[i]])

# SlideRule Service - Temporal
if args.sliderule and args.time_spans:
    SlideRuleSubset(args.domain, args.cluster, names[1], regions[names[1]], "2020-08-27T00:00:00Z", "2020-08-28T00:00:00Z")
    SlideRuleSubset(args.domain, args.cluster, names[1], regions[names[1]], "2020-08-27T00:00:00Z", "2020-11-28T00:00:00Z")
    SlideRuleSubset(args.domain, args.cluster, names[1], regions[names[1]], "2020-08-27T00:00:00Z", "2021-08-28T00:00:00Z")

# SlideRule Local - Temporal
if args.sliderule and args.time_spans and args.local:
    SlideRuleSubset("localhost", None, names[1], regions[names[1]], "2020-08-27T00:00:00Z", "2020-08-28T00:00:00Z")
    SlideRuleSubset("localhost", None, names[1], regions[names[1]], "2020-08-27T00:00:00Z", "2020-11-28T00:00:00Z")
    SlideRuleSubset("localhost", None, names[1], regions[names[1]], "2020-08-27T00:00:00Z", "2021-08-28T00:00:00Z")

# S2Tile - Spatial
if args.s2tile:
    reader = s2tile.Reader(args.bucket, args.prefix)
    for i in args.aois:
        S2TileSubset(reader, names[i], aois[i])

# S2Tile - Temporal
if args.s2tile and args.time_spans:
    reader = s2tile.Reader(args.bucket, args.prefix)
    S2TileSubset(reader, names[1], aois[1], "2020-08-27", "2020-08-28")
    S2TileSubset(reader, names[1], aois[1], "2020-08-27", "2020-11-28")
    S2TileSubset(reader, names[1], aois[1], "2020-08-27", "2021-08-28")
