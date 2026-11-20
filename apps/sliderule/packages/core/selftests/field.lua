local runner = require("test_executive")

-- Requirements --

if not core.UNITTEST then
    return runner.skip()
end

-- Self Test --

runner.unittest("Field Unit Test", function()
    local ut = core.ut_field()
    runner.assert(ut:element())
    runner.assert(ut:array())
    runner.assert(ut:enumeration())
    runner.assert(ut:list())
    runner.assert(ut:column())
    runner.assert(ut:dictionary())
end)

-- Report Results --

runner.report()

