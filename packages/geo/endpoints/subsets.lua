-------------------------------------------------------
-- initialization
-------------------------------------------------------
local json      = require("json")
local parms     = json.decode(arg[1])
local extents   = parms["extents"]

-------------------------------------------------------
-- main
-------------------------------------------------------
local function main()

    -- Get Samples --
    local dem = geo.raster(geo.parms(parms[geo.PARMS]))

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

end

-------------------------------------------------------
-- endpoint
-------------------------------------------------------
return {
    main = main,
    parms = parms,
    name = "Raster Subsetter",
    description = "Return a subset from a raster dataset given an extent",
    logging = core.CRITICAL,
    roles = {},
    signed = false,
    outputs = {"json"}
}

-- INPUT
--  {
--      "subsets": {<geoparms>}
--      "extents": [
--          [<xmin>, <ymin>, <xmax>, <ymax>],
--          [<xmin>, <ymin>, <xmax>, <ymax>]
--      ]
--  }