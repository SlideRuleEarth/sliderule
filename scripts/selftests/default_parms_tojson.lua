local runner = require("test_executive")
local console = require("console")
local asset = require("asset")
local assets = asset.loaddir()

local verbose = true
local jsonstr = ""

if __arrow__ then
    jsonstr = arrow.parms({}):tojson()
    runner.check(string.len(jsonstr) > 0)
    if verbose then print("arrow: " .. jsonstr) end
end

if __cre__ then
    jsonstr = cre.parms({}):tojson()
    runner.check(string.len(jsonstr) > 0)
    if verbose then print("\ncre: " .. jsonstr) end
end

if __geo__ then
    jsonstr = geo.parms({}):tojson()
    runner.check(string.len(jsonstr) > 0)
    if verbose then print("\ngeo: " .. jsonstr) end
end

if __netsvc__ then
    jsonstr = netsvc.parms({}):tojson()
    runner.check(string.len(jsonstr) > 0)
    if verbose then print("\nnetsvc: " .. jsonstr) end
end

if __swot__ then
    jsonstr = swot.parms({}):tojson()
    runner.check(string.len(jsonstr) > 0)
    if verbose then print("\nswot: " .. jsonstr) end
end

if __gedi__ then
    jsonstr = gedi.parms({}):tojson()
    runner.check(string.len(jsonstr) > 0)
    if verbose then print("\ngedi: " .. jsonstr) end
end

if __icesat2__ then
    jsonstr = icesat2.parms({}):tojson()
    runner.check(string.len(jsonstr) > 0)
    if verbose then print("\nicesat2: " .. jsonstr) end
end

-- Report Results --

runner.report()

