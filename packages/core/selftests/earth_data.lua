local runner = require("test_executive")
local earthdata = require("earth_data_query")

-- Self Test --

--[[
    CMR Query
--]]
runner.unittest("CMR Query", function()
    local parms = {
        ["asset"] = "icesat2",
        ["t0"] = "2021-01-01T00:00:00Z",
        ["t1"] = "2021-02-01T23:59:59Z",
        ["poly"] = {
            {["lon"] = -177.0000000001, ["lat"] = 51.0000000001},
            {["lon"] = -179.0000000001, ["lat"] = 51.0000000001},
            {["lon"] = -179.0000000001, ["lat"] = 49.0000000001},
            {["lon"] = -177.0000000001, ["lat"] = 49.0000000001},
            {["lon"] = -177.0000000001, ["lat"] = 51.0000000001}
        }
    }
    local rc, rsps = earthdata.cmr(parms)
    runner.assert(rc == earthdata.SUCCESS, string.format("failed cmr request: %d", rc), true)
    runner.assert(#rsps >= 5)
    runner.assert(type(rsps) == "table")
    if type(rsps) == "table" then
        local found = false
        for _,resource in ipairs(rsps) do
            if resource == "ATL03_20210104122558_01761002_006_01.h5" then
                found = true
            end
        end
        runner.assert(found, "unable to find resource")
    end
end)

--[[
    STAC Query
--]]
runner.unittest("STAC Query", function()
    local parms = {
        ["asset"] = "landsat-hls",
        ["t0"] = "2021-01-01T00:00:00Z",
        ["t1"] = "2021-02-01T23:59:59Z",
        ["poly"] = {
            {["lon"] = -177.0000000001, ["lat"] = 51.0000000001},
            {["lon"] = -179.0000000001, ["lat"] = 51.0000000001},
            {["lon"] = -179.0000000001, ["lat"] = 49.0000000001},
            {["lon"] = -177.0000000001, ["lat"] = 49.0000000001},
            {["lon"] = -177.0000000001, ["lat"] = 51.0000000001}
        }
    }
    local rc, rsps = earthdata.stac(parms)
    runner.assert(rc == earthdata.SUCCESS, string.format("failed stac request: %d", rc), true)
    runner.assert(#rsps["features"] >= 10)
    runner.assert(rsps["features"][1]["properties"]["B11"] ~= nil)
end)

--[[
    TNM Query
--]]
runner.unittest("TNM Query", function()
    local parms = {
        ["asset"] = "usgs3dep-1meter-dem",
        ["poly"] = {
            {["lon"] = -108.3435200747503, ["lat"] = 38.89102961045247},
            {["lon"] = -107.7677425431139, ["lat"] = 38.90611184543033},
            {["lon"] = -107.7818591266989, ["lat"] = 39.26613714985466},
            {["lon"] = -108.3605610678553, ["lat"] = 39.25086131372244},
            {["lon"] = -108.3435200747503, ["lat"] = 38.89102961045247}
        }
    }
    local rc, rsps = earthdata.tnm(parms)
    runner.assert(rc == earthdata.SUCCESS, string.format("failed tnm request: %s", rsps), true)
    runner.assert(#rsps["features"] >= 56, string.format("failed to return enough results: %d", #rsps["features"]))
    runner.assert(#rsps["features"][1]["geometry"]["coordinates"][1] == 5, string.format("failed to return enough coordinates for each feature: %d", #rsps["features"][1]["geometry"]["coordinates"][1]))
end)

--[[
    ArcticDEM Region
--]]
runner.unittest("STAC Query - ArcticDEM Region", function()
    local parms = {
        ["asset"] = "arcticdem-strips",
        ["t0"] = "2000-01-01T00:00:00Z",
        ["poly"] = { -- Central Brooks Range, northern Alaska
            {["lon"] = -152.0, ["lat"] = 68.5},
            {["lon"] = -152.0, ["lat"] = 67.5},
            {["lon"] = -147.0, ["lat"] = 67.5},
            {["lon"] = -147.0, ["lat"] = 68.5},
            {["lon"] = -152.0, ["lat"] = 68.5}
        },
        ["max_resources"] = 1000
    }
    local rc, rsps = earthdata.stac(parms)
    runner.assert(rc == earthdata.SUCCESS, string.format("failed <%d> stac request: %s", rc, rsps), true)
    runner.assert(#rsps["features"] >= 702)
    runner.assert(rsps["features"][1]["properties"]['datetime'] == "2022-05-16T21:47:36Z")
end)

--[[
    ArcticDEM Point
--]]
runner.unittest("STAC Query - ArcticDEM Point", function()
    local parms = {
        ["asset"] = "arcticdem-strips",
        ["t0"] = "2000-01-01T00:00:00Z",
        ["poly"] = { {lon = -152.0, lat = 68.5} },
        ["max_resources"] = 1000
    }
    local rc, rsps = earthdata.stac(parms)
    runner.assert(rc == earthdata.SUCCESS, string.format("failed <%d> stac request: %s", rc, rsps), true)
    runner.assert(#rsps["features"] >= 16)
    runner.assert(rsps["features"][1]["properties"]['datetime'] == "2022-03-24T22:38:46Z")
end)

--[[
    REMA Region
--]]
runner.unittest("STAC Query - REMA Region", function()
    local parms = {
        ["asset"] = "rema-strips",
        ["t0"] = "2000-01-01T00:00:00Z",
        ["poly"] = { -- Ellsworth Mountains, West Antarctica
            {["lon"] = -86.0, ["lat"] = -78.0},
            {["lon"] = -86.0, ["lat"] = -79.0},
            {["lon"] = -82.0, ["lat"] = -79.0},
            {["lon"] = -82.0, ["lat"] = -78.0},
            {["lon"] = -86.0, ["lat"] = -78.0}
        },
        ["max_resources"] = 1000
    }
    local rc, rsps = earthdata.stac(parms)
    runner.assert(rc == earthdata.SUCCESS, string.format("failed <%d> stac request: %s", rc, rsps), true)
    runner.assert(#rsps["features"] >= 603)
    runner.assert(rsps["features"][1]["properties"]['datetime'] == "2024-12-21T14:07:17Z")
end)

--[[
    REMA Point
--]]
runner.unittest("STAC Query - REMA Point", function()
    local parms = {
        ["asset"] = "rema-strips",
        ["t0"] = "2000-01-01T00:00:00Z",
        ["poly"] = { {lon = -86.0, lat = -78.0} },
        ["max_resources"] = 1000
    }
    local rc, rsps = earthdata.stac(parms)
    runner.assert(rc == earthdata.SUCCESS, string.format("failed <%d> stac request: %s", rc, rsps), true)
    runner.assert(#rsps["features"] >= 49)
    runner.assert(rsps["features"][1]["properties"]['datetime'] == "2024-11-07T14:25:48Z")
end)

-- Clean Up --

-- Report Results --

runner.report()
