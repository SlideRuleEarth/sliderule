local runner = require("test_executive")
local asset = require("asset")
local console = require("console")
local td = runner.rootdir(arg[0]) .. "../tests"

-- Setup --

console.logger:config(core.INFO)
assets = asset.load(td.."/asset_directory.csv")

expected = {
    dataset1={format="json", url="/data/1"},
    dataset2={format="binary", url="/data/2"}
}

-- Unit Test --

print('\n------------------\nTest01\n------------------')
for _,v in pairs(assets) do
    name, format, url, status = v:info()
    runner.compare(format, expected[name]["format"])
    runner.compare(url, expected[name]["url"])
    runner.check(status)
end

-- Clean Up --

-- Report Results --

runner.report()

