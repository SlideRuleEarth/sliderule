local runner = require("test_executive")
local console = require("console")

-- Setup --

console.logger:config(core.INFO)

-- Unit Test --

print('\n------------------\nTest01: Traverse\n------------------')

--f1 = h5.file("file:///data/ATLAS/ATL03_20200304065203_10470605_003_01.h5")
--runner.check(f1:dir(2, "gt2l/heights/delta_time"), "failed to traverse h5lite file")

print('\n------------------\nTest02: Read\n------------------')

f2 = h5.dataset(core.READER, "file:///data/ATLAS/ATL03_20200304065203_10470605_003_01.h5", "gt2l/heights/delta_time", 0, true)

-- Clean Up --

-- Report Results --

sys.wait(1)

runner.report()

