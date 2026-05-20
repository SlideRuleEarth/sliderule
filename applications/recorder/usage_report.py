import csv
import sys
import copy
import time
import argparse
import boto3
import pandas as pd
import numpy as np
import geoip2.database
from PIL import Image
from matplotlib.colors import LogNorm
import matplotlib.cm as cm
from datetime import datetime, timezone
from collections import defaultdict

# -------------------------------------------
# command line arguments
# -------------------------------------------
parser = argparse.ArgumentParser(
    description="""SlideRule Usage Report""",
    formatter_class=argparse.RawDescriptionHelpFormatter,
    epilog=(
        "Examples:\n"
        "  python usage_report.py --start \"2026-01-10\" --end \"2026-01-29\"\n"
        "  python usage_report.py --start \"2026-01-10\"\n"
    )
)
parser.add_argument('--start',          type=str,   required=True, help='start is required as ISO datetime string YYYY-MM-DD HH:MM:SS)') #
parser.add_argument('--end',            type=str,   default=f'{datetime.now()}') # optional ISO datetime string
parser.add_argument('--grid_aoi',       type=str,   default="/data/web/sliderule_aoi_grid.png")
parser.add_argument('--grid_usage',     type=str,   default="/data/web/sliderule_usage_grid.png")
parser.add_argument('--geonames',       type=str,   default="/data/geopy/cities500.txt") # from http://download.geonames.org/export/dump/cities500.zip
parser.add_argument('--country_info',   type=str,   default="/data/geopy/countryInfo.txt") # from http://download.geonames.org/export/dump/countryInfo.txt
args,_ = parser.parse_known_args()

# -------------------------------------------
# constants
# -------------------------------------------
GLUE_DATABASE           = 'recorder-database'
ATHENA_WORKGROUP        = 'recorder-workgroup'
TELEMETRY_TABLE         = 'telemetry'
ALERTS_TABLE            = 'alerts'
PROJECT_BUCKET          = 'sliderule'
AWS_REGION              = 'us-west-2'
GEOLITE2_CITY_DB        = '/data/GeoLite2-City.mmdb'
GEOLITE2_COUNTRY_DB     = '/data/GeoLite2-Country.mmdb'
QUERY_WAIT_SECONDS      = 300
QUERY_POLL_SECONDS      = 2
MAX_LABEL_LEN           = 30
MAX_VALUE_LEN           = 20
ICESAT2_ENDPOINTS       = ['atl03x', 'atl03s', 'atl03v', 'atl06', 'atl06s', 'atl06x', 'atl08', 'atl08x', 'atl13s', 'atl13x', 'atl24x']
ICESAT2_PROXY_ENDPOINTS = ['atl03x', 'atl03sp', 'atl03vp', 'atl06p', 'atl06sp', 'atl08p', 'atl13sp', 'atl13x', 'atl24x', 'atl06x', 'atl08x']
GEDI_ENDPOINTS          = ['gedi01b', 'gedi02a', 'gedi04a']
GEDI_PROXY_ENDPOINTS    = ['gedi01bp', 'gedi02ap', 'gedi04ap']

# -------------------------------------------
# globals
# -------------------------------------------
athena = boto3.client('athena', region_name=AWS_REGION)
glue = boto3.client('glue', region_name=AWS_REGION)
s3 = boto3.client('s3', region_name=AWS_REGION)
geo_country = geoip2.database.Reader(GEOLITE2_COUNTRY_DB)
geo_city = geoip2.database.Reader(GEOLITE2_CITY_DB)

# -------------------------------------------
# build where clause
# -------------------------------------------
def build_where_clause(start_dt, end_dt, days):
    conditions = []
    # build partition filter
    if len(days) > 31: # enumerate individual days when the number of days is less than a typical month
        parts = [f"(year = '{dt.year:04d}' AND month = '{dt.month:02d}' AND day = '{dt.day:02d}')" for dt in days]
    else: # enumerate months
        parts = list({f"(year = '{dt.year:04d}' AND month = '{dt.month:02d}')" for dt in days})
    conditions.append( '(' + ' OR '.join(parts) + ')' )
    # build timestamp expression
    start_iso = start_dt.strftime('%Y-%m-%dT%H:%M:%S')
    end_iso = end_dt.strftime('%Y-%m-%dT%H:%M:%S')
    ts_expr = (
        "CASE "
        "WHEN regexp_like(timestamp, '^[0-9]+$') "
        "THEN from_unixtime(CAST(timestamp AS bigint)) "
        "ELSE from_iso8601_timestamp(timestamp) "
        "END"
    )
    conditions.append(
        f"{ts_expr} >= from_iso8601_timestamp('{start_iso}') AND "
        f"{ts_expr} <= from_iso8601_timestamp('{end_iso}')"
    )
    # return where clause
    return ' AND '.join(conditions)

# -------------------------------------------
# execute query
# -------------------------------------------
def execute_query(query, context=''):

    # initiate athena query
    request = {
        'QueryString': query,
        'QueryExecutionContext': {'Database': GLUE_DATABASE, 'Catalog': 'AwsDataCatalog'},
        'WorkGroup': ATHENA_WORKGROUP
    }
    response = athena.start_query_execution(**request)
    query_execution_id = response['QueryExecutionId']
    print(f'Initiated query on {context}', end='')

    # wait for athena query to complete
    status = 'IN PROGRESS'
    start_time = time.time()
    while (time.time() - start_time) < QUERY_WAIT_SECONDS:
        response = athena.get_query_execution(QueryExecutionId=query_execution_id)
        status = response['QueryExecution']['Status']['State']
        if status in ['SUCCEEDED', 'FAILED', 'CANCELLED']:
            break
        sys.stdout.write(".")
        sys.stdout.flush()
        time.sleep(QUERY_POLL_SECONDS)

    # check athena query status
    if status == 'FAILED':
        reason = response['QueryExecution']['Status'].get('StateChangeReason', 'Unknown')
        raise Exception(f'Query failed: {reason}')
    elif status == 'IN PROGRESS':
        raise Exception('Query execution timeout')

    # get athena query results
    results = []
    paginator = athena.get_paginator('get_query_results')
    page_iterator = paginator.paginate(QueryExecutionId=query_execution_id)
    column_names = None
    for page in page_iterator:
        for row in page['ResultSet']['Rows']:
            if column_names is None:
                column_names = [col.get('VarCharValue') for col in row['Data']]
                continue
            row_data = {}
            for i, col in enumerate(row['Data']):
                col_name = column_names[i]
                col_value = col.get('VarCharValue') if col else None
                row_data[col_name] = col_value
            results.append(row_data)
        sys.stdout.write("!")
        sys.stdout.flush()

    # return athena query results
    sys.stdout.write("\n")
    sys.stdout.flush()
    return results

# -------------------------------------------
# list s3 day partitions
# -------------------------------------------
def list_s3_day_partitions(bucket, label, start_dt, end_dt):
    found = set()
    curr_dt = start_dt
    while curr_dt <= end_dt:
        prefix = f"{label}/year={curr_dt.year:04d}/month={curr_dt.month:02d}/"
        token = None
        while True:
            if token:
                response = s3.list_objects_v2(Bucket=bucket, Prefix=prefix, Delimiter='/', ContinuationToken=token)
            else:
                response = s3.list_objects_v2(Bucket=bucket, Prefix=prefix, Delimiter='/')
            for cp in response.get('CommonPrefixes', []):
                p = cp.get('Prefix', '')
                if p.endswith('/') and p.startswith(prefix):
                    day_part = p[len(prefix):].strip('/')
                    if day_part.startswith('day='):
                        found.add(f"year={curr_dt.year:04d}/month={curr_dt.month:02d}/{day_part}")
            token = response.get('NextContinuationToken')
            if not response.get('IsTruncated'):
                break
        if curr_dt.month == 12:
            curr_dt = curr_dt.replace(year=curr_dt.year+1, month=1, day=1)
        else:
            curr_dt = curr_dt.replace(month=curr_dt.month+1, day=1)
    return found

# -------------------------------------------
# glue partitions
# -------------------------------------------
def glue_partitions(table_name):
    partitions = []
    paginator = glue.get_paginator('get_partitions')
    for page in paginator.paginate(DatabaseName=GLUE_DATABASE, TableName=table_name):
        for part in page.get('Partitions', []):
            values = part.get('Values', [])
            if len(values) >= 3:
                year, month, day = values[0], values[1], values[2]
                partitions.append(f'year={year}/month={month}/day={day}')
    return set(partitions)

# -------------------------------------------
# ensure partitions for range
# -------------------------------------------
def ensure_partitions_for_range(table_name, expected, label, start_dt, end_dt):
    # build list of requested partitions that are missing from the glue table
    existing_glue = glue_partitions(table_name)
    existing_s3 = list_s3_day_partitions(PROJECT_BUCKET, label, start_dt, end_dt)
    missing = [p for p in expected if ((p not in existing_glue) and (p in existing_s3))]
    print(f'Existing glue partitions for {label}: {len(existing_glue)}')
    print(f'Existing s3 partitions for {label}: {len(existing_s3)}')
    print(f'Missing partitions for {label}: {len(missing)}')

    # add missing partitions to the glue table
    table_def = glue.get_table(DatabaseName=GLUE_DATABASE, Name=table_name)
    base_sd = table_def['Table']['StorageDescriptor']
    inputs = []
    for partition in missing:
        kv = dict([item.split('=') for item in partition.split('/')])
        location = f"s3://{PROJECT_BUCKET}/{label}/year={kv['year']}/month={kv['month']}/day={kv['day']}/"
        sd = copy.deepcopy(base_sd)
        sd['Location'] = location
        inputs.append({'Values': [kv['year'], kv['month'], kv['day']], 'StorageDescriptor': sd})
    for i in range(0, len(inputs), 100):
        batch = inputs[i:i+100]
        response = glue.batch_create_partition(DatabaseName=GLUE_DATABASE, TableName=table_name, PartitionInputList=batch)
        for err in response.get('Errors', []):
            code = err.get('ErrorDetail', {}).get('ErrorCode')
            msg = err.get('ErrorDetail', {}).get('ErrorMessage')
            if code != 'AlreadyExistsException':
                print(f'Glue partition error: {msg}')

# -------------------------------------------
# value counts
# -------------------------------------------
def value_counts(table, field, where_clause):
    query = f"""
        SELECT {field} AS key, COUNT(*) AS count
        FROM {table}
        WHERE {where_clause}
        GROUP BY {field}
    """
    rows = execute_query(query, f'{field} of {table}')
    return {row['key']: int(row['count']) for row in rows}

# -------------------------------------------
# sum counts
# -------------------------------------------
def sum_counts(counts, include_list=None, match_str=None):
    total = 0
    for item in counts:
        if (not include_list and not match_str) or \
           (include_list and item in include_list) or \
           (match_str and match_str in item):
            total += counts[item]
    return total

# -------------------------------------------
# locate it
# -------------------------------------------
def locateit(source_ip, debug_info):
    try:
        if source_ip == '0.0.0.0' or source_ip == '127.0.0.1':
            return 'localhost, localhost'
        country = geo_country.country(source_ip).country.name
        city = geo_city.city(source_ip).city.name
        return f'{country}, {city}'
    except Exception as e:
        print(f'Failed to get location information for <{debug_info}>: {e}')
        return 'unknown, unknown'

# -------------------------------------------
# build location counts
# -------------------------------------------
def build_location_counts(ip_counts):
    location_counts = {}
    for ip in ip_counts:
        location = locateit(ip, ip)
        if location not in location_counts:
            location_counts[location] = 0
        location_counts[location] += ip_counts[ip]
    return location_counts

# -------------------------------------------
# get timespan
# -------------------------------------------
def get_timespan(table, where_clause):
    query = f"""
        SELECT MIN(timestamp) AS start_time, MAX(timestamp) AS end_time
        FROM {table}
        WHERE {where_clause}
    """
    rows = execute_query(query, f'timespan of {table}')
    start_unix_ts = int(rows[0]['start_time'].strip())
    end_unix_ts = int(rows[0]['end_time'].strip())
    start_dt = datetime.fromtimestamp(start_unix_ts, tz=timezone.utc)
    end_dt = datetime.fromtimestamp(end_unix_ts, tz=timezone.utc)
    return {'start': start_dt, 'end': end_dt, 'span': end_dt - start_dt}

# -------------------------------------------
# generate map
# -------------------------------------------
def generate_map(grid, filename):
    data = grid.astype(np.float64)
    data[data == 0] = np.nan

    # Log-scale normalization to accentuate lower values
    vmin = max(np.nanmin(data), 1.0)  # clamp min to 1 for log scale
    norm = LogNorm(vmin=vmin, vmax=np.nanmax(data))
    mapped = cm.jet(norm(data))  # RGBA float array
    mapped = (mapped * 255).astype(np.uint8)

    # Make NaN pixels fully transparent
    mask = np.isnan(data)
    mapped[mask] = [0, 0, 0, 0]

    # Save image
    img = Image.fromarray(np.flipud(mapped), mode='RGBA')
    img.save(filename)
    print(f'Saved PNG {filename}: {img.size[0]}x{img.size[1]} pixels')

# -------------------------------------------
# AIO grid
# -------------------------------------------
def aoi_sql_grid(table, where_clause, filename):
    query = f"""
        SELECT
            CAST(FLOOR((aoi_x + 180.0) / 0.25) AS INTEGER) AS grid_x,
            CAST(FLOOR((aoi_y + 90.0)  / 0.25) AS INTEGER) AS grid_y,
            COUNT(*) AS point_count
        FROM "recorder-database"."telemetry"
        WHERE aoi_x IS NOT NULL
        AND aoi_y IS NOT NULL
        AND {where_clause}
        GROUP BY
            FLOOR((aoi_x + 180.0) / 0.25),
            FLOOR((aoi_y + 90.0)  / 0.25)
        ORDER BY
            grid_x, grid_y;
    """
    rows = execute_query(query, f'AOI of {table}')
    grid = np.zeros((720, 1440), dtype=np.uint32)
    for row in rows:
        x = int(row["grid_x"])
        y = int(row["grid_y"])
        if x == 1440: x = 1439
        if y == 720: y = 719
        if(x < 1440 and x >= 0 and y < 720 and y >= 0):
            grid[y, x] = row["point_count"]
        else:
            print(f"Not gridding {y},{x} => {row["point_count"]}")
    generate_map(grid, filename)

# -------------------------------------------
# grid usage
# -------------------------------------------
def load_geonames(path):
    """Load GeoNames cities500.txt into a dict keyed by city name (lowercase).
    Each entry is a list of (city_name, country_code, lat, lon, population)."""
    print("Loading GeoNames database...")
    cities = defaultdict(list)
    with open(path, "r", encoding="utf-8") as f:
        reader = csv.reader(f, delimiter="\t", quoting=csv.QUOTE_NONE)
        for row in reader:
            name = row[1]
            asciiname = row[2]
            lat = float(row[4])
            lon = float(row[5])
            country_code = row[8]
            population = int(row[14]) if row[14] else 0
            entry = (name, country_code, lat, lon, population)
            cities[name.lower()].append(entry)
            if asciiname.lower() != name.lower():
                cities[asciiname.lower()].append(entry)
            # Also index alternate names
            for alt in row[3].split(","):
                alt = alt.strip().lower()
                if alt and alt not in cities:
                    cities[alt].append(entry)
    print(f"Loaded {len(cities)} unique city name keys.")
    return cities

def load_countrycodes(path):
    """Country code lookup from country name"""
    print("Loading CountryCode database...")
    country_to_code = {}
    with open(path, "r", encoding="utf-8") as f:
        for line in f:
            if line.startswith("#"):
                continue
            parts = line.strip().split("\t")
            if len(parts) >= 5:
                country_to_code[parts[4].lower()] = parts[0]
    print(f"Loaded {len(country_to_code)} country codes.")
    return country_to_code

def geocode_offline(place, geonames_db, countrycode_db):
    """Lookup lat/lon from 'Country, City' string using local GeoNames database."""
    parts = [p.strip() for p in place.split(",")]
    city_name = parts[-1] if len(parts) > 1 else parts[0]
    country_name = parts[0] if len(parts) > 1 else None
    # If city is "None", use the most populous city in the country as a fallback
    if city_name.lower() == "none":
        if country_name:
            country_code = countrycode_db.get(country_name.lower())
            if country_code:
                # Find the most populous city in that country
                best = None
                for candidates_list in geonames_db.values():
                    for c in candidates_list:
                        if c[1] == country_code:
                            if best is None or c[4] > best[4]:
                                best = c
                if best:
                    return best[2], best[3]
        return None
    candidates = geonames_db.get(city_name.lower(), [])
    if not candidates:
        return None
    # filter by country if provided
    if country_name:
        country_code = countrycode_db.get(country_name.lower())
        if country_code:
            filtered = [c for c in candidates if c[1] == country_code]
            if filtered:
                candidates = filtered
    # pick the one with highest population
    best = max(candidates, key=lambda c: c[4])
    return best[2], best[3]

def usage_grid(locations, filename):
    grid = np.zeros((720, 1440), dtype=np.uint32)
    geonames_db = load_geonames(args.geonames)
    countrycode_db = load_countrycodes(args.country_info)
    # create grid of locations
    for place, count in locations.items():
        loc = geocode_offline(place, geonames_db, countrycode_db)
        if loc is not None:
            lat, lon = loc
        else:
            print(f"Could not geocode {place}")
            continue
        # Convert lat/lon to 0.25 degree grid indices
        # Lat: -90 to 90 -> row 0 (south) to 719 (north)
        # Lon: -180 to 180 -> col 0 (west) to 1439 (east)
        row = int((lat + 90) / 0.25)
        col = int((lon + 180) / 0.25)
        row = min(max(row, 0), 719)
        col = min(max(col, 0), 1439)
#        print(f"{place} -> lat={lat:.2f}, lon={lon:.2f} -> row={row}, col={col}, count={count}")
        grid[row, col] = count
        # expand grid points into boxes based on value partitions
        expanded = np.zeros_like(grid, dtype=np.uint32)
        ys, xs = np.where(grid > 0) # Find all non-zero cells
        for y, x in zip(ys, xs):
            val = grid[y, x]
            # determine partition (0..24 -> 1, 25..49 -> 2, ..., 225..255 -> 10)
            partition = val // 25 + 1
            partition = min(partition, 10)
            radius = partition
            # define box bounds, clipped to grid
            y_min = max(y - radius, 0)
            y_max = min(y + radius, 719)
            x_min = max(x - radius, 0)
            x_max = min(x + radius, 1439)
            # add value to all cells in the box
            expanded[y_min:y_max+1, x_min:x_max+1] += val
    # create png
    generate_map(expanded, filename)

# -------------------------------------------
# display stats
# -------------------------------------------
def display_stats(title, stats, sort_values=False):
    print(f'\n===================\n{title}\n===================')
    if sort_values:
        stat_list = sorted(stats.items(), key=lambda item: item[1], reverse=True)
    else:
        stat_list = stats.items()
    for count in stat_list:
        print(f'{str(count[0]).ljust(MAX_LABEL_LEN)}  {str(count[1]).rjust(MAX_VALUE_LEN)}')

# -------------------------------------------
# main
# -------------------------------------------

# build needed athena partitions to handle request
start_dt    = datetime.fromisoformat(args.start)
end_dt      = datetime.fromisoformat(args.end)
days        = pd.date_range(start_dt.date(), end_dt.date(), freq='D').date.tolist()
expected    = [f"year={d.year:04d}/month={d.month:02d}/day={d.day:02d}" for d in days]
ensure_partitions_for_range(TELEMETRY_TABLE, expected, 'telemetry', start_dt, end_dt)
ensure_partitions_for_range(ALERTS_TABLE, expected, 'alerts', start_dt, end_dt)

# query for usage statistics
telemetry_table                 = f'"{GLUE_DATABASE}".{TELEMETRY_TABLE}'
alerts_table                    = f'"{GLUE_DATABASE}".{ALERTS_TABLE}'
where_clause                    = build_where_clause(start_dt, end_dt, days)
time_stats                      = get_timespan(telemetry_table, where_clause)
unique_ip_counts                = value_counts(telemetry_table, 'source_ip', where_clause)
source_location_counts          = build_location_counts(unique_ip_counts)
client_counts                   = value_counts(telemetry_table, 'client', where_clause)
endpoint_counts                 = value_counts(telemetry_table, 'endpoint', where_clause)
telemetry_status_code_counts    = value_counts(telemetry_table, 'code', where_clause)
alert_status_code_counts        = value_counts(alerts_table, 'code', where_clause)
summary = {
    'Start':                        time_stats["start"].strftime("%Y-%m-%d %H:%M:%S"),
    'End':                          time_stats["end"].strftime("%Y-%m-%d %H:%M:%S"),
    'Duration':                     f'{time_stats['span'].days} days, {(time_stats['span'].total_seconds() / 3600) % 24:.2f} hours',
    'Unique IPs':                   len(unique_ip_counts),
    'Unique Locations':             len(source_location_counts),
    'Total Requests':               sum_counts(endpoint_counts),
    'Python Client Requests':       sum_counts(client_counts, match_str="python"),
    'Web Client Requests':          sum_counts(client_counts, match_str="web"),
    'Unknown Client Requests':      sum_counts(client_counts, match_str="unknown"),
    'ICESat-2 Granules Processed':  sum_counts(endpoint_counts, ICESAT2_ENDPOINTS),
    'ICESat-2 Proxied Requests':    sum_counts(endpoint_counts, ICESAT2_PROXY_ENDPOINTS),
    'GEDI Granules Processed':      sum_counts(endpoint_counts, GEDI_ENDPOINTS),
    'GEDI Proxied Requests':        sum_counts(endpoint_counts, GEDI_PROXY_ENDPOINTS)
}

# display usage statistics
display_stats('Source Locations', source_location_counts, True)
display_stats('Clients', client_counts, True)
display_stats('Endpoints', endpoint_counts, True)
display_stats('Request Codes', telemetry_status_code_counts, True)
display_stats('Alert Codes', alert_status_code_counts, True)
display_stats('Summary', summary, False)

# grid requests
usage_grid(source_location_counts, args.grid_usage)
aoi_sql_grid(telemetry_table, where_clause, args.grid_aoi)
display_stats('Globe', {
    'icesat2':  sum_counts(endpoint_counts, ICESAT2_ENDPOINTS),
    'gedi':     sum_counts(endpoint_counts, GEDI_ENDPOINTS),
}, False)
