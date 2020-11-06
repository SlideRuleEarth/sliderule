#
# Imports
#
import sys
import logging
import pandas as pd
import matplotlib.pyplot as plt
import cartopy
import icesat2

grand_mesa = [ {"lon": -108.3435200747503, "lat": 38.89102961045247},
               {"lon": -107.7677425431139, "lat": 38.90611184543033}, 
               {"lon": -107.7818591266989, "lat": 39.26613714985466},
               {"lon": -108.3605610678553, "lat": 39.25086131372244},
               {"lon": -108.3435200747503, "lat": 38.89102961045247} ]

#
# SlideRule Processing Request
#
def algoexec(asset):                

    # Build ATL06 Request
    parms = {
        "poly": grand_mesa,
        "srt": icesat2.SRT_LAND,
        "cnf": icesat2.CNF_SURFACE_HIGH,
        "ats": 10.0,
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
def plotresults(df):
    # Create Plot
    fig = plt.figure(num=None, figsize=(12, 6))
    box_lon = [e["lon"] for e in grand_mesa]
    box_lat = [e["lat"] for e in grand_mesa]

    # Plot Ground Tracks
    ax1 = plt.subplot(121,projection=cartopy.crs.PlateCarree())
    ax1.set_title("Zoomed Ground Tracks")
    ax1.scatter(df["longitude"].values, df["latitude"].values, s=2.5, c=df["h_mean"], cmap='winter_r', zorder=3, transform=cartopy.crs.PlateCarree())
    ax1.set_extent((-108.4,-107.7,38.8,39.3),crs=cartopy.crs.PlateCarree())
    ax1.plot(box_lon, box_lat, linewidth=1.5, color='b', zorder=2, transform=cartopy.crs.Geodetic())

    # Plot Global View
    ax2 = plt.subplot(122,projection=cartopy.crs.PlateCarree())
    ax2.set_title("Global Ground Tracks")
    ax2.scatter(df["longitude"].values, df["latitude"].values, s=2.5, color='r', zorder=3, transform=cartopy.crs.PlateCarree())
    ax2.add_feature(cartopy.feature.LAND, zorder=0, edgecolor='black')
    ax2.add_feature(cartopy.feature.LAKES)
    ax2.set_extent((-180,180,-90,90),crs=cartopy.crs.PlateCarree())

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
    df = algoexec(atl03_asset)

    # Plot Results
    plotresults(df)