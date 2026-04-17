-------------------------------------------------------
-- main
-------------------------------------------------------
local function main()
    local json = require("json")
    local parm = json.decode(arg[1])
    local def = msg.definition(parm["rectype"])
    return json.encode(def)
end

-------------------------------------------------------
-- endpoint
-------------------------------------------------------
return {
    main = main,
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
