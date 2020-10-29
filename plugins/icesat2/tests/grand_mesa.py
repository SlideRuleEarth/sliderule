#
# Imports
#
import sys
import logging
import pandas as pd
import cartopy
import icesat2

#
# SlideRule Processing Request
#
def algoexec(asset):                

    # Build ATL06 Request
    parms = {
        "poly": [ {"lon": -108.3435200747503, "lat": 38.89102961045247},
                  {"lon": -107.7677425431139, "lat": 38.90611184543033}, 
                  {"lon": -107.7818591266989, "lat": 39.26613714985466},
                  {"lon": -108.3605610678553, "lat": 39.25086131372244},
                  {"lon": -108.3435200747503, "lat": 38.89102961045247} ],
        "cnf": 4,
        "ats": 20.0,
        "cnt": 10,
        "len": 40.0,
        "res": 20.0,
        "maxi": 1
    }

    # Request ATL06 Data
    rsps = icesat2.atl06p(parms, asset)

    # Build Dataframe of SlideRule Responses
    df = pd.DataFrame(data=list(zip(rsps["h_mean"], rsps["lat"], rsps["lon"], rsps["spot"])), index=rsps["segment_id"], columns=["h_mean", "latitude", "longitude", "spot"])

    # Return DataFrame
    print("Reference Ground Tracks: {} to {}".format(min(rsps["rgt"]), max(rsps["rgt"])))
    print("Cycle: {} to {}".format(min(rsps["cycle"]), max(rsps["cycle"])))
    print("Retrieved {} points from SlideRule".format(len(rsps["h_mean"])))
    return df

#
# Plot Results
#
def plotresults(rsps):
    # Create Plot
    fig = plt.figure(num=None, figsize=(12, 6))

    # Plot Ground Tracks
    ax1 = plt.subplot(121,projection=cartopy.crs.PlateCarree())
    ax1.set_title("Ground Tracks")
    ax1.plot(rsps["longitude"].values, rsps["latitude"].values, linewidth=1.5, color='r', zorder=3, transform=cartopy.crs.Geodetic())
    ax1.add_feature(cartopy.feature.LAND, zorder=0, edgecolor='black')
    ax1.add_feature(cartopy.feature.LAKES)
    ax1.set_extent((-110,-106,37,41),crs=cartopy.crs.PlateCarree())

    # Show Plot
    plt.show()

###############################################################################
# MAIN
###############################################################################

if __name__ == '__main__':

    # configure logging
    logging.basicConfig(level=logging.INFO)

    # Set URL #
    url = "http://127.0.0.1:9081"
    if len(sys.argv) > 1:
        url = sys.argv[1]

    # Set ATL03 Asset #
    atl03_asset = "atl03-local"
    if len(sys.argv) > 2:
        atl03_asset = sys.argv[2]

    # Initialize Icesat2 Package #
    icesat2.init(url, False)

    # Execute SlideRule Algorithm
    rsps = algoexec(atl03_asset)

    # Plot Results
    plotresults(rsps)