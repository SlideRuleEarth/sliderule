local runner = require("test_executive")

-- Requirements --

if (not sys.getcfg("in_cloud") and not runner.isglobal()) then
    return runner.skip()
end

-- Setup --

core.script("iam_role_auth")
local test_bucket = sys.getcfg("sys_bucket")
local test_path = "data/test"
local test_file = "t8.shakespeare.txt"
local status = false

sys.wait(2)

-- Self Test --

-- TEST #1: failed download
status = aws.s3download(test_bucket, string.format("%s/%s", test_path, test_file), nil, nil, "/missing_path/missing_file.txt")
runner.assert(status == false, "should have failed when trying to write to a non-existent path")

-- TEST #2: successful download
status = aws.s3download(test_bucket, string.format("%s/%s", test_path, test_file), nil, nil, test_file)
runner.assert(status == true, "failed to download file: "..test_file)
local f = io.open(test_file, "r")
local fsize = f:seek("end")
f:close()
runner.assert(fsize == 5458199, "failed to read entire file: "..test_file)

-- TEST #3: range read
local response, status = aws.s3read(test_bucket, string.format("%s/%s", test_path, test_file), 11, 261, nil, nil)
runner.assert(status == true, "failed to read file: "..test_file)
runner.assert(response == "Shakespeare")

-- Clean Up --

os.remove(test_file)

-- Report Results --

runner.report()

