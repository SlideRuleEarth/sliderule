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

function ascii_display(raster)
    rows, cols = raster:dim()
    print("DIM: ", rows, cols)
    for r = 0,rows-1,1 do
        for c = 0,cols-1,1 do
            io.write(raster:pixel(r, c) and "#" or " ")
        end
        io.write("\n")
    end
end

raster = geotiff.file(parm["raster"])
ascii_display(raster)

return json.encode({input=parm})
