local runner = require("test_executive")
console = require("console")
json = require("json")

-- Setup --

console.logger:config(core.INFO)

-- Unit Test --

print('\n------------------\nTest01: Atl03 Reader \n------------------')

f1 = icesat2.atl03("missing_file", "tmpq", {srt=icesat2.SRT_SEA_ICE, cnf=icesat2.CNF_NOT_CONSIDERED}, icesat2.RPT_1)
p1 = f1:parms()

runner.check(p1.srt == icesat2.SRT_SEA_ICE, "Failed to set surface type")
runner.check(p1.cnf == icesat2.CNF_NOT_CONSIDERED, "Failed to set signal confidence")


print('\n------------------\nTest02: Atl03 Extent Record\n------------------')

recq = msg.subscribe("recq")
f2 = icesat2.atl03("file:///data/ATLAS/ATL03_20200304065203_10470605_003_01.h5", "recq", {}, icesat2.RPT_1)
extentrec = recq:recvrecord(3000)
recq:destroy()

runner.check(extentrec, "Failed to read an extent record")

if extentrec then
    runner.check(extentrec:getvalue("track") == 1, extentrec:getvalue("track"))
    runner.check(extentrec:getvalue("segment_id[0]") == 555764, extentrec:getvalue("segment_id[0]"))
    runner.check(extentrec:getvalue("segment_id[1]") == 555764, extentrec:getvalue("segment_id[1]"))
    runner.check(runner.cmpfloat(extentrec:getvalue("delta_time[0]"), 1267339942.127844, 0.0001), extentrec:getvalue("delta_time[0]"))
    runner.check(runner.cmpfloat(extentrec:getvalue("delta_time[1]"), 1267339942.472445, 0.0001), extentrec:getvalue("delta_time[1]"))
    runner.check(extentrec:getvalue("count[0]") == 136, extentrec:getvalue("count[0]"))
    runner.check(extentrec:getvalue("count[1]") == 539, extentrec:getvalue("count[1]"))

    t2 = extentrec:tabulate()

    runner.check(t2.segment_id[1] == extentrec:getvalue("segment_id[0]"))
    runner.check(t2.segment_id[2] == extentrec:getvalue("segment_id[1]"))
    runner.check(runner.cmpfloat(t2.delta_time[1], extentrec:getvalue("delta_time[0]"), 0.0001))
    runner.check(runner.cmpfloat(t2.delta_time[2], extentrec:getvalue("delta_time[1]"), 0.0001))
end

print('\n------------------\nTest03: Atl03 Extent Definition\n------------------')

def = msg.definition("atl03rec")
print("atl03rec", json.encode(def))

-- Clean Up --

-- Report Results --

runner.report()

