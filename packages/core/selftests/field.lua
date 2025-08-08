local runner = require("test_executive")

-- Requirements --

if not core.UNITTEST then
    return runner.skip()
end

-- Self Test --

local ut = core.ut_field()
runner.assert(ut:element())
runner.assert(ut:array())
runner.assert(ut:enumeration())
runner.assert(ut:list())
runner.assert(ut:column())
runner.assert(ut:dictionary())

-- Report Results --

runner.report()

