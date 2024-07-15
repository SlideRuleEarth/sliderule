local runner = require("test_executive")
local asset = require("asset")
local json = require("json")
local pp = require("prettyprint")

-- Setup --

local console = require("console")
console.monitor:config(core.LOG, core.DEBUG)
sys.setlvl(core.LOG, core.DEBUG)

local assets = asset.loaddir()
local asset_name = "icesat2"
local atlas_asset = core.getbyname(asset_name)
local name, identity, driver = atlas_asset:info()
local resource = "ATL03_20190218011204_07880205_006_02.h5"

local creds = aws.csget(identity)
if not creds then
    local earthdata_url = "https://data.nsidc.earthdatacloud.nasa.gov/s3credentials"
    local response, _ = netsvc.get(earthdata_url)
    local _, credential = pcall(json.decode, response)
    aws.csput(identity, credential)
end

-- Unit Test --

local f = h5.file(atlas_asset, resource):name(resource)
local rspq = msg.subscribe("h5testq")

local var = {dataset="gt1r/heights/signal_conf_ph", col=3, startrow=0, numrows=10}
f:read({var}, "h5testq")

local recdata = rspq:recvrecord(3000)
local rectable = recdata:tabulate()

runner.check(rectable.data[1] == 4)
runner.check(rectable.data[2] == 1)
runner.check(rectable.data[3] == 4)
runner.check(rectable.data[4] == 4)
runner.check(rectable.data[5] == 4)
runner.check(rectable.data[6] == 4)
runner.check(rectable.data[7] == 4)
runner.check(rectable.data[8] == 4)
runner.check(rectable.data[9] == 4)
runner.check(rectable.data[10] == 4)

pp.display(rectable)

rspq:destroy()
f:destroy()

-- Report Results --

runner.report()

