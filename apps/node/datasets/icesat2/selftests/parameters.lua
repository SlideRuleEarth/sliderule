local runner = require("test_executive")
local prettyprint = require("prettyprint")

-- Self Test --

runner.unittest("ICESat-2 Fields", function()

    local parms = icesat2.parms03({
        track = 1,
        cnf = 0,
        yapc = {
            score = 0
        },
        atl08_class = {
            "atl08_noise",
            "atl08_ground",
            "atl08_canopy",
            "atl08_top_of_canopy",
            "atl08_unclassified"
        },
        poly = {
            { lon = -70.0, lat = -81.0  },
            { lon = -65.0, lat = -81.0  },
            { lon = -65.0, lat = -80.75 },
            { lon = -70.0, lat = -80.75 },
            { lon = -70.0, lat = -81.0  }
        },
        resource = "ATL03_20181019065445_03150111_007_01.h5",
        resources = {
            "ATL03_20181019065445_03150111_007_01.h5"
        },
        output = {
            path = "/tmp/tmpbary5z1t",
            format = "geoparquet",
            open_on_complete = true
        }
    })

    runner.assert(parms["track"] == 1)
    runner.assert(parms["cnf"][2] == "atl03_within_10m")
    runner.assert(parms["yapc"]["score"] == 0)
    runner.assert(#parms["atl08_class"] == 5)
    runner.assert(parms["poly"][2]["lon"] == -65.0)
    runner.assert(parms["poly"][2]["lat"] == -81.0)
    runner.assert(#parms["resource"] == 39)
    runner.assert(parms:length("resources") == 1)
    runner.assert(parms["output"]["open_on_complete"] == true)

    local parms_tbl = parms:export()

    runner.assert(parms_tbl["track"] == 1)
    runner.assert(parms_tbl["cnf"][2] == "atl03_within_10m")
    runner.assert(parms_tbl["yapc"]["score"] == 0)
    runner.assert(#parms_tbl["atl08_class"] == 5)
    runner.assert(parms_tbl["poly"][2]["lon"] == -65.0)
    runner.assert(parms_tbl["poly"][2]["lat"] == -81.0)
    runner.assert(#parms_tbl["resource"] == 39)
    runner.assert(#parms_tbl["resources"] == 1)
    runner.assert(parms_tbl["output"]["open_on_complete"] == true)

    prettyprint.display(parms_tbl)

end)

-- Self Test --

runner.unittest("ATL03 Signal Classification Fields", function()

    local parms = icesat2.parms03({
        atl03_signal_class = {
            "primary_signal",
            "fitted_signal"
        }
    })

    runner.assert(#parms["atl03_signal_class"] == 2)
    runner.assert(parms["atl03_signal_class"][1] == "primary_signal")
    runner.assert(parms["atl03_signal_class"][2] == "fitted_signal")

    local parms_numeric = icesat2.parms03({
        atl03_signal_class = { -1, 0, 4, 5 }
    })

    runner.assert(#parms_numeric["atl03_signal_class"] == 4)
    runner.assert(parms_numeric["atl03_signal_class"][1] == "ignored")
    runner.assert(parms_numeric["atl03_signal_class"][2] == "likely_noise")
    runner.assert(parms_numeric["atl03_signal_class"][3] == "primary_signal")
    runner.assert(parms_numeric["atl03_signal_class"][4] == "fitted_signal")

    -- FieldMap catches the converter throw on invalid values: construction
    -- succeeds with a server-side warning and the enumeration falls back to
    -- empty, leaving the selection inert
    local parms_invalid = icesat2.parms03({
        atl03_signal_class = { "not_a_class" }
    })

    runner.assert(parms_invalid ~= false)
    runner.assert(#parms_invalid["atl03_signal_class"] == 0)

    local parms_out_of_bounds = icesat2.parms03({
        atl03_signal_class = { 6 }
    })

    runner.assert(parms_out_of_bounds ~= false)
    runner.assert(#parms_out_of_bounds["atl03_signal_class"] == 0)

end)

-- Self Test --

runner.unittest("Invalid YAPC Version", function()

    -- FieldMap catches the parse-time throw: construction succeeds with a
    -- server-side warning and yapc is left un-provided (stage disabled);
    -- versions 1-3 on atl03x are rejected loudly at runtime (see
    -- atl03_dataframe.lua selftests)
    local parms = icesat2.parms03({
        yapc = { version = 9 }
    })

    runner.assert(parms ~= false)

end)

-- Report Results --

runner.report()

