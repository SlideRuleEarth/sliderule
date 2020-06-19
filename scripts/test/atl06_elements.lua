local runner = require("test_executive")
console = require("console")

-- Setup --

console.logger:config(core.INFO)

-- Unit Test --

print('\n------------------\nTest01: Atl03Device\n------------------')

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

print("", extentrec:getfield("TRACK"))
--rectable = recdata:tabulate()
--print("ID:     "..rectable.ID)


-- Clean Up --

-- Report Results --

runner.report()

