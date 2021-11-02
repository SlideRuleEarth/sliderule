# python

import sys
import srpybin

###############################################################################
# DATA
###############################################################################
""
# set resource parameters
resource0   = "UW_OSD.h5"
resource1   = "ATL06_20200714160647_02950802_003_01.h5"
resource2   = "ATL06_20181019065445_03150111_003_01.h5"
format      = "file"        # "s3"
path        = "/data/ATLAS" # "icesat2-sliderule/data/ATLAS"
region      = ""            # "us-west-2"
endpoint    = ""            # "https://s3.us-west-2.amazonaws.com"

# expected small read
small_exp = [471, -1444, 1131, -258, 247]

# expected single read
h_li_exp_1 = [3432.17578125, 3438.776611328125, 3451.01123046875, 3462.688232421875, 3473.559326171875]

# expected parallel read
h_li_exp_2 = {  '/gt1l/land_ice_segments/h_li': [3432.17578125, 3438.776611328125, 3451.01123046875, 3462.688232421875, 3473.559326171875],
                '/gt2l/land_ice_segments/h_li': [3263.659912109375, 3258.362548828125, 3.4028234663852886e+38, 3233.031494140625, 3235.200927734375],
                '/gt3l/land_ice_segments/h_li': [3043.489013671875, 3187.576171875, 3.4028234663852886e+38, 4205.04248046875, 2924.724365234375]}

# expected negative read
bsnow_conf_exp_3 = [-1, -1, -1, -1, -1]

###############################################################################
# UTILITY FUNCTIONS
###############################################################################

def check_results(act, exp):
    if type(exp) == dict:
        for dataset in exp:
            for i in range(len(exp[dataset])):
                if exp[dataset][i] != act[dataset][i]:
                    return False
        return True
    else:
        for i in range(len(exp)):
            if exp[i] != act[i]:
                return False
        return True

###############################################################################
# MAIN
###############################################################################

if __name__ == '__main__':

    result = True

    # Open H5 File #
    asdf = srpybin.h5coro(resource0, format, "/data/ASDF", region, endpoint)
    
    # Run Meta Test #
    meta = asdf.meta("/Waveforms/UW.OSD/UW.OSD..EHZ__2020-01-01T00:00:00__2020-01-01T05:13:04__raw_recording")
    result = result and (meta['elements'] == 1878490)
    result = result and (meta['typesize'] == 4)
    result = result and (meta['datasize'] == 7513960)
    result = result and (meta['datatype'] == 'INT32')
    result = result and (meta['numcols'] == 1)
    result = result and (meta['numrows'] == 1878490)

    # Read Data from Dataset #
    v = asdf.read("/Waveforms/UW.OSD/UW.OSD..EHZ__2020-01-01T00:00:00__2020-01-01T05:13:04__raw_recording", 0, 57655, 5)
    result = result and check_results(v, small_exp)

    # Read Large Dataset Multiple Times #
    for test in range(10):
        v = asdf.read("/Waveforms/UW.OSD/UW.OSD..EHZ__2020-01-01T00:00:00__2020-01-01T05:13:04__raw_recording")

    # Open H5 File #
    h5file1 = srpybin.h5coro(resource1, format, path, region, endpoint)

    # Run Meta Test #
    h_li_meta = h5file1.meta("/gt1l/land_ice_segments/h_li")
    result = result and (h_li_meta['elements'] == 3563)
    result = result and (h_li_meta['typesize'] == 4)
    result = result and (h_li_meta['datasize'] == 14252)
    result = result and (h_li_meta['datatype'] == 'FLOAT')
    result = result and (h_li_meta['numcols'] == 1)
    result = result and (h_li_meta['numrows'] == 3563)

    # Run Single and Parallel Tests #
    for test in range(100000):
        if test % 100 == 0:
            sys.stdout.write(".")
            sys.stdout.flush()

        # Perform Single Read #
        h_li_1 = h5file1.read("/gt1l/land_ice_segments/h_li", 0, 19, 5)
        result = result and check_results(h_li_1, h_li_exp_1)

        # Perform Parallel Read #
        datasets = [["/gt1l/land_ice_segments/h_li", 0, 19, 5],
                    ["/gt2l/land_ice_segments/h_li", 0, 19, 5],
                    ["/gt3l/land_ice_segments/h_li", 0, 19, 5],
                    ["/gt2r/land_ice_segments/geophysical/bsnow_conf", 0, 19, 5]]
        h_li_2 = h5file1.readp(datasets)
        result = result and check_results(h_li_2, h_li_exp_2)

        # check result for early exit from loop
        if not result:
            break

    # Run Negative Test #
    h5file2 = srpybin.h5coro(resource2, format, path, region, endpoint)
    bsnow_conf = h5file2.read("/gt2r/land_ice_segments/geophysical/bsnow_conf", 0, 19, 5)
    result = result and check_results(bsnow_conf, bsnow_conf_exp_3)

    # Display Results #
    if result:
        print("\nPassed H5Coro Test")
    else:
        print("\nFailed H5Coro Test")
