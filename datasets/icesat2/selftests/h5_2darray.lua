local runner = require("test_executive")
local asset = require("asset")
local json = require("json")
local pp = require("prettyprint")

-- Requirements --

if (not sys.getcfg("in_cloud") and not runner.isglobal()) then
    return runner.skip()
end

-- Setup --

runner.authenticate()

local atlas_asset = core.getbyname("icesat2")
local resource = "ATL03_20190218011204_07880205_006_02.h5"
local f = h5.file(atlas_asset, resource):name(resource)
local rspq = msg.subscribe("h5testq")

-- Self Test 1 --

runner.unittest("Small Array", function()
    local var = {dataset="gt1r/heights/signal_conf_ph", col=3, startrow=580000, numrows=10}
    f:read({var}, "h5testq", false)
    local recdata = rspq:recvrecord(3000)
    local rectable = recdata:tabulate()
    pp.display(rectable)

    runner.assert(rectable.data[1] == 4)
    runner.assert(rectable.data[2] == 4)
    runner.assert(rectable.data[3] == 4)
    runner.assert(rectable.data[4] == 4)
    runner.assert(rectable.data[5] == 4)
    runner.assert(rectable.data[6] == 4)
    runner.assert(rectable.data[7] == 4)
    runner.assert(rectable.data[8] == 4)
    runner.assert(rectable.data[9] == 4)
    runner.assert(rectable.data[10] == 4)
end)

-- Self Test 2 --

runner.unittest("Large Array", function()
    local all_good = true
    local startrow = 0
    local numrows = 10000
    local totalrows = 5231049
    while all_good do

        if startrow + numrows >= totalrows then
            numrows = totalrows - startrow
            all_good = false
        end

        local var = {dataset="gt1r/heights/signal_conf_ph", col=-1, startrow=startrow, numrows=numrows}
        f:read({var}, "h5testq", false)

        local recdata = rspq:recvrecord(3000)
        if not recdata then
            break
        end
        local rectable = recdata:tabulate()

        for _,v in ipairs(rectable.data) do
            runner.assert(((v >= 0) and (v < 5)) or (v == 255))
        end

        startrow = startrow + numrows
    end
end)

-- Clean Up --

rspq:destroy()
f:destroy()

-- Report Results --

runner.report()
