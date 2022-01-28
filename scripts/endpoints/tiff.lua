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
local base64 = require("base64")
local parm = json.decode(arg[1])

print("DECODE RASTER")
local tiff = base64.decode(parm["raster"])
local size = parm["imagelength"]

print("SCAN RASTER")
geotiff.scan(tiff, size)

return json.encode({complete=true})
