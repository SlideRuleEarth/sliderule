-------------------------------------------------------
-- initialization
-------------------------------------------------------
local json  = require("json")
local parms = json.decode(arg[1])

-------------------------------------------------------
-- main
-------------------------------------------------------
local function main()
    print("var1", parms["var1"])
    print("var2", parms["var2"])
    print("var3", parms["var3"])
    return "{ \"result\": \"Hello World\" }"
end

-------------------------------------------------------
-- endpoint
-------------------------------------------------------
return {
    main = main,
    parms = parms,
    name = "Example JSON Endpoint",
    description = "Used by http_server selftest",
    logging = core.CRITICAL,
    roles = {},
    signed = false,
    outputs = {"json"}
}