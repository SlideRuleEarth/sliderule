local runner = require("test_executive")
local asset = require("asset")
local json = require("json")
local pp = require("prettyprint")

-- Setup --

--local console = require("console")
--console.monitor:config(core.LOG, core.DEBUG)
--sys.setlvl(core.LOG, core.DEBUG)

local assets = asset.loaddir()
local asset_name = "icesat2"
local atlas_asset = core.getbyname(asset_name)
local name, identity, driver = atlas_asset:info()
local resource = "ATL03_20190218011204_07880205_006_02.h5"

local creds = aws.csget(identity)
if not creds then
    local earthdata_url = "https://data.nsidc.earthdatacloud.nasa.gov/s3credentials"
    local response, _ = core.get(earthdata_url)
    local _, credential = pcall(json.decode, response)
    aws.csput(identity, credential)
end

local f = h5.file(atlas_asset, resource):name(resource)
local rspq = msg.subscribe("h5testq")

-- Unit Test 1 --

local var = {dataset="gt1r/heights/signal_conf_ph", col=3, startrow=580000, numrows=10}
f:read({var}, "h5testq", false)
local recdata = rspq:recvrecord(3000)
local rectable = recdata:tabulate()
pp.display(rectable)

runner.check(rectable.data[1] == 4)
runner.check(rectable.data[2] == 4)
runner.check(rectable.data[3] == 4)
runner.check(rectable.data[4] == 4)
runner.check(rectable.data[5] == 4)
runner.check(rectable.data[6] == 4)
runner.check(rectable.data[7] == 4)
runner.check(rectable.data[8] == 4)
runner.check(rectable.data[9] == 4)
runner.check(rectable.data[10] == 4)

-- Unit Test 2 --

local all_good = true
local startrow = 0
local numrows = 10000
local totalrows = 5231049
while all_good do

    if startrow + numrows >= totalrows then
        numrows = totalrows - startrow
        all_good = false
    end

    local var = {dataset="gt1r/heights/signal_conf_ph", col=-1, startrow=startrow, numrows=numrows}
    f:read({var}, "h5testq", false)

    local recdata = rspq:recvrecord(3000)
    if not recdata then
        break
    end
    local rectable = recdata:tabulate()

    for _,v in ipairs(rectable.data) do
        runner.check(((v >= 0) and (v < 5)) or (v == 255))
    end

    startrow = startrow + numrows
end


rspq:destroy()
f:destroy()

-- Report Results --

runner.report()

