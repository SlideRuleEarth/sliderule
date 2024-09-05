local runner = require("test_executive")
-- local console = require("console")
local asset = require("asset")
local json = require("json")

-- Setup --

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


print('\n------------------\nTest01: Atl03 Viewer Extent Record\n------------------')

local recq = msg.subscribe("atl03-reader-recq")
local tstart = time.latch()
local f1 = icesat2.atl03v(nsidc_s3, "ATL03_20200304065203_10470605_005_01.h5", "atl03-reader-recq", icesat2.parms({track=icesat2.RPT_1}))
local extentrec = recq:recvrecord(15000)
print("Time to execute: "..tostring(time.latch() - tstart))

runner.check(extentrec, "Failed to read an extent record")


if extentrec then
    runner.check(extentrec:getvalue("track") == 1, extentrec:getvalue("track"))
    runner.check(extentrec:getvalue("segments[0].segment_id") == 555764, extentrec:getvalue("segments[0].segment_id"))

    -- NOTE: there is a bug in LuaLibraryMsg.cpp::getvalue() that does not handle array indices
    --       regardless of the index value, it always returns the first element in the array
    -- runner.check(extentrec:getvalue("segments[256].segment_id") == 556019, extentrec:getvalue("segments[256].segment_id"))
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

