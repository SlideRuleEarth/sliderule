local runner = require("test_executive")
-- local console = require("console")
local asset = require("asset")
local json = require("json")

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
    local response, _ = core.get(earthdata_url)
    local _, credential = pcall(json.decode, response)
    aws.csput(identity, credential)
end

-- Unit Test --

local cnt = 0
local resultq = msg.subscribe("atl06-ancillary-resultq")
local parms = icesat2.parms({cnf=4, track=icesat2.RPT_1, atl03_geo_fields={"solar_elevation"}})
local algo = icesat2.atl06("atl06-ancillary-resultq", parms)
local algo_disp = streaming.dispatcher("atl06-ancillary-recq")
algo_disp:attach(algo, "atl03rec")
algo_disp:run()
local reader = icesat2.atl03s(nsidc_s3, "ATL03_20181017222812_02950102_005_01.h5", "atl06-ancillary-recq", parms)

while true do
    local rec = resultq:recvrecord(20000)
    if rec == nil then
        break
    end
    cnt = cnt + 1
    if cnt < 85 then
        runner.check(rec:getvalue("count") == 257, string.format('Expected different number of records in container: %d', rec:getvalue("count")))    
    else -- last batch
        runner.check(rec:getvalue("count") == 245, string.format('Expected different number of records in container: %d', rec:getvalue("count")))    
    end
end

runner.check(cnt >= 85, string.format('failed to read sufficient number of contaner records: %d', cnt))    

-- Clean Up --

resultq:destroy()
algo:destroy()
algo_disp:destroy()
reader:destroy()

-- Report Results --

runner.report()

