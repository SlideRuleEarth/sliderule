local runner = require("test_executive")
local asset = require("asset")
local json = require("json")

-- Requirements --

if (not sys.getcfg("in_cloud") and not runner.isglobal()) then
    return runner.skip()
end

-- Setup --

runner.authenticate({'nsidc-cloud'})

-- Self Test --

local cnt = 0
local resultq = msg.subscribe("atl06-ancillary-resultq")
local parms = icesat2.parms({resource="ATL03_20181017222812_02950102_005_01.h5", srt=3, cnf=4, track=icesat2.RPT_1, atl03_geo_fields={"solar_elevation"}})
local algo = icesat2.atl06("atl06-ancillary-resultq", parms)
local algo_disp = streaming.dispatcher("atl06-ancillary-recq")
algo_disp:attach(algo, "atl03rec")
algo_disp:run()
local reader = icesat2.atl03s("atl06-ancillary-recq", parms)

while true do
    local rec = resultq:recvrecord(30000)
    if rec == nil then
        break
    end
    cnt = cnt + 1
    if cnt < 85 then
        runner.assert(rec:getvalue("count") == 257, string.format('Expected different number of records in container: %d', rec:getvalue("count")))
    else -- last batch
        runner.assert(rec:getvalue("count") == 245, string.format('Expected different number of records in container: %d', rec:getvalue("count")))
    end
end

runner.assert(cnt >= 85, string.format('failed to read sufficient number of contaner records: %d', cnt))

-- Clean Up --

resultq:destroy()
algo:destroy()
algo_disp:destroy()
reader:destroy()

-- Report Results --

runner.report()

