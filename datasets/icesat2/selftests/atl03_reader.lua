local runner = require("test_executive")
local json = require("json")

-- Requirements --

if (not sys.getcfg("in_cloud") and not runner.isglobal()) then
    return runner.skip()
end

-- Setup --

runner.authenticate({'nsidc-cloud'})

-- Self Test --

print('\n------------------\nTest01: Atl03 Reader \n------------------')

local f1_0 = icesat2.atl03s("tmpq", icesat2.parms({resource="missing_file", srt=icesat2.SRT_SEA_ICE, cnf=icesat2.CNF_NOT_CONSIDERED, track=icesat2.RPT_1}))
local f1_1 = icesat2.atl03s("tmpq", icesat2.parms({resource="missing_file", srt=icesat2.SRT_SEA_ICE, cnf={icesat2.CNF_NOT_CONSIDERED}, track=icesat2.RPT_1}))
local f1_2 = icesat2.atl03s("tmpq", icesat2.parms({resource="missing_file", srt=icesat2.SRT_SEA_ICE, cnf={"atl03_not_considered"}, track=icesat2.RPT_1}))
local f1_3 = icesat2.atl03s("tmpq", icesat2.parms({resource="missing_file", srt=icesat2.SRT_SEA_ICE, cnf={"atl03_low", "atl03_medium", "atl03_high"}, track=icesat2.RPT_1}))

print('\n------------------\nTest02: Atl03 Reader Extent Record\n------------------')

local recq = msg.subscribe("atl03-reader-recq")
local tstart = time.latch()
local f2 = icesat2.atl03s("atl03-reader-recq", icesat2.parms({resource="ATL03_20200304065203_10470605_005_01.h5", cnf=4, track=icesat2.RPT_1}))
local extentrec = recq:recvrecord(15000)
print("Time to execute: "..tostring(time.latch() - tstart))

runner.assert(extentrec, "Failed to read an extent record")

if extentrec then
    runner.assert(extentrec:getvalue("track") == 1, extentrec:getvalue("track"))
    runner.assert(extentrec:getvalue("segment_id") == 555765, extentrec:getvalue("segment_id"))

    local t2 = extentrec:tabulate()
    runner.assert(t2.segment_id == extentrec:getvalue("segment_id"))
end

print('\n------------------\nTest03: Atl03 Reader Extent Definition\n------------------')

local def = msg.definition("atl03rec")
print("atl03rec", json.encode(def))

-- Clean Up --

recq:destroy()
f1_0:destroy()
f1_1:destroy()
f1_2:destroy()
f1_3:destroy()
f2:destroy()

-- Report Results --

runner.report()

