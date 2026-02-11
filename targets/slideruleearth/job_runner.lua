local aws_utils = require("aws_utils")
local script = arg[1]
local result = arg[#arg]
local tmp_script_file = "/tmp/script.lua"
local tmp_result_file = "/tmp/result.json"

--------------------------------------------------
-- Parameters & Validation
--------------------------------------------------

if not sys.getcfg("trusted_environment") then
    print("can only execute in a trusted environment")
    sys.quit(1) -- failure
end

local script_bucket, script_file_path = script:match("^s3://([^/]+)/(.+)$")
local script_download_status = aws.s3download(script_bucket, script_file_path, tmp_script_file)

if not script_download_status then
    print("failed to download script from s3")
    sys.quit(1) -- failure
end

--------------------------------------------------
-- System Configuration
--------------------------------------------------

aws_utils.config_aws() -- in cloud
aws_utils.config_monitoring() -- logs (stdout, firehose)
aws_utils.config_leap_seconds() -- leap seconds
aws_utils.config_earth_data() -- assets and credentials

--------------------------------------------------
-- Execute Script
--------------------------------------------------

local script_result, script_status = dofile(tmp_script_file)

if not script_status then
    print("script failed execution", script_result)
    sys.quit(1) -- failure
end

local f = io.open(tmp_result_file, "w")
if not f then
    print("failed to open local result file")
    sys.quit(1) -- failure
else
    f:write(script_result)
    f:close()
end

local result_bucket, result_file_path = result:match("^s3://([^/]+)/(.+)$")
local result_upload_status = aws.s3upload(result_bucket, result_file_path, tmp_result_file)

if not result_upload_status then
    print("failed to upload results to s3")
    sys.quit(1) -- failure
end

sys.quit(0) -- success
