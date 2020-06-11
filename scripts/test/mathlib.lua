local runner = require("test_executive")
local console = require("console")

-- Setup --

--console.logger:config(core.INFO)

-- Unit Test --

print('\n------------------\nTest01\n------------------')
t = icesat2.ut_mathlib()
runner.check(t:lsftest(), "Failed lsftest")

-- Clean Up --

-- Report Results --

runner.report()

