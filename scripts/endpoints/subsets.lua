--
-- ENDPOINT:    /source/samples
--
-- PURPOSE:     return a subset from a raster dataset given an extent
--
-- INPUT:       rqst
--              {
--                  "subsets": {<geoparms>}
--                  "extents": [
--                      [<xmin>, <ymin>, <xmax>, <ymax>],
--                      [<xmin>, <ymin>, <xmax>, <ymax>]
--                  ]
--              }
--
-- OUTPUT:      samples
--

local json = require("json")

-- Request Parameters --
local rqst = json.decode(arg[1])
local extents = rqst["extents"]

-- Get Samples --
local dem = geo.raster(geo.parms(rqst[geo.PARMS]))

-- Build Table --
local subsets = {}
local errors = {}
if dem then
    for _, position in ipairs(extents) do
        local xmin = position[1]
        local ymin = position[2]
        local xmax = position[3]
        local ymax = position[4]
        local subset, error = dem:subset(xmin, ymin, xmax, ymax)
        table.insert(subsets, subset)
        table.insert(errors, error)
    end
end

-- Return Response
return json.encode({subsets=subsets, errors=errors})