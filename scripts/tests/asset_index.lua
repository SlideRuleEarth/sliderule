local runner = require("test_executive")
local asset = require("asset")
local console = require("console")
local td = runner.rootdir(arg[0]) .. "../tests"

-- Setup --

console.logger:config(core.INFO)
assets = asset.loaddir(td.."/asset_directory.csv")

expected = {
    dataset1={format="json", url="/data/1"},
    dataset2={format="binary", url="/data/2"}
}

local function check_query(act, exp)
    for _, e in pairs(exp) do
        found = false
        for _, v in pairs(act) do
            if tostring(e) == v then
                found = true
            end
        end
        runner.check(found, string.format('Failed to return resource %d', e))
    end
end

-- Unit Test --

print('\n------------------\nTest01: Print Info\n------------------')
for _,v in pairs(assets) do
    name, format, url, status = v:info()
    runner.compare(format, expected[name]["format"])
    runner.compare(url, expected[name]["url"])
    runner.check(status)
end

print('\n------------------\nTest02: Retrieve Existing Asset\n------------------')
a2 = core.asset("dataset1")
name, format, url, status = a2:info()
runner.compare(name, "dataset1")
runner.compare(format, expected["dataset1"]["format"])
runner.compare(url, expected["dataset1"]["url"])

print('\n------------------\nTest03: Display Time Tree for Dataset1\n------------------')
local i3 = core.timeindex(a2)
i3:display()
sys.wait(1)

print('\n------------------\nTest04: Query Dataset1\n------------------')
local r4 = i3:query({t0=5.0, t1=17.0})
local e4 = { 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17} 
check_query(r4, e4)

print('\n------------------\nTest05: Query Dataset1 with Field Index\n------------------')
local a5 = core.asset("dataset1") -- alias
local f5 = core.fieldindex(a5, "foot")
local r5 = f5:query({foot=15})
local e5 = { 1, 4, 7, 10, 13, 14, 17, 18, 21, 22, 25, 26, 29, 30, 33, 34, 37, 38, 41, 42, 45} 
check_query(r5, e5)

-- Clean Up --

-- Report Results --

runner.report()

