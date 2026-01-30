import sys
import copy
import time
import argparse
import boto3
from datetime import datetime, timedelta, timezone

# -------------------------------------------
# command line arguments
# -------------------------------------------
parser = argparse.ArgumentParser(
    description="""Athena usage report""",
    formatter_class=argparse.RawDescriptionHelpFormatter,
    epilog=(
        "Examples:\n"
        "  python athena_usage_report.py --start \"2026-01-10\" --end \"2026-01-29\"\n"
        "  python athena_usage_report.py --start \"2026-01-10\"\n"
    )
)
parser.add_argument('--start',             type=str,               default=None)  # ISO string
parser.add_argument('--end',               type=str,               default=None)  # ISO string
parser.add_argument('--top_n',             type=int,               default=0)
parser.add_argument('--verbose',           action='store_true',    default=False)
parser.add_argument('--repair_partitions', action='store_true',    default=False)
args,_ = parser.parse_known_args()

# -------------------------------------------
# globals
# -------------------------------------------
GLUE_DATABASE          = 'recorder_database'
ATHENA_WORKGROUP       = 'recorder-workgroup'
TELEMETRY_TABLE        = 'telemetry'
ALERTS_TABLE           = 'alerts'
PROJECT_BUCKET         = 'sliderule'
AWS_REGION             = 'us-west-2'
GEOLITE2_CITY_DB       = '/data/GeoLite2-City.mmdb'
GEOLITE2_COUNTRY_DB    = '/data/GeoLite2-Country.mmdb'
QUERY_WAIT_SECONDS     = 300
QUERY_POLL_SECONDS     = 2
#
# Endpoint naming schema:
#   p-series: proxied derived products
#   s-series: subsetted standard data products
#   v-series: quick views of standard data products
#   x-series: subsetted dataframes of standard data products
# Note: legacy reporting treats x-series as both granule-producing and proxied,
# so x endpoints appear in both ICESAT2_ENDPOINTS and ICESAT2_PROXY_ENDPOINTS.
#
ICESAT2_ENDPOINTS       = ['atl03x', 'atl03s', 'atl03v', 'atl06', 'atl06s', 'atl06x', 'atl08', 'atl08x', 'atl13s', 'atl13x', 'atl24x']
ICESAT2_PROXY_ENDPOINTS = ['atl03x', 'atl03sp', 'atl03vp', 'atl06p', 'atl06sp', 'atl08p', 'atl13sp', 'atl13x', 'atl24x', 'atl06x', 'atl08x']
GEDI_ENDPOINTS          = ['gedi01b', 'gedi02a', 'gedi04a']
GEDI_PROXY_ENDPOINTS    = ['gedi01bp', 'gedi02ap', 'gedi04ap']

athena = boto3.client('athena', region_name=AWS_REGION)
glue = boto3.client('glue', region_name=AWS_REGION)
s3 = boto3.client('s3', region_name=AWS_REGION)
_geo_country = None
_geo_city = None
_glue_table_sd_cache = {}

# -------------------------------------------
# helper functions
# -------------------------------------------

def parse_iso(value, end_of_day=False):
    if value is None:
        return None
    v = value.strip()
    if v.isdigit():
        epoch = int(v)
        # heuristic for ms/ns epoch
        if len(v) > 13:
            epoch = epoch / 1_000_000_000
        elif len(v) > 10:
            epoch = epoch / 1_000
        return datetime.fromtimestamp(epoch, tz=timezone.utc)
    if v.endswith('Z'):
        v = v[:-1] + '+00:00'
    if len(v) == 10:
        v = v + (' 23:59:59' if end_of_day else ' 00:00:00')
    try:
        dt = datetime.fromisoformat(v)
        if dt.tzinfo is None:
            return dt.replace(tzinfo=timezone.utc)
        return dt
    except ValueError:
        print(f'Invalid ISO datetime: {value}')
        sys.exit(2)


def escape_sql(value):
    return value.replace("'", "''")


def day_range(start_dt, end_dt):
    if start_dt is None or end_dt is None:
        return []
    start_date = start_dt.date()
    end_date = end_dt.date()
    days = []
    cur = start_date
    while cur <= end_date:
        days.append(cur)
        cur += timedelta(days=1)
    return days


def build_partition_filter(days):
    if len(days) == 0:
        return '1=1'
    parts = [f"(year = '{d.year:04d}' AND month = '{d.month:02d}' AND day = '{d.day:02d}')" for d in days]
    return '(' + ' OR '.join(parts) + ')'


def expected_partitions(days):
    return [f"year={d.year:04d}/month={d.month:02d}/day={d.day:02d}" for d in days]


def build_where_clause(start_dt, end_dt, days):
    conditions = []
    conditions.append(build_partition_filter(days))
    if start_dt and end_dt:
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
            f"{ts_expr} >= from_iso8601_timestamp('{escape_sql(start_iso)}') AND "
            f"{ts_expr} <= from_iso8601_timestamp('{escape_sql(end_iso)}')"
        )
    if len(conditions) == 0:
        return '1=1'
    return ' AND '.join(conditions)


def start_query(query):
    if args.verbose:
        print(query)
    request = {
        'QueryString': query,
        'QueryExecutionContext': {'Database': GLUE_DATABASE, 'Catalog': 'AwsDataCatalog'},
        'WorkGroup': ATHENA_WORKGROUP
    }
    response = athena.start_query_execution(**request)
    return response['QueryExecutionId']


def wait_for_query(query_execution_id):
    start_time = time.time()
    while True:
        response = athena.get_query_execution(QueryExecutionId=query_execution_id)
        status = response['QueryExecution']['Status']['State']
        if status in ['SUCCEEDED', 'FAILED', 'CANCELLED']:
            if status == 'FAILED':
                reason = response['QueryExecution']['Status'].get('StateChangeReason', 'Unknown')
                raise Exception(f'Query failed: {reason}')
            return status
        if time.time() - start_time > QUERY_WAIT_SECONDS:
            raise Exception('Query execution timeout')
        time.sleep(QUERY_POLL_SECONDS)


def get_query_results(query_execution_id):
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
    return results


def execute_query(query):
    query_execution_id = start_query(query)
    wait_for_query(query_execution_id)
    return get_query_results(query_execution_id)


def list_s3_day_partitions(bucket, label, start_dt, end_dt):
    found = set()
    start_month = start_dt.replace(day=1)
    end_month = end_dt.replace(day=1)
    cur = start_month
    while cur <= end_month:
        prefix = f"{label}/year={cur.year:04d}/month={cur.month:02d}/"
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
                        found.add(f"year={cur.year:04d}/month={cur.month:02d}/{day_part}")
            token = response.get('NextContinuationToken')
            if not response.get('IsTruncated'):
                break
        if cur.month == 12:
            cur = cur.replace(year=cur.year + 1, month=1)
        else:
            cur = cur.replace(month=cur.month + 1)
    return found


def glue_partitions(table_name):
    # Use Glue directly because Athena SHOW PARTITIONS/DDL can return stale or
    # inconsistent results depending on workgroup caching or permissions.
    partitions = []
    paginator = glue.get_paginator('get_partitions')
    for page in paginator.paginate(DatabaseName=GLUE_DATABASE, TableName=table_name):
        for part in page.get('Partitions', []):
            values = part.get('Values', [])
            if len(values) >= 3:
                year, month, day = values[0], values[1], values[2]
                partitions.append(f'year={year}/month={month}/day={day}')
    return partitions


def add_partitions(table, label, partitions):
    # Use Glue BatchCreatePartition instead of Athena DDL for reliability.
    table_name = table.split('.')[-1]
    if table_name in _glue_table_sd_cache:
        base_sd = _glue_table_sd_cache[table_name]
    else:
        table_def = glue.get_table(DatabaseName=GLUE_DATABASE, Name=table_name)
        base_sd = table_def['Table']['StorageDescriptor']
        _glue_table_sd_cache[table_name] = base_sd
    inputs = []
    for p in partitions:
        parts = p.split('/')
        kv = dict(item.split('=') for item in parts)
        year = kv.get('year')
        month = kv.get('month')
        day = kv.get('day')
        if not (year and month and day):
            continue
        location = f"s3://{PROJECT_BUCKET}/{label}/year={year}/month={month}/day={day}/"
        sd = copy.deepcopy(base_sd)
        sd['Location'] = location
        inputs.append({'Values': [year, month, day], 'StorageDescriptor': sd})
    if len(inputs) == 0:
        return
    for i in range(0, len(inputs), 100):
        batch = inputs[i:i+100]
        response = glue.batch_create_partition(
            DatabaseName=GLUE_DATABASE,
            TableName=table_name,
            PartitionInputList=batch
        )
        for err in response.get('Errors', []):
            code = err.get('ErrorDetail', {}).get('ErrorCode')
            msg = err.get('ErrorDetail', {}).get('ErrorMessage')
            values = err.get('PartitionValues', [])
            if code != 'AlreadyExistsException' and args.verbose:
                print(f'Glue partition error: {msg}')


def ensure_partitions_for_range(table, expected, label, start_dt, end_dt):
    if len(expected) == 0:
        return
    table_name = table.split('.')[-1]
    rows = glue_partitions(table_name)
    existing = set()
    for value in rows:
        if value:
            existing.add(value)
    if args.verbose:
        print(f'Existing partitions for {label}: {", ".join(sorted(existing))}')
    missing = [p for p in expected if p not in existing]
    if len(missing) == 0:
        return
    bucket = PROJECT_BUCKET
    missing_s3 = missing
    if bucket:
        try:
            s3_partitions = list_s3_day_partitions(bucket, label, start_dt, end_dt)
        except Exception as e:
            if args.verbose:
                print(f'Failed to list s3://{bucket}/{label}/ partitions: {e}')
            s3_partitions = set()
        if len(s3_partitions) == 0:
            if args.verbose:
                print(f'No S3 objects found for {label} partitions; skipping repair.')
            return
        missing_s3 = [p for p in missing if p in s3_partitions]
        missing = missing_s3
        if len(missing) == 0:
            return
    if args.verbose:
        print(f'Missing partitions for {label}: {", ".join(missing)}')
    else:
        print(f'Creating missing partitions for {label}')
    add_partitions(table, label, missing)
    existing = set(glue_partitions(table_name))
    still_missing = [p for p in expected if p not in existing]
    if bucket:
        still_missing = [p for p in still_missing if p in missing_s3]
    if len(still_missing) > 0:
        print(f'Still missing partitions for {label} after add: {", ".join(still_missing)}')
        sys.exit(2)


def value_counts(table, field, where_clause):
    query = f"""
        SELECT {field} AS key, COUNT(*) AS count
        FROM {table}
        WHERE {where_clause}
        GROUP BY {field}
    """
    rows = execute_query(query)
    counts = {}
    for row in rows:
        key = row.get('key')
        count = row.get('count')
        if key is None:
            key = 'None'
        if count is None:
            count = 0
        counts[key] = int(count)
    return counts


def sum_counts(counts, include_list=None):
    total = 0
    if include_list is not None:
        for item in include_list:
            if item in counts:
                total += counts[item]
    else:
        for item in counts:
            total += counts[item]
    return total


def sort_counts(counts):
    return sorted(counts.items(), key=lambda item: item[1], reverse=True)


def display_sorted(title, counts, top_n=0):
    print(f'\n===================\n{title}\n===================')
    sorted_counts = sort_counts(counts)
    if top_n and top_n > 0:
        sorted_counts = sorted_counts[:top_n]
    max_key_len = 0
    max_val_len = 0
    for key, _ in sorted_counts:
        if key is None:
            key = 'None'
        max_key_len = max(max_key_len, len(str(key)))
    for _, value in sorted_counts:
        max_val_len = max(max_val_len, len(str(value)))
    for count in sorted_counts:
        key = str(count[0])
        print(f'{key.ljust(max_key_len)}  {str(count[1]).rjust(max_val_len)}')


def display_summary(time_stats, unique_ip_counts, source_location_counts, total_requests,
                    total_icesat2_granules, total_icesat2_proxied_requests,
                    total_gedi_granules, total_gedi_proxied_requests):
    summary = {}
    if time_stats['start'] is not None and time_stats['end'] is not None:
        summary['Start'] = time_stats["start"].strftime("%Y-%m-%d %H:%M:%S")
        summary['End'] = time_stats["end"].strftime("%Y-%m-%d %H:%M:%S")
        duration_days = time_stats['span'].days
        duration_hours = (time_stats['span'].total_seconds() / 3600) % 24
        summary['Duration'] = f'{duration_days} days, {duration_hours:.2f} hours'
    else:
        summary['Start'] = 'None'
        summary['End'] = 'None'
        summary['Duration'] = '0 days, 0.00 hours'

    summary['Unique IPs'] = len(unique_ip_counts)
    summary['Unique Locations'] = len(source_location_counts)
    summary['Total Requests'] = total_requests
    summary['ICESat-2 Granules Processed'] = total_icesat2_granules
    summary['ICESat-2 Proxied Requests'] = total_icesat2_proxied_requests
    summary['GEDI Granules Processed'] = total_gedi_granules
    summary['GEDI Proxied Requests'] = total_gedi_proxied_requests

    max_label_len = max(len(k) for k in summary.keys())
    max_value_len = max(len(str(v)) for v in summary.values())
    for key in summary:
        print(f'{key.ljust(max_label_len)}  {str(summary[key]).rjust(max_value_len)}')


def get_geo():
    global _geo_country, _geo_city
    if _geo_country is None or _geo_city is None:
        import geoip2.database
        _geo_country = geoip2.database.Reader(GEOLITE2_COUNTRY_DB)
        _geo_city = geoip2.database.Reader(GEOLITE2_CITY_DB)
    return _geo_country, _geo_city


def locateit(source_ip, debug_info):
    try:
        if source_ip == '0.0.0.0' or source_ip == '127.0.0.1':
            return 'localhost, localhost'
        geo_country, geo_city = get_geo()
        country = geo_country.country(source_ip).country.name
        city = geo_city.city(source_ip).city.name
        return f'{country}, {city}'
    except Exception as e:
        print(f'Failed to get location information for <{debug_info}>: {e}')
        return 'unknown, unknown'


def build_location_counts(ip_counts):
    location_counts = {}
    for ip in ip_counts:
        location = locateit(ip, ip)
        if location not in location_counts:
            location_counts[location] = 0
        location_counts[location] += ip_counts[ip]
    return location_counts


def get_timespan(table, where_clause):
    query = f"""
        SELECT MIN(timestamp) AS start_time, MAX(timestamp) AS end_time
        FROM {table}
        WHERE {where_clause}
    """
    rows = execute_query(query)
    if len(rows) == 0:
        return {'start': None, 'end': None, 'span': None}
    start_str = rows[0].get('start_time')
    end_str = rows[0].get('end_time')
    if start_str is None or end_str is None:
        return {'start': None, 'end': None, 'span': None}
    start_dt = parse_iso(start_str)
    end_dt = parse_iso(end_str)
    return {'start': start_dt, 'end': end_dt, 'span': end_dt - start_dt}


def main():
    if args.repair_partitions and not args.start and not args.end:
        if args.verbose:
            print('Generating all missing partitions for telemetry and alerts.')
        print('Provide --start and --end to repair a specific range.')
        return
    if args.repair_partitions and args.start:
        start_dt = parse_iso(args.start)
        end_dt = parse_iso(args.end, end_of_day=True) if args.end else datetime.now(timezone.utc).replace(hour=23, minute=59, second=59, microsecond=0)
        if start_dt > end_dt:
            print('--start must be before --end')
            sys.exit(2)
        days = day_range(start_dt, end_dt)
        expected = expected_partitions(days)
        telemetry_table = f'{GLUE_DATABASE}.{TELEMETRY_TABLE}'
        alerts_table = f'{GLUE_DATABASE}.{ALERTS_TABLE}'
        ensure_partitions_for_range(telemetry_table, expected, 'telemetry', start_dt, end_dt)
        ensure_partitions_for_range(alerts_table, expected, 'alerts', start_dt, end_dt)
        return

    if not args.start:
        print('--start is required (ISO datetime string or YYYY-MM-DD).')
        sys.exit(2)

    start_dt = parse_iso(args.start)
    end_dt = parse_iso(args.end, end_of_day=True) if args.end else datetime.now(timezone.utc).replace(hour=23, minute=59, second=59, microsecond=0)
    if start_dt > end_dt:
        print('--start must be before --end')
        sys.exit(2)

    telemetry_table = f'{GLUE_DATABASE}.{TELEMETRY_TABLE}'
    alerts_table = f'{GLUE_DATABASE}.{ALERTS_TABLE}'

    days = day_range(start_dt, end_dt)
    expected = expected_partitions(days)
    ensure_partitions_for_range(telemetry_table, expected, 'telemetry', start_dt, end_dt)
    ensure_partitions_for_range(alerts_table, expected, 'alerts', start_dt, end_dt)

    telemetry_where = build_where_clause(start_dt, end_dt, days)
    alerts_where = build_where_clause(start_dt, end_dt, days)

    time_stats = get_timespan(telemetry_table, telemetry_where)

    unique_ip_counts = value_counts(telemetry_table, 'source_ip', telemetry_where)
    source_location_counts = build_location_counts(unique_ip_counts)
    client_counts = value_counts(telemetry_table, 'client', telemetry_where)
    endpoint_counts = value_counts(telemetry_table, 'endpoint', telemetry_where)
    telemetry_status_code_counts = value_counts(telemetry_table, 'code', telemetry_where)
    alert_status_code_counts = value_counts(alerts_table, 'code', alerts_where)

    total_requests = sum_counts(endpoint_counts)
    total_icesat2_granules = sum_counts(endpoint_counts, ICESAT2_ENDPOINTS)
    total_icesat2_proxied_requests = sum_counts(endpoint_counts, ICESAT2_PROXY_ENDPOINTS)
    total_gedi_granules = sum_counts(endpoint_counts, GEDI_ENDPOINTS)
    total_gedi_proxied_requests = sum_counts(endpoint_counts, GEDI_PROXY_ENDPOINTS)

    display_summary(
        time_stats,
        unique_ip_counts,
        source_location_counts,
        total_requests,
        total_icesat2_granules,
        total_icesat2_proxied_requests,
        total_gedi_granules,
        total_gedi_proxied_requests
    )

    display_sorted('Source Locations', source_location_counts, args.top_n)
    display_sorted('Clients', client_counts, args.top_n)
    display_sorted('Endpoints', endpoint_counts, args.top_n)
    display_sorted('Request Codes', telemetry_status_code_counts, args.top_n)
    display_sorted('Alert Codes', alert_status_code_counts, args.top_n)


if __name__ == '__main__':
    main()
