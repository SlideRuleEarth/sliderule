local runner = require("test_executive")
local console = require("console")

-- Setup --

atl06_dispatch = icesat2.ut_atl06()
atl03_reader = icesat2.ut_atl03()

-- Unit Test --

print('\n------------------\nTest01\n------------------')
runner.check(atl06_dispatch:lsftest(), "Failed lsftest")

print('\n------------------\nTest02\n------------------')
runner.check(atl06_dispatch:sorttest(), "Failed sorttest")

-- Clean Up --

-- Report Results --

runner.report()

