import sys
import time
import copy
import json
import numpy as np
import matplotlib.pyplot as plt
import srpybin as sliderule

# Command Line Option - context
context = "local"
if len(sys.argv) > 1:
    context = sys.argv[1]

# Command Line Option - credentials
credential_file = ".credentials"
if len(sys.argv) > 2:
    credential_file = sys.argv[2]

# Parameters
num_runs = 3

# Open H5 Files
if context == "local":
    # Create H5 Handles
    atl08 = sliderule.h5coro("ATL08_20181214194017_11790102_005_01.h5", "file", "/data/ATLAS", "", "")
    atl03 = sliderule.h5coro("ATL03_20181214194017_11790102_005_01.h5", "file", "/data/ATLAS", "", "")
elif context == "remote":
    # Setup Credentials
    with open(credential_file, "r") as file:
        my_creds = json.load(file)
    nsidc_creds = sliderule.credentials("cumulus://nsidc-cumulus-prod-protected")
    nsidc_creds.provide(my_creds)
    # Create H5 Handles
    atl08 = sliderule.h5coro("ATL08_20181214194017_11790102_005_01.h5", "cumulus", "nsidc-cumulus-prod-protected", "us-west-2", "https://s3.us-west-2.amazonaws.com")
    atl03 = sliderule.h5coro("ATL03_20181214194017_11790102_005_01.h5", "cumulus", "nsidc-cumulus-prod-protected", "us-west-2", "https://s3.us-west-2.amazonaws.com")

# Build Dictionary of Statistics
stats = {
    "pre_prefetch_request": [],
    "post_prefetch_request": [],
    "cache_miss": [],
    "l1_cache_replace": [],
    "l2_cache_replace": [],
    "bytes_read": []
}

# Build Dictionaries for each Test Group
testGroup = {
    "atl08": {
        "dataset": ["%s/%s" % (gt, ds) for gt in ["gt1l", "gt2l", "gt3l"] for ds in ["signal_photons/ph_segment_id", "signal_photons/classed_pc_indx", "signal_photons/classed_pc_flag"]],
        "file": atl08,
        "stats": copy.deepcopy(stats),
        "duration": []
    },
    "atl03p": {
        "dataset": ["%s/%s" % (gt, ds) for gt in ["gt1l", "gt2l", "gt3l"] for ds in ["geolocation/delta_time", "geolocation/segment_id", "geolocation/segment_dist_x"]],
        "file": atl03,
        "stats": copy.deepcopy(stats),
        "duration": []
    },
    "atl03s": {
        "dataset": ["%s/%s" % (gt, ds) for gt in ["gt1l", "gt2l", "gt3l"] for ds in ["heights/dist_ph_along", "heights/h_ph", "heights/delta_time"]],
        "file": atl03,
        "stats": copy.deepcopy(stats),
        "duration": []
    }
}

# Run Performance Test
for idx in range(num_runs):
    for test in testGroup:
        for dataset in testGroup[test]["dataset"]:
            print("Test %s/%d: reading dataset %s" % (test, idx, dataset))
            h5file = testGroup[test]["file"]

            # Perform H5Coro Read
            stat_0 = h5file.stat()
            time_0 = time.perf_counter()
            _ =  h5file.read(dataset, 0, 0, -1)
            time_1 = time.perf_counter()
            stat_1 = h5file.stat()

            # Accumulate Statistics
            testGroup[test]["duration"].append(time_1 - time_0)
            for stat in testGroup[test]["stats"]:
                testGroup[test]["stats"][stat].append(stat_1[stat] - stat_0[stat])

# Plot Results
fig = plt.figure(figsize = (14, 15))
for index, test in enumerate(testGroup):
    plt.subplots_adjust(hspace = 0.2)

    plt.subplot(5, 1, 1)
    plt.plot(testGroup[test]["duration"], linewidth=2, label=test, zorder=index)
    plt.ylabel("Reading time [s]", fontsize=16)
    plt.legend(loc = "upper right", fontsize=14)
    plt.grid(True)

    plt.subplot(5, 1, 2)
    plt.plot(np.array(testGroup[test]["stats"]["cache_miss"]), linewidth=2, label=test, zorder=index)
    plt.ylabel("Cache Misses", fontsize=16)
    plt.legend(loc = "upper right", fontsize=14)
    plt.grid(True)

    plt.subplot(5, 1, 3)
    plt.plot(np.array(testGroup[test]["stats"]["pre_prefetch_request"]), linewidth=2, label=test, zorder=index)
    plt.ylabel("Pre-Prefetch", fontsize=16)
    plt.legend(loc = "upper right", fontsize=14)
    plt.grid(True)

    plt.subplot(5, 1, 4)
    plt.plot(np.array(testGroup[test]["stats"]["post_prefetch_request"]), linewidth=2, label=test, zorder=index)
    plt.ylabel("Post-Prefetch", fontsize=16)
    plt.legend(loc = "upper right", fontsize=14)
    plt.grid(True)

    plt.subplot(5, 1, 5)
    plt.plot(np.array(testGroup[test]["stats"]["bytes_read"]) / 1e6, linewidth=2, label=test, zorder=index)
    plt.ylabel("Bytes Read [MB]", fontsize=16)
    plt.legend(loc = "upper right", fontsize=14)
    plt.grid(True)

plt.savefig('perfreport.pdf')
