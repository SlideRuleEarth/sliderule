local console = require("console")
local asset = require("asset")
local json = require("json")
local pp = require("prettyprint")

-- Parameters --

local asset_name = arg[1] or "nsidc-s3"
local resource = arg[2] or "ATL03_20181015231931_02650102_005_01.h5"
local dataset = arg[3] or "ancillary_data/atlas_sdp_gps_epoch"

-- Setup --

local assets = asset.loaddir() -- looks for asset_directory.csv in same directory this script is located in
local atlas_asset = core.getbyname(asset_name)

local creds = aws.csget(asset_name)
if not creds then
    local earthdata_url = "https://data.nsidc.earthdatacloud.nasa.gov/s3credentials"
    local response, _ = netsvc.get(earthdata_url)
    local _, credential = pcall(json.decode, response)
    aws.csput(asset_name, credential)
end

-- Read File --

local f = h5.file(atlas_asset, resource):name(resource)
local rspq = msg.subscribe("h5q")

f:read({{dataset=dataset}}, "h5q")

local recdata = rspq:recvrecord(3000)
local rectable = recdata:tabulate()

pp.display(rectable)

rspq:destroy()
f:destroy()
