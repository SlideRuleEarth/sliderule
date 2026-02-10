local runner = require("test_executive")
local asset = require("asset")

-- Helper --

local function test()
    asset.loaddir()
    local nsidc_s3 = core.getbyname("icesat2")
    local not_there = core.getbyname("not_there")
    local arcticdem_mosaic = core.getbyname("arcticdem-mosaic")
    nsidc_s3 = core.getbyname("icesat2")
    runner.assert(nsidc_s3)
    runner.assert(not_there == nil)
    runner.assert(arcticdem_mosaic)
end

-- Self Test --

runner.unittest("Asset Directory Repeated Loads", function()
    test() -- first time
    test() -- second time
    test() -- third time
end)

-- Report Results --

runner.report()
