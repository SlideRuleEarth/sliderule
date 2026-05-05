-------------------------------------------------------
-- initialization
-------------------------------------------------------
local json  = require("json")
local rqst  = json.decode(arg[1])
local parms = icesat2.parms(rqst["parms"], rqst["key_space"], "icesat2", rqst["resource"])

-------------------------------------------------------
-- main
-------------------------------------------------------
local function main()
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
    parms = parms,
    name = "Bathymetry Viewer",
    description = "Return number of photons in ATL03 granule within a bathymetry mask (s-series)",
    logging = core.CRITICAL,
    roles = {},
    signed = false,
    inputs = {"json"},
    outputs = {"json"},
    schema = {
        request = [[ "application/json": {
            "schema": {
                "$ref": "#/components/schemas/BathyParameters"
            }
        } ]],
        response = [[ "application/json": {
            "schema": {
                "type": "object",
                "properties": {
                    "total_photons": { "type": "integer", "description": "total number of photons in the granule" },
                    "photons_in_mask": { "type": "integer", "description": "number of photons in the granule that fall within the bathymetry mask" },
                    "total_segments": { "type": "integer", "description": "total number of segments in the granule" },
                    "segments_in_mask": { "type": "integer", "description": "number of segments in the granule that fall within the bathymetry mask" },
                    "total_errors": { "type": "integer", "description": "number of errors that occurred when processing granule" }
                }
            }
        } ]]
    }
}

