-------------------------------------------------------
-- initialization
-------------------------------------------------------
local json  = require("json")
local parms = json.decode(arg[1])
local outp  = msg.publish(_rqst.rspq)

-------------------------------------------------------
-- main
-------------------------------------------------------
local function main()
    print("Output Queue", _rqst.rspq)
    outp:sendstring(parms["var4"]["type"].."\n")
    outp:sendstring(parms["var4"]["files"].."\n")
    outp:sendstring(parms["var4"]["format"].."\n")
end

-------------------------------------------------------
-- endpoint
-------------------------------------------------------
return {
    main = main,
    parms = parms,
    name = "Example Binary Endpoint",
    description = "Used by http_server selftest",
    logging = core.CRITICAL,
    roles = {},
    signed = false,
    outputs = {"binary"}
}