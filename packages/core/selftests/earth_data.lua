local runner = require("test_executive")
local earthdata = require("earth_data_query")

-- Self Test --

--[[
    CMR Query
--]]
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
runner.assert(rc == earthdata.SUCCESS, string.format("failed cmr request: %d", rc))
if rc == earthdata.SUCCESS then
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
end

--[[
    STAC Query
--]]
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
runner.assert(rc == earthdata.SUCCESS, string.format("failed stac request: %d", rc))
if rc == earthdata.SUCCESS then
    runner.assert(rsps["context"]["returned"] >= 10)
    runner.assert(rsps["context"]["returned"] == #rsps["features"])
    runner.assert(rsps["features"][1]["properties"]["B11"] ~= nil)
end

--[[
    TNM Query
--]]
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
runner.assert(rc == earthdata.SUCCESS, string.format("failed tnm request: %d", rc))
if rc == earthdata.SUCCESS then
    runner.assert(#rsps["features"] >= 56, string.format("failed to return enough results: %d", #rsps["features"]))
    runner.assert(#rsps["features"][1]["geometry"]["coordinates"][1] == 5, string.format("failed to return enough coordinates for each feature: %d", #rsps["features"][1]["geometry"]["coordinates"][1]))
end

-- Clean Up --

-- Report Results --

runner.report()
