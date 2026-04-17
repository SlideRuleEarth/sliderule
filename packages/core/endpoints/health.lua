-------------------------------------------------------
-- main
-------------------------------------------------------
local function main()
    local json = require("json")
    local healthy = sys.healthy()
    return json.encode({healthy=healthy}), healthy
end

-------------------------------------------------------
-- endpoint
-------------------------------------------------------
return {
    main = main,
    name = "Health",
    description = "Application health",
    logging = core.DEBUG,
    roles = {},
    signed = false,
    outputs = {"json"}
}
