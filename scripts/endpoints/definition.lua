--
-- ENDPOINT:    /source/definition
--
-- INPUT:       arg[1]
--              {
--                  "rectype":  "<record type>"
--              }
--
-- OUTPUT:      <record definition>
--
-- NOTES:       1. Both the input and the output are json objects
--

local json = require("json")
local parm = json.decode(arg[1])

def = msg.definition(parm["rectype"])

return json.encode(def)
