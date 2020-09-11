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

-- Unit Test --

print('\n------------------\nTest01: Print Info\n------------------')
for _,v in pairs(assets) do
    name, format, url, status = v:info()
    runner.compare(format, expected[name]["format"])
    runner.compare(url, expected[name]["url"])
    runner.check(status)
end

print('\n------------------\nTest02: Retrieve Existing Asset\n------------------')
a1 = core.asset("dataset1")
name, format, url, status = a1:info()
runner.compare(name, "dataset1")
runner.compare(format, expected["dataset1"]["format"])
runner.compare(url, expected["dataset1"]["url"])


print('\n------------------\nTest03: Display Time Tree for Dataset1\n------------------')
a1:display()


-- Clean Up --

-- Report Results --

runner.report()

