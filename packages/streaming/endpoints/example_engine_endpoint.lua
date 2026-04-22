-------------------------------------------------------
-- main
-------------------------------------------------------
local function main()
    local json = require("json")
    local parm = json.decode(arg[1])
    local outp = msg.publish(_rqst.rspq)

    print("Output Queue", _rqst.rspq)

    outp:sendstring(parm["var4"]["type"].."\n")
    outp:sendstring(parm["var4"]["files"].."\n")
    outp:sendstring(parm["var4"]["format"].."\n")
end

-------------------------------------------------------
-- endpoint
-------------------------------------------------------
return {
    main = main,
    name = "Example Binary Endpoint",
    description = "Used by http_server selftest",
    logging = core.CRITICAL,
    roles = {},
    signed = false,
    outputs = {"binary"}
}