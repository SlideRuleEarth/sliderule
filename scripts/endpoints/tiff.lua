--
-- ENDPOINT:    /source/tiff
--
-- INPUT:       arg[1] -
--              {
--                  "raster":  "<base64 encoded geotiff file>"
--              }
--
-- OUTPUT:      raster processing
--

local json = require("json")
local parm = json.decode(arg[1])
print(parm["raster"])
return json.encode({healthy=true})
