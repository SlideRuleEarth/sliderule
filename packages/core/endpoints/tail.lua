--
-- ENDPOINT:    /source/tail
--
-- INPUT:       arg[1] -
--              {
--                  "monitor":  <name of monitor to tail>
--              }
--
-- OUTPUT:      application event messages
--

local json = require("json")
local parm = json.decode(arg[1])

local monitor_name = parm["monitor"]
local monitor = core.getbyname(monitor_name)

local msgs = {}
if monitor then
    msgs = monitor:cat(1) --  1: LOCAL mode
end

return json.encode(msgs)
