local runner    = require("test_executive")
local consoleq  = msg.subscribe("consoleq") -- prevents error posting to consoleq
local timeout   = 60 * 1000 -- 1 minute
-- Setup --

runner.authenticate({'nsidc-cloud'})

-- Self Test --

runner.unittest("ATL24 Granule", function()

    local resource      = "ATL03_20191215112656_12150507_006_01.h5"
    local parms         = bathy.parms({}, nil, "icesat2", resource)
    local atl03h5       = h5coro.object(parms["asset"], resource)
    local granule       = icesat2.atl03granule(parms, atl03h5, "consoleq")

    runner.assert(granule:waiton(timeout), "failed to read granule", true)

    local info = granule:export()

    runner.assert(info["sc_orient_time"] == 61601400.0)
    runner.assert(info["start_geoseg"] == 852094)
    runner.assert(info["end_orbit"] == 6964)
    runner.assert(info["data_start_utc"] == "2019-12-15T11:26:56.059303Z")

end)

-- Report Results --

runner.report()
