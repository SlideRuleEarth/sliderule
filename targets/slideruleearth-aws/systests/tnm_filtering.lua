local earthdata = require("earth_data_query")
local prettyprint = require("prettyprint")

local console = require("console")
console.monitor:config(core.INFO)
sys.setlvl(core.LOG, core.INFO)

local poly = {
    {lon = -108.6083,   lat = 33.3583},
    {lon = -108.5416,   lat = 33.3583},
    {lon = -105.5500,   lat = 35.5500},
    {lon = -105.5083,   lat = 35.5833},
    {lon = -103.9916,   lat = 36.9166},
    {lon = -103.9916,   lat = 44.5000},
    {lon = -108.4000,   lat = 47.9833},
    {lon = -108.4250,   lat = 48.0000},
    {lon = -109.3500,   lat = 48.3250},
    {lon = -111.1333,   lat = 48.9500},
    {lon = -111.1833,   lat = 48.9666},
    {lon = -111.5583,   lat = 49.0000},
    {lon = -124.5500,   lat = 49.0000},
    {lon = -124.6500,   lat = 48.8166},
    {lon = -123.8166,   lat = 41.7416},
    {lon = -123.7666,   lat = 41.5666},
    {lon = -123.6000,   lat = 41.0916},
    {lon = -123.2833,   lat = 40.3833},
    {lon = -123.1500,   lat = 40.0916},
    {lon = -123.1416,   lat = 40.0750},
    {lon = -123.0000,   lat = 39.8333},
    {lon = -122.8916,   lat = 39.6750},
    {lon = -122.8416,   lat = 39.6083},
    {lon = -117.0750,   lat = 34.2000},
    {lon = -108.6083,   lat = 33.3583}
}

local parms = {
    asset = 'usgs3dep-1meter-dem',
    t0 = '2022-10-01',
    t1 = '2023-09-30',
    max_resources = 999999
}

local status, catalog = earthdata.tnm(parms, poly)
if status == earthdata.SUCCESS then
    print("Number of features: ", #catalog["features"])
else
    print("Failed request: ", catalog)
end
