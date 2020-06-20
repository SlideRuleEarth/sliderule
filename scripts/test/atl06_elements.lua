local runner = require("test_executive")
console = require("console")
json = require("json")

-- Setup --

console.logger:config(core.INFO)

-- Unit Test --

print('\n------------------\nTest01: Atl03 Device\n------------------')

f1 = icesat2.h5atl03("", {srt=icesat2.SRT_SEA_ICE, cnf=icesat2.CNF_NOT_CONSIDERED})
p1 = f1:parms()

runner.check(p1.srt == icesat2.SRT_SEA_ICE, "Failed to set surface type")
runner.check(p1.cnf == icesat2.CNF_NOT_CONSIDERED, "Failed to set signal confidence")


print('\n------------------\nTest02: Atl03 Extent Record\n------------------')

f2 = icesat2.h5atl03("/data/ATLAS/ATL03_20200304065203_10470605_003_01.h5")
r2 = core.reader(f2, "recq")

recq = msg.subscribe("recq")
extentrec = recq:recvrecord(3000)
recq:destroy()

runner.check(extentrec, "Failed to read an extent record")

if extentrec then
    runner.check(extentrec:getvalue("TRACK") == 1, extentrec:getvalue("TRACK"))
    runner.check(extentrec:getvalue("SEG_ID[0]") == 555764, extentrec:getvalue("SEG_ID[0]"))
    runner.check(extentrec:getvalue("SEG_ID[1]") == 555764, extentrec:getvalue("SEG_ID[1]"))
    runner.check(runner.cmpfloat(extentrec:getvalue("GPS[0]"), 1267339942.127844, 0.0001), extentrec:getvalue("GPS[0]"))
    runner.check(runner.cmpfloat(extentrec:getvalue("GPS[1]"), 1267339942.472445, 0.0001), extentrec:getvalue("GPS[1]"))
    runner.check(runner.cmpfloat(extentrec:getvalue("DIST[0]"), 11132813.466047, 0.0001), extentrec:getvalue("DIST[0]"))
    runner.check(runner.cmpfloat(extentrec:getvalue("DIST[1]"), 11132813.466047, 0.0001), extentrec:getvalue("DIST[1]"))
    runner.check(extentrec:getvalue("COUNT[0]") == 136, extentrec:getvalue("COUNT[0]"))
    runner.check(extentrec:getvalue("COUNT[1]") == 539, extentrec:getvalue("COUNT[1]"))
end

t2 = extentrec:tabulate()

runner.check(t2.SEG_ID[1] == extentrec:getvalue("SEG_ID[0]"))
runner.check(t2.SEG_ID[2] == extentrec:getvalue("SEG_ID[1]"))
runner.check(runner.cmpfloat(t2.GPS[1], extentrec:getvalue("GPS[0]"), 0.0001))
runner.check(runner.cmpfloat(t2.GPS[2], extentrec:getvalue("GPS[1]"), 0.0001))


print('\n------------------\nTest03: Atl03 Extent Definition\n------------------')

def = msg.definition("atl03rec")
print("atl03rec", json.encode(def))

-- Clean Up --

-- Report Results --

runner.report()

