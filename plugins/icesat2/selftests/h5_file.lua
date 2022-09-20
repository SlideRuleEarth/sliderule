local runner = require("test_executive")
local console = require("console")
local pp = require("prettyprint")

local asset = "atlas-s3"
local resource = "ATL03_20181015231931_02650102_003_01.h5"

-- Unit Test --

local f = h5.file(asset, resource)
local rspq = msg.subscribe("h5testq")

f:read({{dataset="ancillary_data/atlas_sdp_gps_epoch"}}, "h5testq")

local recdata = rspq:recvrecord(3000)
local rectable = recdata:tabulate()

pp.display(rectable)

rspq:destroy()
f:destroy()

-- Report Results --

runner.report()

