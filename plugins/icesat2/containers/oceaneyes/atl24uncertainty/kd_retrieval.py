#!/usr/bin/env python
# -*- coding: utf-8 -*-
# -----------------------------------------------------------------------------
# MIT License
# 
# Copyright (c) 2024 James Dietrich
# 
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
# 
# The above copyright notice and this permission notice shall be included in all
# copies or substantial portions of the Software.
# 
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
# SOFTWARE.
# -----------------------------------------------------------------------------
# 6/5/24:   the code below was modified from its original form by @jpswinski
#           in the following ways: 
#               - only the get_kd_values function was kept and its dependencies
#               - removed all but the necessary imports
#
#           for the original source code see the repository at:
#               https://github.com/geojames/ICESat2_tidibts.git           
# -----------------------------------------------------------------------------
__author__ = 'James T. Dietrich'
__contact__ = 'james.dietrich@austin.utexas.edu'
__copyright__ = '(c) James Dietrich 2022'
__license__ = 'MIT'
__date__ = '2022-Nov-01'
__version__ = '0.1'
__status__ = "initial release"
__url__ = "..."

"""
Name:           ManualLabel_EnvStats_ATL24.py.py
Compatibility:  Python 3.11
Description:    A description of your program

Requires:       Modules/Libraries required

Dev ToDo:       1)

AUTHOR:         James T. Dietrich
ORGANIZATION:   University of Texas @ Austin
                3D Geospatial Laboratory
                Center for Space Research
Contact:        james.dietrich@austin.utexas.edu
"""
#%%

import pandas as pd
import warnings
from datetime import datetime as dt
import requests
import os

warnings.filterwarnings("ignore")
     

#%%

def path_fix(folder):
    
    if (folder[-1] == '/') or (folder[-1] == '\\'):
        folder = folder[:-1]
        
    return folder

def get_kd_values(date, outpath = 'c:/temp', keep=False):
    '''
    Downloads an 8-day NOAA20 VIIRS Kd490 image from NASA OceanColor

    Parameters
    ----------
    date : string data from ATL-03 file "%Y-%m-%d_%H%MZ"
        suggested 
        start_dt_obj = dt.strptime(h5['ancillary_data/data_start_utc'][0].decode(),
                                           "%Y-%m-%dT%H:%M:%S.%fZ")
        atl03_date = dt.strftime(start_dt_obj,"%Y-%m-%d_%H%MZ")
    out : file path
        file path for output of the KD images. The default is 'c:/temp'.
    keep : boolean, optional
        True = files will be kept and stored with their original filename.
        False = files will be overwritten with the filename kd_490.nc
        The default is False.

    Returns
    -------
    kd_out = file path to the downloaded KD image

    '''
    
    # check outpath for trailing slash
    kd_out = path_fix(outpath)
    
    # OpenDAP file structure:
    # http://oceandata.sci.gsfc.nasa.gov/opendap/VIIRSJ1/L3SMI/2019/0101/JPSS1_VIIRS.20190101_20190108.L3m.8D.KD.Kd_490.4km.nc.dap.nc4

    # get start datetime from the granule, establish the 8-day cycle start dates for
    #   the year, generate the time deltas to the granule date
    h5_date = dt.strptime(date,"%Y-%m-%d_%H%MZ")
    eight_day = pd.date_range(start='1/1/%s'%h5_date.year, end='12/31/%s'%h5_date.year,freq='8d')
    td = eight_day - h5_date

    # calculate the closest starting point of the 8-day cycle based on the
    #   granule time - infer the ending point +7 days
    dl_start = eight_day[td[td<=pd.Timedelta(0)].argmax()]
    dl_end = dl_start + pd.Timedelta('7d')
    
    if dl_end.year != dl_start.year:
        dl_end = dt.strptime("%s-12-31"%dl_start.year,"%Y-%m-%d")

    # build the download string
    oc_start = 'http://oceandata.sci.gsfc.nasa.gov/opendap/VIIRSJ1/L3SMI/%s/%s/'%(dl_start.year,dl_start.strftime('%m%d'))
    oc_dates = 'JPSS1_VIIRS.%s_%s'%(dl_start.strftime('%Y%m%d'), dl_end.strftime('%Y%m%d'))
    oc_end = ".L3m.8D.KD.Kd_490.4km.nc"
    dap_end = '.dap.nc4'
    oc_file = oc_start + oc_dates + oc_end + dap_end

    # set the file_put path and filename
    if keep:
        file_out = kd_out + "/" + oc_dates + oc_end
    else:
        file_out = kd_out + "/kd_490_temp.nc"

    # quick check to see if file already exists
    if os.path.exists(file_out):
        return (file_out, True)

    # submit download request, if OK (no errors) write the file out
    #   if no file, kill everything

    r = requests.get(oc_file)

    if r.ok:
        with open(file_out,'wb') as f:
          
            # write the contents of the response (r.content)
            # to a new file in binary mode.
            f.write(r.content)
            
            return(file_out, r.ok)
    else:
        print('* Trying NRT *')
        oc_end = ".L3m.8D.KD.Kd_490.4km.NRT.nc"
        dap_end = '.dap.nc4'
        oc_file = oc_start + oc_dates + oc_end + dap_end
        r = requests.get(oc_file)
        
        if r.ok:
            with open(file_out,'wb') as f:
              
                # write the contents of the response (r.content)
                # to a new file in binary mode.
                f.write(r.content)
                
                return(file_out, r.ok)
        else:
            print("unable to download file %s | Status: %i, %s"%(oc_dates + oc_end + dap_end,
                                                                 r.status_code, r.reason))
            return(file_out, r.ok)
