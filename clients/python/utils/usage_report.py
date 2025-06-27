# Copyright (c) 2021, University of Washington
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are met:
#
# 1. Redistributions of source code must retain the above copyright notice,
#    this list of conditions and the following disclaimer.
#
# 2. Redistributions in binary form must reproduce the above copyright notice,
#    this list of conditions and the following disclaimer in the documentation
#    and/or other materials provided with the distribution.
#
# 3. Neither the name of the University of Washington nor the names of its
#    contributors may be used to endorse or promote products derived from this
#    software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE UNIVERSITY OF WASHINGTON AND CONTRIBUTORS
# “AS IS” AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
# TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
# PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE UNIVERSITY OF WASHINGTON OR
# CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
# EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
# PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
# OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
# WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
# OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
# ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

# Imports
import os
import sys
import argparse
import boto3
import duckdb
from datetime import datetime, timedelta
from sliderule.session import Session

# Command Line Arguments
parser = argparse.ArgumentParser(description="""usage report""")
parser.add_argument('--domain',             type=str,               default="slideruleearth.io")
parser.add_argument('--organization',       type=str,               default="sliderule")
parser.add_argument('--localdir',           type=str,               default="/data/")
parser.add_argument('--verbose',            action='store_true',    default=False)
parser.add_argument('--apikey',             type=str,               default="")
parser.add_argument('--export',             action='store_true',    default=False)
parser.add_argument('--imports',            nargs='+', type=str,    default=[]) # s3://sliderule/data/manager/manager-developers-latest.db
args,_ = parser.parse_known_args()

# Initialize Organization
if args.organization == "None":
    args.organization = None

# Initialize SlideRule Client
session = Session(domain=args.domain, organization=args.organization, verbose=args.verbose)

##############################
# Helper Functions
##############################

# Sum Counts
def sum_counts(counts, include_list=None):
    total = 0
    if include_list != None:
        for item in include_list:
            if item in counts:
                total += counts[item]
    else:
        for item in counts:
            total += counts[item]
    return total

# Sort Counts
def sort_counts(counts):
    return sorted(counts.items(), key=lambda item: item[1], reverse=True)

# Display Sorted
def display_sorted(title, counts):
    print(f'\n===================\n{title}\n===================')
    sorted_counts = sort_counts(counts)
    for count in sorted_counts:
        print(f'{count[0]}: {count[1]}')

def value_counts(table, field, db=None):
    if db != None:
        table = {"telemetry": "combined_telemetry", "alerts": "combined_alerts"}.get(table)
        # build initial query
        cmd = f"""
            SELECT {field}, COUNT(*) AS count
            FROM {table}
            GROUP BY {field}
        """
        # execute request
        result = db.execute(cmd).fetchall()
        # return response
        return {key:value for (key,value) in result}
    else:
        api = {"telemetry": "telemetry_counts", "alerts": "alert_counts"}.get(table)
        return session.manager(f'status/{api}/{field}')

def timespan(table, field, db=None):
    if db != None:
        table = {"telemetry": "combined_telemetry", "alerts": "combined_alerts"}.get(table)
        # build initial query
        cmd = f"""
            SELECT
                MIN({field}) AS start_time,
                MAX({field}) AS end_time
            FROM {table};
        """
        # execute request
        data = db.execute(cmd).fetchall()
        result = {"start": data[0][0].isoformat(), "end": data[0][1].isoformat(), "span": (data[0][1] - data[0][0]).total_seconds()}
    else:
        result = session.manager(f'status/timespan/{field}')
    # return response
    return {"start": datetime.fromisoformat(result["start"]), "end": datetime.fromisoformat(result["end"]), "span": timedelta(seconds=result["span"])}

##############################
# Export Database - Exits
##############################

if args.export:
    response = session.manager("db/export", content_json=False, as_post=True, headers={"x-sliderule-api-key": args.apikey})
    print(response)
    sys.exit(0)

##############################
# Import Databases
##############################

db = None
if len(args.imports) > 0:
    db_files = []

    # download all database files locally
    for database_file in args.imports:
        path = database_file.split("s3://")[-1].split("/")
        bucket = path[0]
        key = '/'.join(path[1:])
        filename = os.path.join(args.localdir, path[-1])
        if not os.path.exists(filename):
            s3_client = boto3.client('s3')
            s3_client.download_file(bucket, key, filename)
            print(f'Downloaded {database_file}')
        else:
            print(f'Using {filename}')
        db_files.append(filename)

    # create database connections
    db = duckdb.connect()
    for i, f in enumerate(db_files):
        db.execute(f"ATTACH '{f}' AS db{i}")

    # combine telemetry tables
    cmd = "CREATE VIEW combined_telemetry AS\n"
    cmd += 'UNION ALL\n'.join([f'SELECT * FROM db{i}.telemetry\n' for i in range(len(db_files))])
    db.execute(cmd)

    # combine telemetry tables
    cmd = "CREATE VIEW combined_alerts AS\n"
    cmd += 'UNION ALL\n'.join([f'SELECT * FROM db{i}.alerts\n' for i in range(len(db_files))])
    db.execute(cmd)

##############################
# Collect Statistics
##############################

# Calculate Time Statistics
time_stats                      = timespan('telemetry', 'record_time', db)

# Gather Statistics on Usage
unique_ip_counts                =  value_counts('telemetry', 'source_ip_hash', db)
source_location_counts          =  value_counts('telemetry', 'source_ip_location', db)
client_counts                   =  value_counts('telemetry', 'client', db)
endpoint_counts                 =  value_counts('telemetry', 'endpoint', db)
telemetry_status_code_counts    =  value_counts('telemetry', 'status_code', db)
alert_status_code_counts        =  value_counts('alerts', 'status_code', db)

# Process Request Counts
total_requests                  = sum_counts(endpoint_counts)
total_icesat2_granules          = sum_counts(endpoint_counts, ['atl03x', 'atl03s', 'atl03v', 'atl06', 'atl06s', 'atl08', 'atl13s', 'atl13x', 'atl24x'])
total_icesat2_proxied_requests  = sum_counts(endpoint_counts, ['atl03x', 'atl03sp', 'atl03vp', 'atl06p', 'atl06sp', 'atl08p', 'atl13sp', 'atl13x', 'atl24x'])
total_gedi_granules             = sum_counts(endpoint_counts, ['gedi01b', 'gedi02a', 'gedi04a'])
total_gedi_proxied_requests     = sum_counts(endpoint_counts, ['gedi01bp', 'gedi02ap', 'gedi04ap'])

##############################
# Report Statistics
##############################

# Report Time Statistics
print(f'Start: {time_stats["start"].strftime("%Y-%m-%d %H:%M:%S")}')
print(f'End: {time_stats["end"].strftime("%Y-%m-%d %H:%M:%S")}')
print(f'Duration: {time_stats["span"].days} days, {(time_stats["span"].total_seconds() / 3600) % 24:.2f} hours')

# Report Usage Statistics
print(f'Unique IPs: {len(unique_ip_counts)}')
print(f'Unique Locations: {len(source_location_counts)}')
print(f'Total Requests: {total_requests}')
print(f'ICESat-2 Granules Processed: {total_icesat2_granules}')
print(f'ICESat-2 Proxied Requests: {total_icesat2_proxied_requests}')
print(f'GEDI Granules Processed: {total_gedi_granules}')
print(f'GEDI Proxied Requests: {total_gedi_proxied_requests}')

# Display Sorted
display_sorted("Source Locations", source_location_counts)
display_sorted("Clients", client_counts)
display_sorted("Endpoints", endpoint_counts)
display_sorted("Request Codes", telemetry_status_code_counts)
display_sorted("Alert Codes", alert_status_code_counts)
