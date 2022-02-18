#
# Imports
#
import sys
import json
import base64
import geopandas
import folium
from sliderule import icesat2
from sliderule import sliderule

###############################################################################
# MAIN
###############################################################################

if __name__ == '__main__':

    # Make Call to TIFF Endpoint #
    filename = sys.argv[1]
    region = icesat2.toregion(filename)
    icesat2.init(["127.0.0.1"], False)
    rsps = sliderule.source("tiff", { "raster": region["raster"] })

    # Display Response #
    print(json.dumps(rsps, indent=4))

    # Optionally Write TIFF File #
    if "--write" in sys.argv[1:]:
        f = open("map.tiff", "wb")
        f.write(base64.b64decode(region["raster"]["image"]))
        f.close()

    # Generate Interactive Map #
    if "--interact" in sys.argv[1:]:
        pregion = geopandas.read_file(filename)
        m = folium.Map(location=[region["poly"][0]["lat"], region["poly"][0]["lon"]], zoom_start=5)
        folium.Choropleth(pregion, line_color='red', line_weight=2).add_to(m)
        m.save("map.html")

