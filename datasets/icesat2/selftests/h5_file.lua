local runner = require("test_executive")
local asset = require("asset")
local json = require("json")
local pp = require("prettyprint")

-- Requirements --

if (not sys.incloud() and not runner.isglobal()) then
    return runner.skip()
end

-- Setup --

local assets = asset.loaddir()
local asset_name = "icesat2"
local atlas_asset = core.getbyname(asset_name)
local name, identity, driver = atlas_asset:info()
local resource = "ATL03_20181015231931_02650102_005_01.h5"

local creds = aws.csget(identity)
if not creds then
    local earthdata_url = "https://data.nsidc.earthdatacloud.nasa.gov/s3credentials"
    local response, _ = core.get(earthdata_url)
    local _, credential = pcall(json.decode, response)
    aws.csput(identity, credential)
end

local f = h5.file(atlas_asset, resource):name(resource)
local rspq = msg.subscribe("h5testq")

-- Self Test --

f:read({{dataset="ancillary_data/atlas_sdp_gps_epoch"}}, "h5testq")

local recdata = rspq:recvrecord(3000)
local rectable = recdata:tabulate()

pp.display(rectable)

runner.assert(rectable.data[1] == 0)
runner.assert(rectable.data[2] == 0)
runner.assert(rectable.data[3] == 128)
runner.assert(rectable.data[4] == 36)
runner.assert(rectable.data[5] == 15)
runner.assert(rectable.data[6] == 221)
runner.assert(rectable.data[7] == 209)
runner.assert(rectable.data[8] == 65)

-- Clean Up --

rspq:destroy()
f:destroy()

-- Report Results --

runner.report()

