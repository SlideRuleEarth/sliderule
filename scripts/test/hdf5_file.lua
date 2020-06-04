local runner = require("test_executive")
console = require("console")

-- Setup --

console.logger:config(core.INFO)

-- Unit Test --

print('\n------------------\nTest01: Traverse\n------------------')
f1 = icesat2.h5file(nil, core.READER, "/data/ATLAS/ATL03_20200304065203_10470605_003_01.h5")
f1:dir(2, "gt2l")


print('\n------------------\nTest02: Read Dataset\n------------------')
h2 = icesat2.h5dataset("gt2l/heights/dist_ph_along", 0)
f2 = icesat2.h5file(h2, core.READER, "/data/ATLAS/ATL03_20200304065203_10470605_003_01.h5")

-- Clean Up --


-- Report Results --

runner.report()

