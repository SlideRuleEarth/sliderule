local runner = require("test_executive")

-- Requirements --
if (not sys.getcfg("in_cloud") and not runner.isglobal()) then
    return runner.skip()
end

-- Config --
runner.authenticate({'nsidc-cloud'})
local asset_name = "icesat2"

local script_dir = debug.getinfo(1, "S").source:sub(2):match("(.*/)")
local baseline_path = script_dir .. "../data/UT_AOIData.txt"

local function run_df(factory, beam, resource, extra_parms)
    local parms = icesat2.parms({resource = resource})
    if extra_parms then
        for k,v in pairs(extra_parms) do
            parms[k] = v
        end
    end
    local h5 = h5.object(asset_name, parms["resource"])
    local df = icesat2[factory](beam, parms, h5, core.EVENTQ)
    runner.assert(df:waiton(60000), string.format("timeout creating %s %s", factory, beam))
    runner.assert(df:inerror() == false, string.format("%s encountered error", factory))
    local rows = df:numrows()
    df:destroy()
    h5:destroy()
    return rows
end

local function collect()
    local cases = {
        {name = "atl06x", beam = "gt1l", resource = "ATL06_20200303180710_10390603_007_01.h5"},
        {name = "atl08x", beam = "gt3r", resource = "ATL08_20200307004141_10890603_007_01.h5"},
    }

    local results = {}
    for _,c in ipairs(cases) do
        local rows = run_df(c.name, c.beam, c.resource, c.parms)
        table.insert(results, string.format("%s %s %d", c.name, c.beam, rows))
    end
    return results
end

local function write_baseline(lines)
    local f = io.open(baseline_path, "w")
    runner.assert(f ~= nil, string.format("failed to open %s for write", baseline_path))
    for _,line in ipairs(lines) do
        f:write(line .. "\n")
    end
    f:close()
end

local function load_baseline()
    local f = io.open(baseline_path, "r")
    if not f then return nil end
    local lines = {}
    for line in f:lines() do
        table.insert(lines, line)
    end
    f:close()
    return lines
end

local function compare(expected, observed)
    runner.assert(#expected == #observed, "baseline length mismatch")
    for i=1,#expected do
        runner.assert(expected[i] == observed[i], string.format("baseline mismatch: %s vs %s", expected[i], observed[i]))
    end
end

runner.unittest("AOI DataFrame Baseline", function()
    local observed = collect()
    local baseline = load_baseline()

    if (AOI_GENERATE_BASELINE == true) or (baseline == nil) then
        write_baseline(observed)
        runner.printf("AOI baseline generated at %s\n", baseline_path)
    else
        compare(baseline, observed)
    end
end)
