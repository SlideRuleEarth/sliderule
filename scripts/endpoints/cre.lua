--
-- ENDPOINT:    /source/cre
--
-- INPUT:       arg[1]
--              {
--                  "image":  "<container image>"
--                  "timeout": <timeout in seconds>
--              }
--
-- OUTPUT:      <output from container>
--

local json = require("json")
local parm = json.decode(arg[1])

local cre_parms = cre.parms(parm)
local cre_runner = cre.container(cre_parms)

local result = cre_runner:result()

return result
