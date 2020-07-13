local runner = require("test_executive")
local console = require("console")

-- Setup --

--console.logger:config(core.INFO)
t = icesat2.ut_atl06()

-- Unit Test --

print('\n------------------\nTest01\n------------------')
runner.check(t:lsftest(), "Failed lsftest")

print('\n------------------\nTest02\n------------------')
runner.check(t:sorttest(), "Failed sorttest")

-- Clean Up --

-- Report Results --

runner.report()

