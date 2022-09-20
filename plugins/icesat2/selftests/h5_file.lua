local runner = require("test_executive")
local console = require("console")
local asset = require("asset")
local pp = require("prettyprint")

-- Setup --

console.loglvl(core.DEBUG)

local assets = asset.loaddir() -- looks for asset_directory.csv in same directory this script is located in
local atlas_asset = core.getbyname("atlas-s3")
local resource = "ATL03_20181015231931_02650102_003_01.h5"

-- Unit Test --

local f = h5.file(atlas_asset, resource):name(resource)
local rspq = msg.subscribe("h5testq")

f:read({{dataset="ancillary_data/atlas_sdp_gps_epoch"}}, "h5testq")

local recdata = rspq:recvrecord(3000)
local rectable = recdata:tabulate()

pp.display(rectable)

runner.check(rectable.data[1] == 0)
runner.check(rectable.data[2] == 0)
runner.check(rectable.data[3] == 128)
runner.check(rectable.data[4] == 36)
runner.check(rectable.data[5] == 15)
runner.check(rectable.data[6] == 221)
runner.check(rectable.data[7] == 209)
runner.check(rectable.data[8] == 65)

rspq:destroy()
f:destroy()

-- Report Results --

runner.report()

