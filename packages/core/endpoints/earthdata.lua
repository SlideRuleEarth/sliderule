--
-- ENDPOINT:    /source/earthdata
--
-- INPUT:       none
--
-- OUTPUT:      json string of default parameters
--
local json = require("json")
local earthdata = require("earth_data_query")
local parm = json.decode(arg[1])
local status,rsps = earthdata.search(parm)
if status == earthdata.SUCCESS then
    return json.encode(rsps)
else
    return json.encode({error=rsps})
end

