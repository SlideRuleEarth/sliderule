local console = require("console")
local earthdata = require("earth_data_query")
local asset = require("asset")
local json = require("json")

-- get command line parameters
local resource = arg[1] or "ATL03_20190604044922_10220307_006_02.h5"

-- area of interest
local poly = {
    {lat = 0.73734824791566,    lon = 172.51715474926},
    {lat = 0.73734824791566,    lon = 173.47569646684},
    {lat = 2.1059226076334,     lon = 173.47569646684},
    {lat = 2.1059226076334,     lon = 172.51715474926},
    {lat = 0.73734824791566,    lon = 172.51715474926}
}

-- request parameters
local parms = {
    asset = "icesat2",
    poly = poly,
    srt = icesat2.SRT_DYNAMIC,
    cnf = "atl03_not_considered",
    pass_invalid = true,
    generate_ndwi = true,
    use_bathy_mask = true,
    beam_file_prefix = "/tmp/bathy_"
}

-- configure logging
console.monitor:config(core.LOG, core.INFO)
sys.setlvl(core.LOG, core.INFO)

-- load asset directory
local assets = asset.loaddir()
local nsidc_s3 = core.getbyname(parms["asset"])

-- get credentials for icesat2 asset
local nsidc_parms = {earthdata="https://data.nsidc.earthdatacloud.nasa.gov/s3credentials", identity="nsidc-cloud"}
local nsidc_script = core.script("earth_data_auth", json.encode(nsidc_parms)):name("NsidcAuthScript")
while not aws.csget("nsidc-cloud") do
    print("Waiting to authenticate to NSIDC...")
    sys.wait(1)
end

-- get credentials for icesat2 asset
local lpdaac_parms = {earthdata="https://data.lpdaac.earthdatacloud.nasa.gov/s3credentials", identity="lpdaac-cloud"}
local lpdaac_script = core.script("earth_data_auth", json.encode(lpdaac_parms))
while not aws.csget("lpdaac-cloud") do
    print("Waiting to authenticate to LPDAAC...")
    sys.wait(1)
end

-- atl09 granules
local rc, rsps = earthdata.search({ poly = poly, asset = "icesat2-atl09" })
if rc == earthdata.SUCCESS then parms["resource09"] = rsps end

-- build hls raster object
local year      = resource:sub(7,10)
local month     = resource:sub(11,12)
local day       = resource:sub(13,14)
local rdate     = string.format("%04d-%02d-%02dT00:00:00Z", year, month, day)
local rgps      = time.gmt2gps(rdate)
local rdelta    = 5 * 24 * 60 * 60 * 1000 -- 5 days * (24 hours/day * 60 minutes/hour * 60 seconds/minute * 1000 milliseconds/second)
local t0        = string.format('%04d-%02d-%02dT%02d:%02d:%02dZ', time.gps2date(rgps - rdelta))
local t1        = string.format('%04d-%02d-%02dT%02d:%02d:%02dZ', time.gps2date(rgps + rdelta))
local hls_parms = {
    asset = "landsat-hls",
    t0 = t0,
    t1 = t1,
    use_poi_time = true,
    bands = {"NDWI"},
    poly = poly
}
rc, rsps = earthdata.stac(hls_parms)
hls_parms["catalog"] = (rc == earthdata.SUCCESS) and json.encode(rsps) or nil
local hls_parms_obj = geo.parms(hls_parms)

-- create bathy reader
local starttime = time.latch();
local bathy_parms = icesat2.bathyparms(parms)
local reader = icesat2.bathyreader(nsidc_s3, resource, "resultq", bathy_parms, hls_parms_obj, false)

-- wait until bathy reader completes
local timeout = parms["node_timeout"] or parms["timeout"] or core.NODE_TIMEOUT
local duration = 0
local interval = 10 < timeout and 10 or timeout -- seconds
while not reader:waiton(interval * 1000) do
    duration = duration + interval
    if timeout >= 0 and duration >= timeout then
        sys.log(core.ERROR, string.format("bathy reader for %s timed-out after %d seconds", resource, duration))
        do return false end
    end
    sys.log(core.INFO, string.format("bathy reader continuing to read %s (after %d seconds)", resource, duration))
end

local stoptime = time.latch();
local dtime = stoptime - starttime
print(string.format("\nBathyReader run time: %.4f", dtime))


-- exit
sys.quit()