-------------------------------------------------------
-- main
-------------------------------------------------------
local function main()
    return core.ams("POST", _rqst.arg, arg[1])
end

-------------------------------------------------------
-- endpoint
-------------------------------------------------------
return {
    main = main,
    parms = nil,
    name = "Asset Metadata Service",
    description = "Pass-through to Asset Metadata Service API",
    logging = core.CRITICAL,
    roles = {},
    signed = false,
    inputs = {"json"},
    outputs = {"json"},
    schema = {
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
