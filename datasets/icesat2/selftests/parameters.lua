local runner = require("test_executive")
local asset = require("asset")
local json = require("json")
local prettyprint = require("prettyprint")

-- Setup Logging --

local console = require("console")
console.loglvl(core.INFO)

-- Setup Assets --

local assets = asset.loaddir()

-- Unit Test --

runner.unittest("ICESat-2 Fields", function()

    local parms = icesat2.parms({
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
        resources = {
            "ATL03_20181019065445_03150111_005_01.h5"
        },
        output = {
            path = "/tmp/tmpbary5z1t",
            format = "geoparquet",
            open_on_complete = true
        }
    })

    prettyprint.display(parms["cnf"])

    runner.check(parms["track"] == 1)
    runner.check(parms["cnf"][2] == "atl03_within_10m")
    runner.check(parms["yapc"]["score"] == 0)
    runner.check(#parms["atl08_class"] == 5)
    runner.check(parms["poly"][2]["lon"] == -65.0)
    runner.check(parms["poly"][2]["lat"] == -81.0)
    runner.check(parms:length("resources") == 1)
    runner.check(parms["output"]["open_on_complete"] == true)

    local parms_tbl = parms:export()

    runner.check(parms_tbl["track"] == 1)
    runner.check(parms_tbl["cnf"][2] == "atl03_within_10m")
    runner.check(parms_tbl["yapc"]["score"] == 0)
    runner.check(#parms_tbl["atl08_class"] == 5)
    runner.check(parms_tbl["poly"][2]["lon"] == -65.0)
    runner.check(parms_tbl["poly"][2]["lat"] == -81.0)
    runner.check(#parms_tbl["resources"] == 1)
    runner.check(parms_tbl["output"]["open_on_complete"] == true)

    prettyprint.display(parms_tbl)

end)

-- Report Results --

runner.report()

