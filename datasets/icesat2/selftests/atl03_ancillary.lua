local runner = require("test_executive")

-- Requirements --

if (not sys.getcfg("in_cloud") and not runner.isglobal()) then
    return runner.skip()
end

-- Setup --

runner.authenticate({'nsidc-cloud'})

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

