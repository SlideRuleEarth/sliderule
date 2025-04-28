local runner = require("test_executive")
local asset = require("asset")
local json = require("json")

-- Requirements --

if (not sys.incloud() and not runner.isglobal()) then
    return runner.skip()
end

-- Setup --

-- local console = require("console")
-- console.monitor:config(core.DEBUG)
-- sys.setlvl(core.LOG, core.DEBUG)

local assets = asset.loaddir()
local asset_name = "icesat2"
local nsidc_s3 = core.getbyname(asset_name)
local name, identity, driver = nsidc_s3:info()

local creds = aws.csget(identity)
if not creds then
    local earthdata_url = "https://data.nsidc.earthdatacloud.nasa.gov/s3credentials"
    local response, _ = core.get(earthdata_url)
    local _, credential = pcall(json.decode, response)
    aws.csput(identity, credential)
end

-- Self Test --

local cnt = 0
local recq = msg.subscribe("atl03-ancillary-recq")
local f = icesat2.atl03s("atl03-ancillary-recq", icesat2.parms({resource="ATL03_20181017222812_02950102_005_01.h5", srt=3, cnf=4, track=icesat2.RPT_1, atl03_geo_fields={"solar_elevation"}}))
while true do
    local rec = recq:recvrecord(25000)
    if rec == nil then
        break
    end
    cnt = cnt + 1
    runner.assert(rec:getvalue("count") == 2, rec:getvalue("count"))
end

local expected_cnt = 21748
runner.assert(cnt == expected_cnt, string.format('failed to read sufficient number of container records, expected: %d, got: %d', expected_cnt, cnt))

-- Clean Up --

recq:destroy()
f:destroy()

-- Report Results --

runner.report()

