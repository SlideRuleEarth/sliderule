local runner = require("test_executive")
local console = require("console")
local earthdata = require("earth_data_query")
local prettyprint = require("prettyprint")
local json = require("json")

-- Setup --

console.monitor:config(core.LOG, core.DEBUG)
sys.setlvl(core.LOG, core.DEBUG)

-- Unit Test --

--[[
    CMR Query
--]]
local parms = {
    ["asset"] = "icesat2",
    ["t0"] = "2021-01-01T00:00:00Z",
    ["t1"] = "2021-02-01T23:59:59Z",
    ["polygon"] = { 
        {["lon"] = -177.0000000001, ["lat"] = 51.0000000001},
        {["lon"] = -179.0000000001, ["lat"] = 51.0000000001},
        {["lon"] = -179.0000000001, ["lat"] = 49.0000000001},
        {["lon"] = -177.0000000001, ["lat"] = 49.0000000001},
        {["lon"] = -177.0000000001, ["lat"] = 51.0000000001} 
    }
}
local rc, rsps = earthdata.cmr(parms)

--[[
    STAC Query
local parms = {
    ["asset"] = "landsat-hls",
    ["t0"] = "2021-01-01T00:00:00Z",
    ["t1"] = "2021-02-01T23:59:59Z",
    ["polygon"] = { 
        {["lon"] = -177.0000000001, ["lat"] = 51.0000000001},
        {["lon"] = -179.0000000001, ["lat"] = 51.0000000001},
        {["lon"] = -179.0000000001, ["lat"] = 49.0000000001},
        {["lon"] = -177.0000000001, ["lat"] = 49.0000000001},
        {["lon"] = -177.0000000001, ["lat"] = 51.0000000001} 
    }
}
local rc, rsps = earthdata.stac(parms)
prettyprint.display(rsps)
--]]

-- Clean Up --


-- Report Results --

runner.report()
