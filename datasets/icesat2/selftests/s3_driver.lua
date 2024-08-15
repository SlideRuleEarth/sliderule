local runner = require("test_executive")
local console = require("console")
local json = require("json")
local pp = require("prettyprint")

-- Setup --

local role_auth_script = core.script("iam_role_auth"):name("RoleAuthScript")
local test_bucket = "sliderule"
local test_path = "data/test"
local test_file = "t8.shakespeare.txt"
local status = false

sys.wait(2)

-- Unit Test --

-- TEST #1: failed download
status = aws.s3download(test_bucket, string.format("%s/%s", test_path, test_file), nil, nil, "/missing_path/missing_file.txt")
runner.check(status == false, "should have failed when trying to write to a non-existent path")

-- TEST #2: successful download
status = aws.s3download(test_bucket, string.format("%s/%s", test_path, test_file), nil, nil, test_file)
runner.check(status == true, "failed to download file: "..test_file)
local f = io.open(test_file, "r")
local fsize = f:seek("end")
f:close()
runner.check(fsize == 5458199, "failed to read entire file: "..test_file)

-- TEST #3: range read
local response, status = aws.s3read(test_bucket, string.format("%s/%s", test_path, test_file), 11, 261, nil, nil)
runner.check(status == true, "failed to read file: "..test_file)
runner.check(response == "Shakespeare")

-- Clean Up --

os.remove(test_file)

-- Report Results --

runner.report()

