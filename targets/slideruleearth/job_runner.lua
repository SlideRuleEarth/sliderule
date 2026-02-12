local aws_utils = require("aws_utils")
local script = arg[1]
local result = arg[#arg]
local script_file = nil
local result_file = nil

--------------------------------------------------
-- Unique String (Helper Function)
--------------------------------------------------

local function unique_string (nchars)
    local unique = {}
    for i = 1, nchars do
        local selection = math.random(1, 3) -- digit, lower alpha, upper alpha
        local possibilities = {
            math.random(48, 57), -- 0 to 9
            math.random(97, 122), -- a to z
            math.random(65, 90) -- A to Z
        }
        table.insert(unique, string.char(possibilities[selection]))
    end
    return table.concat(unique)
end

--------------------------------------------------
-- Validation
--------------------------------------------------

if not sys.getcfg("trusted_environment") then
    print("Can only execute in a trusted environment")
    sys.quit(1) -- failure
end

print("Executing job runner in trusted environment")

--------------------------------------------------
-- Get Script
--------------------------------------------------

if script:find("s3://") == 1 then
    script_file = string.format("/tmp/script-%s.lua", unique_string(7))
    local script_bucket, script_file_path = script:match("^s3://([^/]+)/(.+)$")
    local script_download_status = aws.s3download(script_bucket, script_file_path, script_file)
    if not script_download_status then
        print("Failed to download script from s3")
        sys.quit(1) -- failure
    end
else
    script_file = script
end

print(string.format("Running script: %s", script_file))

--------------------------------------------------
-- System Configuration
--------------------------------------------------

aws_utils.config_aws() -- in cloud
aws_utils.config_monitoring() -- logs (stdout, firehose)
aws_utils.config_leap_seconds() -- leap seconds
aws_utils.config_earth_data() -- assets and credentials

print("Cloud environment initialized")

--------------------------------------------------
-- Execute Script
--------------------------------------------------

local script_result, script_status = dofile(script_file)
if not script_status then
    print("Script failed execution", script_result)
    sys.quit(1) -- failure
end

if result:find("s3://") == 1 then
    result_file = string.format("/tmp/result-%s.json", unique_string(7))
else
    result_file = result
end

local f = io.open(result_file, "w")
if not f then
    print("Failed to open local result file")
    sys.quit(1) -- failure
else
    f:write(script_result)
    f:close()
end

print(string.format("Results written to: %s", result_file))

--------------------------------------------------
-- Put Result
--------------------------------------------------

if result_file:find("s3://") == 1 then
    local result_bucket, result_file_path = result:match("^s3://([^/]+)/(.+)$")
    local result_upload_status = aws.s3upload(result_bucket, result_file_path, result_file)
    if not result_upload_status then
        print("Failed to upload results to s3")
        sys.quit(1) -- failure
    end
end

sys.quit(0) -- success
