local json = require("json")
local aws_utils = require("aws_utils")
local script = arg[1]
local args_url = arg[2]
local output = arg[3] -- directory

--------------------------------------------------
-- Validation
--------------------------------------------------

io.write("Executing job: ")
for _,a in ipairs(arg) do
    io.write(a)
    io.write(" ")
end
io.write("\n")

--------------------------------------------------
-- System Configuration
--------------------------------------------------

aws_utils.config_aws() -- in cloud
aws_utils.config_monitoring() -- logs (stdout, firehose)
aws_utils.config_leap_seconds() -- leap seconds
aws_utils.config_earth_data() -- assets and credentials

print("Cloud environment initialized")

--------------------------------------------------
-- Get Script
--------------------------------------------------

local local_script_file = nil
if script:find("s3://") == 1 then
    local_script_file = string.format("/tmp/script-%s.lua", aws_utils.unique_string(7))
    local script_bucket, script_file_path = script:match("^s3://([^/]+)/(.+)$")
    print(string.format("Downloading bucket=%s, file=%s", script_bucket, script_file_path))
    local script_download_status = core.s3download(script_bucket, script_file_path, local_script_file)
    if not script_download_status then
        print("Failed to download script from s3")
        return sys.quit(1) -- failure
    end
else
    local_script_file = script
end

print(string.format("Running script: %s", local_script_file))

--------------------------------------------------
-- Get Arguments
--------------------------------------------------

-- get local arguments file
local local_arguments_file = nil
if args_url:find("s3://") == 1 then
    local_arguments_file = string.format("/tmp/args-%s.json", aws_utils.unique_string(7))
    local arguments_bucket, arguments_file_path = args_url:match("^s3://([^/]+)/(.+)$")
    print(string.format("Downloading bucket=%s, file=%s", arguments_bucket, arguments_file_path))
    local arguments_download_status = core.s3download(arguments_bucket, arguments_file_path, local_arguments_file)
    if not arguments_download_status then
        print("Failed to download arguments from s3")
        return sys.quit(1) -- failure
    end
else
    local_arguments_file = args_url
end

-- get arguments file content
local args_f, args_err = io.open(local_arguments_file, "r")
if not args_f then
    print("Failed to open arguments file: ", args_err)
    return sys.quit(1) -- failure
end
local args_content = args_f:read("*a")
args_f:close()

-- get arguments for script
local array_index = tonumber(os.getenv("AWS_BATCH_JOB_ARRAY_INDEX"))
if array_index then -- args as array
    local args_rc, args_json = pcall(json.decode, args_content)
    if (not args_rc) or (type(args_json) ~= 'table') then
        print("Failed to parse arguments from s3")
        return sys.quit(1) -- failure
    elseif #args_json < array_index then
        print(string.format("Argument array index is out of bounds, %d < %d", #args_json, array_index))
        return sys.quit(1) -- failure
    end
    Arguments = args_json[array_index + 1]
else
    Arguments = args_content
end

--------------------------------------------------
-- Execute Script
--------------------------------------------------

local ok, script_result = pcall(dofile, local_script_file)
if (not ok) or (not script_result) then
    print("Script failed execution", script_result)
    return sys.quit(1) -- failure
end

local local_result_file = nil
if output:find("s3://") == 1 then
    local_result_file = string.format("/tmp/result%s.json", aws_utils.unique_string(7))
else
    local_result_file = string.format("%s/result.json", output)
end

local result_f, result_err = io.open(local_result_file, "w")
if not result_f then
    print("Failed to open local result file: ", result_err)
    return sys.quit(1) -- failure
end
result_f:write(script_result)
result_f:close()

print(string.format("Results written to: %s", local_result_file))

--------------------------------------------------
-- Put Result
--------------------------------------------------

if output:find("s3://") == 1 then
    local output_bucket, output_directory = output:match("^s3://([^/]+)/(.+)$")
    local remote_result_file = string.format("%s/result%s.json", output_directory, array_index or "")
    print(string.format("Uploading bucket=%s, file=%s", output_bucket, remote_result_file))
    local result_upload_status = core.s3upload(output_bucket, remote_result_file, local_result_file)
    if not result_upload_status then
        print("Failed to upload results to s3")
        return sys.quit(1) -- failure (with cleanup)
    end
end

sys.abort(0) -- success
