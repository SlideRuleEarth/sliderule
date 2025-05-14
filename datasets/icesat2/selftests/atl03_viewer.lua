local runner = require("test_executive")
local asset = require("asset")
local json = require("json")

-- Requirements --

if (not sys.getcfg("in_cloud") and not runner.isglobal()) then
    return runner.skip()
end

-- Setup --

runner.authenticate()

-- Self Test --

print('\n------------------\nTest01: Atl03 Viewer Extent Record\n------------------')

local recq = msg.subscribe("atl03-reader-recq")
local tstart = time.latch()
local f1 = icesat2.atl03v("atl03-reader-recq", icesat2.parms({resource="ATL03_20200304065203_10470605_005_01.h5", track=icesat2.RPT_1}))
local extentrec = recq:recvrecord(15000)
print("Time to execute: "..tostring(time.latch() - tstart))

runner.assert(extentrec, "Failed to read an extent record")

if extentrec then
    runner.assert(extentrec:getvalue("track") == 1, extentrec:getvalue("track"))
    runner.assert(extentrec:getvalue("segments[0].segment_id") == 555764, extentrec:getvalue("segments[0].segment_id"))

    -- NOTE: there is a bug in LuaLibraryMsg.cpp::getvalue() that does not handle array indices
    --       regardless of the index value, it always returns the first element in the array
    -- runner.assert(extentrec:getvalue("segments[256].segment_id") == 556019, extentrec:getvalue("segments[256].segment_id"))
    print("segment   0", extentrec:getvalue("segments[0].segment_id"))
    print("segment  10", extentrec:getvalue("segments[10].segment_id"))
    print("segment 100", extentrec:getvalue("segments[100].segment_id"))
end

print('\n------------------\nTest02: Atl03 Viewer Extent Definition\n------------------')

local def = msg.definition("atl03vrec")
print("atl03vrec", json.encode(def))

-- Clean Up --

recq:destroy()
f1:destroy()

-- Report Results --

runner.report()

