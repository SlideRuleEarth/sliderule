local runner = require("test_executive")
console = require("console")
asset = require("asset")
json = require("json")

-- Setup --

local assets = asset.loaddir() -- looks for asset_directory.csv in same directory this script is located in
local asset_name = "nsidc-s3"
local nsidc_s3 = core.getbyname(asset_name)

local creds = aws.csget(asset_name)
if not creds then
    local earthdata_url = "https://data.nsidc.earthdatacloud.nasa.gov/s3credentials"
    local response, _ = netsvc.get(earthdata_url)
    local _, credential = pcall(json.decode, response)
    aws.csput(asset_name, credential)
end

-- Unit Test --

print('\n------------------\nTest01: Atl03 Reader \n------------------')

f1_0 = icesat2.atl03(nsidc_s3, "missing_file", "tmpq", {srt=icesat2.SRT_SEA_ICE, cnf=icesat2.CNF_NOT_CONSIDERED, track=icesat2.RPT_1})
p1 = f1_0:parms()

runner.check(p1.srt == icesat2.SRT_SEA_ICE, "Failed to set surface type")
runner.check(not p1.cnf[icesat2.CNF_POSSIBLE_TEP], "Failed to not set _tep_")
runner.check(p1.cnf[icesat2.CNF_NOT_CONSIDERED], "Failed to set _not considered_")
runner.check(p1.cnf[icesat2.CNF_BACKGROUND], "Failed to set _background_")
runner.check(p1.cnf[icesat2.CNF_WITHIN_10M], "Failed to set _within 10m_")
runner.check(p1.cnf[icesat2.CNF_SURFACE_LOW], "Failed to set _low_")
runner.check(p1.cnf[icesat2.CNF_SURFACE_MEDIUM], "Failed to set _medium_")
runner.check(p1.cnf[icesat2.CNF_SURFACE_HIGH], "Failed to set _high_")

f1_1 = icesat2.atl03(nsidc_s3, "missing_file", "tmpq", {srt=icesat2.SRT_SEA_ICE, cnf={icesat2.CNF_NOT_CONSIDERED}, track=icesat2.RPT_1})
p1 = f1_1:parms()

runner.check(not p1.cnf[icesat2.CNF_POSSIBLE_TEP], "Failed to not set _tep_")
runner.check(p1.cnf[icesat2.CNF_NOT_CONSIDERED], "Failed to set _not considered_")
runner.check(not p1.cnf[icesat2.CNF_BACKGROUND], "Failed to not set _background_")
runner.check(not p1.cnf[icesat2.CNF_WITHIN_10M], "Failed to not set _within 10m_")
runner.check(not p1.cnf[icesat2.CNF_SURFACE_LOW], "Failed to not set _low_")
runner.check(not p1.cnf[icesat2.CNF_SURFACE_MEDIUM], "Failed to not set _medium_")
runner.check(not p1.cnf[icesat2.CNF_SURFACE_HIGH], "Failed to not set _high_")

f1_2 = icesat2.atl03(nsidc_s3, "missing_file", "tmpq", {srt=icesat2.SRT_SEA_ICE, cnf={"atl03_not_considered"}, track=icesat2.RPT_1})
p1 = f1_2:parms()

runner.check(not p1.cnf[icesat2.CNF_POSSIBLE_TEP], "Failed to not set _tep_")
runner.check(p1.cnf[icesat2.CNF_NOT_CONSIDERED], "Failed to set _not considered_")
runner.check(not p1.cnf[icesat2.CNF_BACKGROUND], "Failed to not set _background_")
runner.check(not p1.cnf[icesat2.CNF_WITHIN_10M], "Failed to not set _within 10m_")
runner.check(not p1.cnf[icesat2.CNF_SURFACE_LOW], "Failed to not set _low_")
runner.check(not p1.cnf[icesat2.CNF_SURFACE_MEDIUM], "Failed to not set _medium_")
runner.check(not p1.cnf[icesat2.CNF_SURFACE_HIGH], "Failed to not set _high_")

f1_3 = icesat2.atl03(nsidc_s3, "missing_file", "tmpq", {srt=icesat2.SRT_SEA_ICE, cnf={"atl03_low", "atl03_medium", "atl03_high"}, track=icesat2.RPT_1})
p1 = f1_3:parms()

runner.check(not p1.cnf[icesat2.CNF_POSSIBLE_TEP], "Failed to not set _tep_")
runner.check(not p1.cnf[icesat2.CNF_NOT_CONSIDERED], "Failed to not set _not considered_")
runner.check(not p1.cnf[icesat2.CNF_BACKGROUND], "Failed to not set _background_")
runner.check(not p1.cnf[icesat2.CNF_WITHIN_10M], "Failed to not set _within 10m_")
runner.check(p1.cnf[icesat2.CNF_SURFACE_LOW], "Failed to set _low_")
runner.check(p1.cnf[icesat2.CNF_SURFACE_MEDIUM], "Failed to set _medium_")
runner.check(p1.cnf[icesat2.CNF_SURFACE_HIGH], "Failed to set _high_")

print('\n------------------\nTest02: Atl03 Extent Record\n------------------')

recq = msg.subscribe("recq")
f2 = icesat2.atl03(nsidc_s3, "ATL03_20200304065203_10470605_004_01.h5", "recq", {cnf=4, track=icesat2.RPT_1})
extentrec = recq:recvrecord(3000)
recq:destroy()

runner.check(extentrec, "Failed to read an extent record")

if extentrec then
    runner.check(extentrec:getvalue("track") == 1, extentrec:getvalue("track"))
    runner.check(extentrec:getvalue("segment_id[0]") == 555765, extentrec:getvalue("segment_id[0]"))
    runner.check(extentrec:getvalue("segment_id[1]") == 555765, extentrec:getvalue("segment_id[1]"))
    runner.check(extentrec:getvalue("count[0]") == 136, extentrec:getvalue("count[0]"))
    runner.check(extentrec:getvalue("count[1]") == 540, extentrec:getvalue("count[1]"))

    t2 = extentrec:tabulate()

    runner.check(t2.segment_id[1] == extentrec:getvalue("segment_id[0]"))
    runner.check(t2.segment_id[2] == extentrec:getvalue("segment_id[1]"))
end

print('\n------------------\nTest03: Atl03 Extent Definition\n------------------')

def = msg.definition("atl03rec")
print("atl03rec", json.encode(def))

-- Clean Up --

-- Report Results --

runner.report()

