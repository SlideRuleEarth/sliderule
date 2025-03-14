# python

import srpybin
import requests

###############################################################################
# UTILITY FUNCTIONS
###############################################################################

# gets s3 credentials for daac
def get_s3credentials(daac: str):
    earthadata_s3 = f"https://data.{daac}.earthdata.nasa.gov/s3credentials"
    r = requests.get(earthadata_s3)
    r.raise_for_status()
    return r.json()

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

    # checking length of data read
    print("checking length of datasets read...")
    assert len(data["/BEAM0000/lat_lowestmode"]) == num_footprints
    assert len(data["/BEAM0000/lon_lowestmode"]) == num_footprints
    assert len(data["/BEAM0000/agbd"]) == num_footprints

    # checking values
    print("checking first 32 values of /BEAM0000/agbd...")
    expected_values = [ 11.883420944213867, 11.978496551513672, 16.00865364074707, 17.996280670166016, 14.123430252075195, -9999.0,
                        26.816543579101562, 13.037765502929688, 64.72089385986328, 169.4375762939453, 127.10370635986328,
                        94.90692138671875, 23.459997177124023, 18.710899353027344, 105.9470443725586, 188.6793670654297,
                        276.2403259277344, 196.25143432617188, 132.2151336669922, 113.12105560302734, 205.57577514648438,
                        62.28338623046875, 69.27841186523438, 48.490386962890625, 114.13134765625, 205.54757690429688,
                        101.64289855957031, 74.65874481201172, 86.33869934082031, 83.43904113769531, -9999.0, 29.942535400390625 ]
    for act,exp in zip(data["/BEAM0000/agbd"][:32], expected_values):
        assert abs(act - exp) < 0.0001

    # complete
    print("script complete")
