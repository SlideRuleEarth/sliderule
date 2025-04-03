local runner = require("test_executive")
local glb = require("global")

-- Configure Running In Cloud --
local aws_rsps_code = core.servicecheck("http://169.254.169.254/latest/meta-data/", 1)
if aws_rsps_code >= 200 and aws_rsps_code < 500 then
    sys.setincloud(true)
end

-- Find All Self Test Directories --
local selftests_dir_list = {}
local td = runner.rootdir(arg[0]).."../../" -- root directory
local packages_dir_list = sys.pathfind(td.."packages", "selftests")
local datasets_dir_list = sys.pathfind(td.."datasets", "selftests")
table.move(packages_dir_list, 1, #packages_dir_list, #selftests_dir_list + 1, selftests_dir_list)
table.move(datasets_dir_list, 1, #datasets_dir_list, #selftests_dir_list + 1, selftests_dir_list)

-- Execute All Tests --
for _,test_dir in ipairs(selftests_dir_list) do
    local selftest_file_list = sys.filefind(test_dir, ".lua")
    for _,test_file in ipairs(selftest_file_list) do
        local s = glb.split(test_file, "/")
        local pkg = s[#s-2]
        local name = s[#s]
        if glb.check(string.format("__%s__", pkg)) then
            runner.script(test_file)
        else
            runner.skip(name)
        end
    end
end

-- Report Results --
local errors = runner.report()

-- Cleanup and Exit --
sys.quit( errors )
