--
-- INPUT: `arg` array with only 1 string element
--        'rspq' string containing name of message queue for output
-- OUTPUT: binary data posted to the rspq
--
-- NOTES
--  1. By convention, the input is a json object
--  2. Anything output from this script is ignored
--  3. The response will conclude when the script returns and the output message queue is empty
--  4. See scripts/test/pistache_endpoint.lua for how to execute this lua script
--

local json = require("json")
local parm = json.decode(arg[1])
local outp = msg.publish(rspq)

print("Output Queue", rspq)

outp:sendstring(parm["var4"]["type"].."\n")
outp:sendstring(parm["var4"]["files"].."\n")
outp:sendstring(parm["var4"]["format"].."\n")
outp:sendstring("")

return
