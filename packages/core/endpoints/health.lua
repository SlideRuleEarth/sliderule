-------------------------------------------------------
-- initialization
-------------------------------------------------------
local json  = require("json")
local parms = nil

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
    parms = parms,
    name = "Health",
    description = "Application health",
    logging = core.DEBUG,
    roles = {},
    signed = false,
    outputs = {"json"}
}
