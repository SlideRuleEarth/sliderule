local runner = require("test_executive")

-- Requirements --

if (not sys.getcfg("in_cloud") and not runner.isglobal()) then
    return runner.skip()
end

-- Setup --

runner.authenticate({})

-- Helper Function --

local function check_expected(exp, df, index, t)
    for key,value in pairs(exp) do
        runner.assert(math.abs(df[key][index] - value) <= t, string.format("%s[%d] => %f", key, index, df[key][index]))
    end
end

-- Self Tests --

runner.unittest("CASALS 1B DataFrame", function()

    local parms = casals.parms({
        resource = "lidar/2024-11-12/casals_l1b_20241112T163941_001_02.h5"
    })

    local casals1bh5 = h5.object("casals1b", parms["resource"])
    local casals1bdf = casals.casals1bx(parms, casals1bh5, nil, nil, core.EVENTQ)

    runner.assert(casals1bdf:waiton(30000), "timed out creating dataframe", true)
    runner.assert(casals1bdf:inerror() == false, "dataframe encountered error")

    runner.assert(casals1bdf:numrows() == 440832, string.format("incorrect number of rows: %d", casals1bdf:numrows()))
    runner.assert(casals1bdf:numcols() == 4, string.format("incorrect number of columns: %d", casals1bdf:numcols()))

    check_expected({
        time_ns = 1731429581715359744,
        latitude = 38.298965,
        longitude = -75.539483,
        refh = -79.631470
    }, casals1bdf, 100, 0.00001)

end)

-- Report Results --

runner.report()
