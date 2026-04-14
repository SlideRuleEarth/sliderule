# imports
from h5coro import h5coro, s3driver
from shapely.geometry import LineString, MultiLineString
from multiprocessing.pool import Pool
from contextlib import suppress
import geopandas as gpd
import numpy as np
import earthaccess
import argparse
import os

# constant parameters
TRACKS = ["gt1l", "gt1r", "gt2l", "gt2r", "gt3l", "gt3r"]
THRESHOLD_DEG = 0.25
BUFFER_DEG = 0.50
SIMPLIFY_DEG = 0.25
GRANULE_STEP = 100

# ------------------------------------------------
# HELPER FUNCTIONS
# ------------------------------------------------

# read filenames from cache
def read_cache(cache):
    data = []
    with suppress(FileNotFoundError):
        with open(cache, "r") as file:
            for line in file.readlines():
                filename = line.strip()
                data.append(filename)
            print(f"Found cache file <{cache}> with {len(data)} entries")
    return data

# write filenames to cache
def write_cache(cache, data):
    with open(cache, "w") as file:
        for granule in data:
            file.write(f'{granule}\n')

# get geometry for a granule
def get_granule_geometry(promise, threshold_deg, buffer_deg, simplify_deg):
    lines = []
    for track in TRACKS:
        latitudes = promise[f"{track}/segment_lat"]
        longitudes = promise[f"{track}/segment_lon"]
        lat_diffs = np.diff(latitudes)
        lon_diffs = np.diff(longitudes)
        lat_rad = np.radians(latitudes[:-1])  # match diff shape
        scaled_lon_diffs = lon_diffs * np.cos(lat_rad)
        jumps = np.where((lat_diffs**2 + scaled_lon_diffs**2) > threshold_deg**2)[0]
        last = len(latitudes) - 1
        prev = 0
        for jump in jumps:
            lines.append(LineString([(longitudes[prev],latitudes[prev]), (longitudes[jump],latitudes[jump])]))
            prev = jump + 1
        lines.append(LineString([(longitudes[prev],latitudes[prev]), (longitudes[last],latitudes[last])]))
    multiline = MultiLineString(lines)
    geometry = multiline.buffer(buffer_deg).simplify(simplify_deg)
    return geometry

# get begin time for a granule
def get_granule_begin_time(promise):
    begin_times = []
    for track in TRACKS:
        try:
            begin_times.append(promise[f"{track}/delta_time"][0])
        except:
            pass
    if len(begin_times) > 0:
        begin_time = min(begin_times)
    else:
        begin_time = 0.0
    delta_time = (1e9*begin_time).astype('timedelta64[ns]')
    return gpd.pd.to_datetime(np.datetime64('2018-01-01T00:00:00') + delta_time)

# ------------------------------------------------
# WORKER PROCESS FUNCTION
# ------------------------------------------------

def worker(args):
    try:
        granule, credentials = args
        h5obj = h5coro.H5Coro(granule, s3driver.S3Driver, credentials=credentials)
        datasets = [{"dataset": f'/{track}/{variable}', "hyperslice": []} for track in TRACKS for variable in ["segment_lat", "segment_lon"]]
        datasets += [{"dataset": f'/{track}/{variable}', "hyperslice": [(0,1)]} for track in TRACKS for variable in ["delta_time"]]
        data = h5obj.readDatasets(datasets=datasets, block=True)
        geometry = get_granule_geometry(data, THRESHOLD_DEG, BUFFER_DEG, SIMPLIFY_DEG)
        begin_time = get_granule_begin_time(data)
        return {
            "granule": granule.split("/")[-1],
            "count": len(geometry.geoms),
            "begin_time": begin_time,
            "geometry": geometry
        }, True
    except Exception as e:
        return f"{e}", False

# ------------------------------------------------
# MAIN
# ------------------------------------------------

if __name__ == "__main__":

    # command line parameters
    parser = argparse.ArgumentParser(description="""ATL24""")
    parser.add_argument('--granule_cache',  type=str,   default="/data/ATL13/atl13_granules.txt")
    parser.add_argument('--atl13_granules', type=str,   default="/data/ATL13/atl13_granules.parquet")
    parser.add_argument("--cores",          type=int,   default=os.cpu_count())
    args,_ = parser.parse_known_args()

    # get list of granules to process
    granules = read_cache(args.granule_cache)
    if len(granules) == 0:
        print(f"Querying earth access for ATL13 data")
        results = earthaccess.search_data(short_name = 'ATL13', version = '007', cloud_hosted = True, count=-1)
        granules = [x['umm']['RelatedUrls'][0]['URL'].split('https://data.nsidc.earthdatacloud.nasa.gov/')[-1] for x in results]
        write_cache(args.granule_cache, granules)

    # run workers
    columns = {}
    for i in range(0, len(granules), GRANULE_STEP):
        pool = Pool(args.cores)
        auth = earthaccess.login()
        credentials = auth.get_s3_credentials(daac="NSIDC")
        worker_args = [(granule, credentials) for granule in granules[i:i+GRANULE_STEP]]
        for result, ok in pool.imap_unordered(worker, worker_args):
            if ok == True:
                print(f"Processed granules {i} to {i + GRANULE_STEP}: {result['granule']}")
                for column in result:
                    if column not in columns:
                        columns[column] = []
                    columns[column].append(result[column])
            else:
                print(f"Error processing granules {i} to {i+GRANULE_STEP}: {result}")
        pool.close()
        pool.join()
        del pool

    # write parquet file out
    print(f'Writing parquet file {args.atl13_granules}')
    gdf = gpd.GeoDataFrame(columns, crs="EPSG:7912")
    gdf.to_parquet(args.atl13_granules, index=True)
