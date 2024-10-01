local runner = require("test_executive")
--local console = require("console")
local asset = require("asset")
local json = require("json")

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

-- Unit Test --

local value = h5.read(atlas_asset, resource, "ancillary_data/data_end_utc", core.STRING)
runner.compare(value, "2018-10-15T23:28:00.056100Z", string.format("Failed to read string from dataset: %s", value))

-- Report Results --

runner.report()

