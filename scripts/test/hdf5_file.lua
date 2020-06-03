local runner = require("test_executive")
console = require("console")

-- Setup --

console.logger:config(core.INFO)

-- Unit Test --

print('\n------------------\nTest01: Traverse\n------------------')
--h5 = icesat2.h5dataset("/", 0)
f5 = icesat2.h5file(nil, core.READER, "/data/ATLAS/ATL03_20200304065203_10470605_003_01.h5")
f5:dir(2, "gt2l")

-- Clean Up --


-- Report Results --

runner.report()

