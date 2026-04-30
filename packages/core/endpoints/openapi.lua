-------------------------------------------------------
-- initialization
-------------------------------------------------------
local json  = require("json")
local parms = nil

-------------------------------------------------------
-- main
-------------------------------------------------------
local function main()
    local spec = [[{"openapi": "3.0.3", "info": {"title": "SlideRule OpenAPI Specification", "version": "%s"}, "components": {"schemas": %s}}]]
    local schemas = {}
    if not _rqst.arg or #_rqst.arg == 0 then
        table.insert(schemas, core.parms():describe("core", "General request parameters that support all requests"))
    elseif _rqst.arg == "cre" then
        table.insert(schemas, cre.parms():describe("cre", "Request parameters for executing container runtime environment processing requests"))
    elseif _rqst.arg == "icesat2" then
        table.insert(schemas, icesat2.parms():describe("icesat2", "Request parameters for executing ICESat-2 processing requests"))
    elseif _rqst.arg == "gedi" then
        table.insert(schemas, icesat2.parms():describe("gedi", "Request parameters for executing GEDI processing requests"))
    elseif _rqst.arg == "swot" then
        table.insert(schemas, icesat2.parms():describe("swot", "Request parameters for executing SWOT processing requests"))
    elseif _rqst.arg == "bathy" then
        table.insert(schemas, icesat2.parms():describe("bathy", "Request parameters for executing bathymetry processing requests"))
    end
    local version_str = sys.version()
    local schema_str = table.concat(schemas, ",")
    return string.format(spec, version_str, schema_str)
end

-------------------------------------------------------
-- endpoint
-------------------------------------------------------
return {
    main = main,
    parms = parms,
    name = "Open API Specification",
    description = "Return the Open API specification for the compliant endpoints, parameters, and dataframes",
    logging = core.CRITICAL,
    roles = {},
    signed = false,
    outputs = {"text"}
}