local runner = require("test_executive")
console = require("console")
asset = require("asset")
json = require("json")

-- Setup --

-- console.monitor:config(core.LOG, core.DEBUG)
-- sys.setlvl(core.LOG, core.DEBUG)

local assets = asset.loaddir()
local asset_name = "icesat2"
local nsidc_s3 = core.getbyname(asset_name)
local name, identity, driver = nsidc_s3:info()

local creds = aws.csget(identity)
if not creds then
    local earthdata_url = "https://data.nsidc.earthdatacloud.nasa.gov/s3credentials"
    local response, _ = netsvc.get(earthdata_url)
    local _, credential = pcall(json.decode, response)
    aws.csput(identity, credential)
end

-- Unit Test --

cnt = 0
recq = msg.subscribe("recq")
f = icesat2.atl03(nsidc_s3, "ATL03_20181017222812_02950102_005_01.h5", "recq", icesat2.parms({cnf=4, track=icesat2.RPT_1, atl03_geo_fields={"solar_elevation"}}))
while true do
    rec = recq:recvrecord(15000)
    if rec == nil then
        break
    end
    cnt = cnt + 1
    runner.check(rec:getvalue("count") == 2, rec:getvalue("count"))
end
recq:destroy()

runner.check(cnt >= 16697, "failed to read sufficient number of contaner records")

-- Report Results --

runner.report()

