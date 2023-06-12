# python

import srpybin
import requests
try:
    import matplotlib.pyplot as plt
    __with_plotting = True
except:
    __with_plotting = False

###############################################################################
# UTILITY FUNCTIONS
###############################################################################

# gets s3 credentials for daac
def get_s3credentials(daac: str):
    earthadata_s3 = f"https://data.{daac}.earthdata.nasa.gov/s3credentials"
    r = requests.get(earthadata_s3)
    r.raise_for_status()
    return r.json()

# plot data
def plot_agbd(data):
    fig,ax = plt.subplots(1, 1, figsize=(24, 12))
    ax.plot([x for x in range(len(data))], data, color='g')
    plt.show()

###############################################################################
# MAIN
###############################################################################

if __name__ == '__main__':

    # setup logging
    logger = srpybin.logger(srpybin.INFO)

    # parameters
    asset = "gedil4a"
    identity = "ornldaac"
    driver = "s3"
    resource = "GEDI04_A_2019229131935_O03846_02_T03642_02_002_02_V002.h5"
    path = "ornl-cumulus-prod-protected/gedi/GEDI_L4A_AGB_Density_V2_1/data"
    region = "us-west-2"
    endpoint = "https://s3.us-west-2.amazonaws.com"
    start_footprint = 93000
    num_footprints = 1000
    #            dataset,                     col,       start_row,       num_rows
    datasets = [["/BEAM0000/lat_lowestmode",    0, start_footprint, num_footprints],
                ["/BEAM0000/lon_lowestmode",    0, start_footprint, num_footprints],
                ["/BEAM0000/agbd",              0, start_footprint, num_footprints]]

    # credentials
    print("fetching credentials...")
    s3credentials = get_s3credentials("ornldaac")
    srcredentials = srpybin.credentials(identity)
    srcredentials.provide({"accessKeyId":       s3credentials['accessKeyId'],
                          "secretAccessKey":    s3credentials['secretAccessKey'],
                          "sessionToken":       s3credentials['sessionToken'],
                          "expiration":         s3credentials["expiration"]})

    # perform read
    print("performing dataset read...")
    h5file = srpybin.h5coro(asset, resource, identity, driver, path, region, endpoint)
    data = h5file.readp(datasets)

    # display
    if __with_plotting:
        print("plotting data...")
        plot_agbd(data["/BEAM0000/agbd"])
    else:
        print("printing first 32 values of /BEAM0000/agbd ...")
        print(data["/BEAM0000/agbd"][:32])

    # complete
    print("script complete")

