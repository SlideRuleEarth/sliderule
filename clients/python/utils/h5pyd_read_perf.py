#
# Read ICESat-2 data using 'h5pyd' package and characterize read performance
#
# Prerequisites:    1. 'h5pyd' installed in local python environment
#                   2. HSDS server running and accessible to python client
#                   3. the hardcoded 'filepath' and 'filename' must be available to HSDS
#

import sys
import time
import h5pyd

timetable = {}
timetotal = 0

def printdata (file, dataset):
    data = file[dataset]
    print(data[0], data[1], data[2])

def timedata (file, dataset):
    global timetable, timetotal
    print("Reading " + dataset + ": ", end = '')
    start = time.time()
    datatotal = sum(file[dataset])
    end = time.time()
    duration = end - start
    timetable[dataset] = duration
    timetotal += duration
    print(duration, datatotal)

def timetrackdata (file, dataset):
    timedata(file, "/gt1l" + dataset)
    timedata(file, "/gt1r" + dataset)
    timedata(file, "/gt2l" + dataset)
    timedata(file, "/gt2r" + dataset)
    timedata(file, "/gt3l" + dataset)
    timedata(file, "/gt3r" + dataset)

if __name__ == '__main__':

    filepath = "/hsds/ATLAS/"
    filename = "ATL03_20181019065445_03150111_003_01.h5"
    test = "read"

    # Override test
    if len(sys.argv) > 1:
        test = sys.argv[1]

    # Override filename
    if len(sys.argv) > 2:
        filename = sys.argv[2]

    # Override filepath
    if len(sys.argv) > 3:
        filepath = sys.argv[3]

    # Open file
    print("Opening file: " + filepath + filename)
    h5file = h5pyd.File(filepath + filename, "r")

    # Test: Read
    if test == "read":
        printdata(h5file, "/gt1r/geolocation/segment_ph_cnt")
    elif test == "perf":
        timedata(h5file, "/ancillary_data/atlas_sdp_gps_epoch")
        timedata(h5file, "/orbit_info/sc_orient")
        timedata(h5file, "/ancillary_data/start_rgt")
        timedata(h5file, "/ancillary_data/end_rgt")
        timedata(h5file, "/ancillary_data/start_cycle")
        timedata(h5file, "/ancillary_data/end_cycle")
        timetrackdata(h5file, "/geolocation/delta_time")
        timetrackdata(h5file, "/geolocation/segment_ph_cnt")
        timetrackdata(h5file, "/geolocation/segment_id")
        timetrackdata(h5file, "/geolocation/segment_dist_x")
        timetrackdata(h5file, "/geolocation/reference_photon_lat")
        timetrackdata(h5file, "/geolocation/reference_photon_lon")
        timetrackdata(h5file, "/heights/dist_ph_along")
        timetrackdata(h5file, "/heights/h_ph")
        timetrackdata(h5file, "/heights/signal_conf_ph")
        timetrackdata(h5file, "/bckgrd_atlas/delta_time")
        timetrackdata(h5file, "/bckgrd_atlas/bckgrd_rate")
        print("TOTAL: " + str(timetotal))


