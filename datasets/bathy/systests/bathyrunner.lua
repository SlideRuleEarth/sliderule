local console = require("console")
local asset = require("asset")
local json = require("json")
local prettyprint = require("prettyprint")

-- configure logging
console.monitor:config(core.LOG, core.INFO)
sys.setlvl(core.LOG, core.INFO)

-- load asset directory
local assets = asset.loaddir()


local parms = bathy.parms()
local ptable = parms:export()
prettyprint.display(ptable)

do return end

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
}

-- get credentials for icesat2 asset
local nsidc_parms = {earthdata="https://data.nsidc.earthdatacloud.nasa.gov/s3credentials", identity="nsidc-cloud"}
local nsidc_script = core.script("earth_data_auth", json.encode(nsidc_parms)):name("NsidcAuthScript")
while not aws.csget("nsidc-cloud") do
    print("Waiting to authenticate to NSIDC...")
    sys.wait(1)
end

-- get credentials for landsat asset
local lpdaac_parms = {earthdata="https://data.lpdaac.earthdatacloud.nasa.gov/s3credentials", identity="lpdaac-cloud"}
local lpdaac_script = core.script("earth_data_auth", json.encode(lpdaac_parms)):name("LpdaacAuthScript")
while not aws.csget("lpdaac-cloud") do
    print("Waiting to authenticate to LPDAAC...")
    sys.wait(1)
end

-- exit
sys.quit()