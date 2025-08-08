local runner = require("test_executive")

-- Requirements --

if (not sys.getcfg("in_cloud") and not runner.isglobal()) then
    return runner.skip()
end

-- Setup --

runner.authenticate({'nsidc-cloud'})

local atlas_asset = core.getbyname("icesat2")
local resource = "ATL03_20181015231931_02650102_007_01.h5"

-- Self Test --

local value = h5.read(atlas_asset, resource, "ancillary_data/data_end_utc", core.STRING)
runner.compare(value, "2018-10-15T23:28:00.056100Z", string.format("Failed to read string from dataset: %s", value))

-- Report Results --

runner.report()

