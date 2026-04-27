-------------------------------------------------------
-- initialization
-------------------------------------------------------
local json  = require("json")
local parms = json.decode(arg[1])

-------------------------------------------------------
-- main
-------------------------------------------------------
local function main()
    local def = msg.definition(parms["rectype"])
    return json.encode(def)
end

-------------------------------------------------------
-- endpoint
-------------------------------------------------------
return {
    main = main,
    parms = parms,
    name = "Record Definition",
    description = "Defines the on-the-wire format and structure for a given record type",
    logging = core.DEBUG,
    roles = {},
    signed = false,
    outputs = {"json"}
}

-- INPUT
--  {
--      "rectype":  "<record type>"
--  }
