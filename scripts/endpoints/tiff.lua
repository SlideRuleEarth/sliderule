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

local tiff = base64.decode(parm["raster"])
local size = parm["imagelength"]

raster = geotiff.scan(tiff, size)
rows, cols = raster:dim()
print("DIM: ", rows, cols)
for r = 0,rows,1 do
    for c = 0,cols,1 do
        io.write(raster:pixel(r, c) and "#" or " ")
    end
    io.write("\n")
end

return json.encode({complete=true})
