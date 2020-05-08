--
-- INPUT: `arg` array with only 1 string element
-- OUTPUT: return string
--
-- NOTES
--  1. By convention, both the input and the output are json objects
--  2. See scripts/test/pistache_endpoint.lua for how to execute this lua script
--

local json = require("json")
local parm = json.decode(arg[1])

print("var1", parm["var1"])
print("var2", parm["var2"])
print("var3", parm["var3"])

return "{ \"result\": \"Hello World\" }"
