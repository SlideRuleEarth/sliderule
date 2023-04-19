#
# This script sourced from LPDAAC git repository: https://git.earthdata.nasa.gov/projects/LPDUR/repos/daac_data_download_python
#

"""
---------------------------------------------------------------------------------------------------
 How to Access the LP DAAC Data Pool with Python
 The following Python code example demonstrates how to configure a connection to download data from
 an Earthdata Login enabled server, specifically the LP DAAC's Data Pool.
---------------------------------------------------------------------------------------------------
 Original Author: Cole Krehbiel
 Date of Version Sourced: 05/14/2020
---------------------------------------------------------------------------------------------------
"""
# Load necessary packages into Python
from netrc import netrc
import argparse
import datetime
import os
import requests

# ----------------------------------USER-DEFINED VARIABLES--------------------------------------- #
# Set up command line arguments
parser = argparse.ArgumentParser(formatter_class=argparse.ArgumentDefaultsHelpFormatter)
parser.add_argument('-dir', '--directory', required=True, help='Specify directory to save files to')
parser.add_argument('-f', '--files', required=True, help='A single granule URL, or the location of csv or textfile containing granule URLs')
args = parser.parse_args()

saveDir = args.directory  # Set local directory to download to
files = args.files        # Define file(s) to download from the LP DAAC Data Pool

# ---------------------------------SET UP WORKSPACE---------------------------------------------- #
# Create a list of files to download based on input type of files above
if files.endswith('.txt') or files.endswith('.csv'):
    fileList = open(files, 'r').readlines()  # If input is textfile w file URLs
elif isinstance(files, str):
    fileList = [files]                       # If input is a single file

# Generalize download directory
if saveDir[-1] != '/' and saveDir[-1] != '\\':
    saveDir = saveDir.strip("'").strip('"') + os.sep
    if not os.path.exists(saveDir):
        os.makedirs(saveDir)

# --------------------------------AUTHENTICATION CONFIGURATION----------------------------------- #
# Determine if netrc file exists, and if so, if it includes NASA Earthdata Login Credentials
urs = 'urs.earthdata.nasa.gov'    # Address to call for authentication
netrcDir = os.path.expanduser("~/.netrc")
netrc(netrcDir).authenticators(urs)[0]
netrc(netrcDir).authenticators(urs)[2]

# -----------------------------------------DOWNLOAD FILE(S)-------------------------------------- #
# Loop through and download all files to the directory specified above, and keeping same filenames
for f in fileList:
    file_name = os.path.join(saveDir, f.split('/')[-1].strip())
    file_date = file_name.split('_')[2]
    file_year = file_date[:4]
    file_doy = file_date[4:7]
    file_date_time = datetime.datetime.strptime('{}-{}'.format(file_year, file_doy),'%Y-%j')
    file_month = '{}'.format(file_date_time.month).rjust(2,'0')
    file_day = '{}'.format(file_date_time.day).rjust(2,'0')
    file_version = file_name.split('_')[9].split('.')[0][1:]
    file_shortname = file_name[2:10]
    file_url = 'https://e4ftl01.cr.usgs.gov/GEDI/{}.{}/{}.{}.{}/{}'.format(file_shortname, file_version, file_year, file_month, file_day, file_name[2:])
    # Create and submit request and download file
    with requests.get(file_url, verify=False, stream=True, auth=(netrc(netrcDir).authenticators(urs)[0], netrc(netrcDir).authenticators(urs)[2])) as response:
        if response.status_code != 200:
            print("Error downloading {} - {} {}".format(file_url, response, response.content))
        else:
            response.raw.decode_content = True
            content = response.raw
            with open(file_name, 'wb') as d:
                while True:
                    chunk = content.read(16 * 1024)
                    if not chunk:
                        break
                    d.write(chunk)
            print('Downloaded file: {}'.format(file_name))
