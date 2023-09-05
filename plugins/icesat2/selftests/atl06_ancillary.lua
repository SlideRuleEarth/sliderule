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
resultq = msg.subscribe("resultq")
parms = icesat2.parms({cnf=4, track=icesat2.RPT_1, atl03_geo_fields={"solar_elevation"}})
algo = icesat2.atl06("resultq", parms)
algo_disp = core.dispatcher("recq")
algo_disp:attach(algo, "atl03rec")
algo_disp:run()
reader = icesat2.atl03(nsidc_s3, "ATL03_20181017222812_02950102_005_01.h5", "recq", parms)

while true do
    rec = resultq:recvrecord(15000)
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
resultq:destroy()

runner.check(cnt >= 85, "failed to read sufficient number of contaner records")    

-- Report Results --

runner.report()

