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

#
# TODO: 
#   1. Add option (defaulted to on) to export the database after running the report
#   2. Add option to download from S3 all manager databases for a cluster and combine together for a report
#     

# Imports
import argparse
from sliderule.session import Session

# Command Line Arguments
parser = argparse.ArgumentParser(description="""usage report""")
parser.add_argument('--domain',             type=str,               default="slideruleearth.io")
parser.add_argument('--organization',       type=str,               default="sliderule")
parser.add_argument('--verbose',            action='store_true',    default=False)
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

##############################
# Collect Statistics
##############################

# Gather Statistics on Usage
unique_ip_counts = session.manager("status/telemetry_counts/source_ip_hash")
source_location_counts = session.manager("status/telemetry_counts/source_ip_location")
client_counts = session.manager("status/telemetry_counts/client")
endpoint_counts = session.manager("status/telemetry_counts/endpoint")
telemetry_status_code_counts = session.manager("status/telemetry_counts/status_code")
alert_status_code_counts = session.manager("status/alert_counts/status_code")

##############################
# Process Statistics
##############################

# Process Request Counts
total_requests = sum_counts(endpoint_counts)
total_icesat2_granules = sum_counts(endpoint_counts, ['atl03x', 'atl03s', 'atl03v', 'atl06', 'atl06s', 'atl08', 'atl13s', 'atl24x'])
total_icesat2_proxied_requests = sum_counts(endpoint_counts, ['atl03x', 'atl03sp', 'atl03vp', 'atl06p', 'atl06sp', 'atl08p', 'atl13sp', 'atl24x'])
total_gedi_granules = sum_counts(endpoint_counts, ['gedi01b', 'gedi02a', 'gedi04a'])
total_gedi_proxied_requests = sum_counts(endpoint_counts, ['gedi01bp', 'gedi02ap', 'gedi04ap'])

##############################
# Report Statistics
##############################

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
