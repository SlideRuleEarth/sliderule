-------------------------------------------------------
-- initialization
-------------------------------------------------------
local json  = require("json")

-------------------------------------------------------
-- main
-------------------------------------------------------
local function main()
    local healthy = sys.healthy()
    return json.encode({healthy=healthy}), healthy
end

-------------------------------------------------------
-- endpoint
-------------------------------------------------------
return {
    main = main,
    parms = nil,
    name = "Health",
    description = "Application health",
    logging = core.DEBUG,
    roles = {},
    signed = false,
    inputs = nil,
    outputs = {"json"},
    schema = {
        tags = "a-series, core",
        request = nil,
        response = [["application/json": {
            "schema": {
                "type": "object",
                "properties": {
                    "health": { "type": "boolean", "description": "health of the system" }
                }
            }
        }]]
    }
}
