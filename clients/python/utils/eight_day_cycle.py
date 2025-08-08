from datetime import datetime as dt
import pandas as pd
import requests
import sys


# OpenDAP file structure:
# http://oceandata.sci.gsfc.nasa.gov/opendap/VIIRSJ1/L3SMI/2019/0101/JPSS1_VIIRS.20190101_20190108.L3m.8D.KD.Kd_490.4km.nc.dap.nc4

years = ['2018', '2019', '2020', '2021', '2022', '2023', '2024']

for year in years:
    # get start datetime from the granule, establish the 8-day cycle start dates for
    #   the year, generate the time deltas to the granule date
    eight_day = pd.date_range(start='1/1/%s'%year, end='12/31/%s'%year,freq='8d')
    for dl_start in eight_day:
        dl_end = dl_start + pd.Timedelta('7d')
        if dl_end.year != dl_start.year:
            dl_end = dt.strptime("%s-12-31"%dl_start.year,"%Y-%m-%d")

        # build the download string
        oc_start = 'http://oceandata.sci.gsfc.nasa.gov/opendap/VIIRSJ1/L3SMI/%s/%s/'%(dl_start.year,dl_start.strftime('%m%d'))
        oc_dates = 'JPSS1_VIIRS.%s_%s'%(dl_start.strftime('%Y%m%d'), dl_end.strftime('%Y%m%d'))
        oc_end = ".L3m.8D.KD.Kd_490.4km.nc"
        dap_end = '.dap.nc4'
        oc_file = oc_start + oc_dates + oc_end + dap_end

        print(oc_file)


        r = requests.get(oc_file)

        if r.ok:
            with open("test.out",'wb') as f:
            
                # write the contents of the response (r.content)
                # to a new file in binary mode.
                f.write(r.content)                
        sys.exit(0)
        