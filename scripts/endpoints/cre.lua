--
-- ENDPOINT:    /source/cre
--
-- INPUT:       arg[1]
--              {
--                  "parms":
--                  {
--                      "image":  "<container image>",
--                      "timeout": <timeout in seconds>,
--                      "userdata": "<string passed to running container>"
--                  }
--              }
--
-- OUTPUT:      <output from container>
--

local json  = require("json")
local rqst  = json.decode(arg[1])
local parms = rqst["parms"]

local cre_parms = cre.parms(parms)
local cre_runner = cre.container(cre_parms)

local result = cre_runner:result()

return result
