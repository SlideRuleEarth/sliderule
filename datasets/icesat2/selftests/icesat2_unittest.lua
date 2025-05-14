local runner = require("test_executive")

-- Requirements --

if (not core.UNITTEST) or (not sys.getcfg("in_cloud") and not runner.isglobal()) then
    return runner.skip()
end

-- Setup --

local atl06_dispatch = icesat2.ut_atl06()

-- Self Test --

print('\n------------------\nTest01\n------------------')
runner.assert(atl06_dispatch:lsftest(), "Failed lsftest")

print('\n------------------\nTest02\n------------------')
runner.assert(atl06_dispatch:sorttest(), "Failed sorttest")

-- Clean Up --

-- Report Results --

runner.report()

