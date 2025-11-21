--
-- ENDPOINT:    /source/earthdata
--
local json = require("json")
local earthdata = require("earth_data_query")
local parm = json.decode(arg[1])
local status,rsps = earthdata.search(parm, parm["api"])
if status == earthdata.SUCCESS then
    return json.encode(rsps)
else
    return json.encode({error=rsps, code=status})
end