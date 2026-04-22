-------------------------------------------------------
-- main
-------------------------------------------------------
local function main()
    local json = require("json")
    local parm = json.decode(arg[1])

    print("var1", parm["var1"])
    print("var2", parm["var2"])
    print("var3", parm["var3"])

    return "{ \"result\": \"Hello World\" }"
end

-------------------------------------------------------
-- endpoint
-------------------------------------------------------
return {
    main = main,
    name = "Example JSON Endpoint",
    description = "Used by http_server selftest",
    logging = core.CRITICAL,
    roles = {},
    signed = false,
    outputs = {"json"}
}