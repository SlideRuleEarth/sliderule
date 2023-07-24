import sys

# get point of interest
lat_deg = sys.argv[1]
lat_min = sys.argv[2]
lat_hem = sys.argv[3]
lon_deg = sys.argv[4]
lon_min = sys.argv[5]
lon_hem = sys.argv[6]

# get extent of bounding box
extent_km = 100.0 / 2.0
if len(sys.argv) > 7:
    extent_km = float(sys.argv[7]) / 2.0
extent = extent_km * 90 / 10000 # convert to degrees

# remove special characters
if lat_deg[-1] == '°':
    lat_deg = lat_deg[:-1]
if lat_min[-1] == "'":
    lat_min = lat_min[:-1]
if lon_deg[-1] == '°':
    lon_deg = lon_deg[:-1]
if lon_min[-1] == "'":
    lon_min = lon_min[:-1]

# convert to numbers
lat_deg = float(lat_deg)
lat_min = float(lat_min)
lon_deg = float(lon_deg)
lon_min = float(lon_min)

# convert to decimal
lat = lat_deg + (lat_min / 60)
if lat_hem == 'S':
    lat *= -1
elif lat_hem != 'N':
    raise RuntimeError(f'Invalid hemisphere designation (only N or S allowed)')
lon = lon_deg + (lon_min / 60)
if lon_hem == 'W':
    lon *= -1
elif lon_hem != 'E':
    raise RuntimeError(f'Invalid hemisphere designation (only E or W allowed)')

# build bounding box
l_lon = lon - extent
if l_lon < -180.0:
    l_lon = 360.0 + l_lon
r_lon = lon + extent
if r_lon > 180.0:
    r_lon = -360.0 + r_lon
u_lat = lat + extent
if u_lat > 90.0:
    u_lat = 180.0 - u_lat
l_lat = lat - extent
if l_lat < -90.0:
    l_lat = -180.0 + l_lat

# build coordinate strings
coord_strs = [
    f'[ {l_lon}, {u_lat} ]', # upper left
    f'[ {l_lon}, {l_lat} ]', # lower left
    f'[ {r_lon}, {l_lat} ]', # lower right
    f'[ {r_lon}, {u_lat} ]', # upper right
    f'[ {l_lon}, {u_lat} ]'  # upper left
]

# build name string
name_str = f'{int(lat_deg)}_{int(lat_min)}_{lat_hem}__{int(lon_deg)}_{int(lon_min)}_{lon_hem}'

# build geojson string
geojson_str = """{
  "type": "FeatureCollection",
  "name": "$NAME",
  "crs": {
    "type": "name",
    "properties": { "name": "urn:ogc:def:crs:OGC:1.3:CRS84" }
  },
  "features": [ {
    "type": "Feature",
    "properties": { },
    "geometry": { "type": "Polygon", "coordinates": [ [ $COORDS ] ] }
  } ]
}""".replace("$COORDS", ", ".join(coord_strs)).replace("$NAME", name_str)

# return geojson str
print(geojson_str)
