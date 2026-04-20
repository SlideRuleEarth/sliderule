-------------------------------------------------------
-- main
-------------------------------------------------------
local function main()
    local json      = require("json")
    local rqst      = json.decode(arg[1])
    local parms     = icesat2.parms(rqst["parms"], "icesat2", rqst["resource"])
    local reader    = bathy.viewer(parms)
    local status    = reader:waiton(parms["timeout"] * 1000)
    if status then
        return json.encode(reader:counts())
    else
        return string.format("{\"message\":\"timeout reached <%d>\"}", parms["timeout"])
    end
end

-------------------------------------------------------
-- endpoint
-------------------------------------------------------
return {
    main = main,
    name = "Bathymetry Viewer",
    description = "Return number of photons in ATL03 granule within a bathymetry mask (s-series)",
    logging = core.CRITICAL,
    roles = {},
    signed = false,
    outputs = {"json"}
}

