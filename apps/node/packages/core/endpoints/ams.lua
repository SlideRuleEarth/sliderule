-------------------------------------------------------
-- main
-------------------------------------------------------
local function main()
    return core.ams("POST", _rqst.arg, arg[1]) -- TODO: this does not pull out timeout from parameters and use it in request
end

-------------------------------------------------------
-- endpoint
-------------------------------------------------------
return {
    main = main,
    parms = nil,
    name = "Asset Metadata Service",
    description = "Pass-through to Asset Metadata Service API",
    logging = core.INFO,
    roles = {},
    signed = false,
    inputs = {"json"},
    outputs = {"json"},
    schema = {
        tags = "a-series, core",
        request = [["application/json": {
            "schema": {
                "type": "object",
                "description": "See AMS specification for details"
            }
        }]],
        response = [["application/json": {
            "schema": {
                "type": "object",
                "description": "See AMS specification for details"
            }
        }]]
    }
}
