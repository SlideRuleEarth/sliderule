local runner = require("test_executive")
console = require("console")
asset = require("asset")
csv = require("csv")
json = require("json")

-- console.monitor:config(core.LOG, core.DEBUG)
-- sys.setlvl(core.LOG, core.DEBUG)

local assets = asset.loaddir()

-- Unit Test --

local sigma = 1.0e-9

-- AOI extent
local llx =  149.80
local lly =  -70.00
local urx =  150.00
local ury =  -69.95

local demTypes = {"rema-mosaic", "rema-strips"}
for i = 1, #demTypes do
    local demType = demTypes[i];
    local dem = geo.raster(geo.parms({asset=demType, algorithm="NearestNeighbour", radius=50, zonal_stats=true}))
    runner.check(dem ~= nil)
    print(string.format("\n--------------------------------\nTest: %s AOI subset\n--------------------------------", demType))

    local err = dem:subsettest(llx, lly, urx, ury)
    runner.check(err == 0)
end


-- Report Results --

runner.report()

