local runner = require("test_executive")

-- Requirements --

if (not sys.getcfg("in_cloud") and not runner.isglobal()) then
    return runner.skip()
end

-- Setup --

runner.authenticate({})

local test_bucket = sys.getcfg("project_bucket")
local test_path = "data/test"
local test_file = "t8.shakespeare.txt"

-- Self Test --

runner.unittest("S3 failed download", function()
    local status = aws.s3download(test_bucket, string.format("%s/%s", test_path, test_file), "/missing_path/missing_file.txt")
    runner.assert(status == false, "should have failed when trying to write to a non-existent path")
end)

-- Self Test --

runner.unittest("S3 successful download", function()
    local status = aws.s3download(test_bucket, string.format("%s/%s", test_path, test_file), test_file)
    runner.assert(status == true, "failed to download file: "..test_file)
    local f = io.open(test_file, "r")
    local fsize = f:seek("end")
    f:close()
    runner.assert(fsize == 5458199, "failed to read entire file: "..test_file)
end)

-- Self Test --

runner.unittest("S3 range read", function()
    local response, status = aws.s3read(test_bucket, string.format("%s/%s", test_path, test_file), 11, 261)
    runner.assert(status == true, "failed to read file: "..test_file)
    runner.assert(response == "Shakespeare")
end)

-- Clean Up --

os.remove(test_file)

-- Report Results --

runner.report()
