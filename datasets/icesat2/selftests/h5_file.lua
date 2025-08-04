local runner = require("test_executive")
local pp = require("prettyprint")

-- Requirements --

if (not sys.getcfg("in_cloud") and not runner.isglobal()) then
    return runner.skip()
end

-- Setup --

runner.authenticate({'nsidc-cloud'})

local atlas_asset = core.getbyname("icesat2")
local resource = "ATL03_20181015231931_02650102_007_01.h5"
local f = h5.file(atlas_asset, resource)
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

