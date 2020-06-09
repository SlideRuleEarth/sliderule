local runner = require("test_executive")
console = require("console")

-- Setup --

console.logger:config(core.INFO)

-- Unit Test --

print('\n------------------\nTest01: Atl03Handle\n------------------')

h = icesat2.h5atl03(1, 0, false)
h:config({srt=icesat2.SRT_SEA_ICE, cnf=icesat2.CNF_NOT_CONSIDERED})
p = h:parms()

runner.check(p.srt == icesat2.SRT_SEA_ICE, "Failed to set surface type")
runner.check(p.cnf == icesat2.CNF_NOT_CONSIDERED, "Failed to set signal confidence")

-- Clean Up --

-- Report Results --

runner.report()

