local json = require("json")
local aws_utils = require("aws_utils")
local script = arg[1]
local output = arg[#arg] -- directory

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
    local script_download_status = aws.s3download(script_bucket, script_file_path, local_script_file)
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

local output_bucket, output_directory = output:match("^s3://([^/]+)/(.+)$")
local array_index = tonumber(os.getenv("AWS_BATCH_JOB_ARRAY_INDEX"))
if array_index then
    local arguments_file_path = string.format("%s/args.json", output_directory)
    local local_arguments_file = string.format("/tmp/args-%s.json", aws_utils.unique_string(7))
    local arguments_download_status = aws.s3download(output_bucket, arguments_file_path, local_arguments_file)
    if not arguments_download_status then
        print("Failed to download arguments from s3://%s/%s", output_bucket, arguments_file_path)
        return sys.quit(1) -- failure
    end
    local f, err = io.open(local_arguments_file, "r")
    if not f then
        print("Failed to open arguments from s3: ", err)
        return sys.quit(1) -- failure
    end
    local content = f:read("*a")
    f:close()
    local rc, local_arguments = pcall(json.decode, content)
    if (not rc) or (type(local_arguments) ~= 'table') then
        print("Failed to parse arguments from s3")
        return sys.quit(1) -- failure
    elseif #local_arguments < array_index then
        print(string.format("Argument array index is out of bounds, %d < %d", #local_arguments, array_index))
        return sys.quit(1) -- failure
    end
    Arguments = local_arguments[array_index + 1]
else
    Arguments = arg[2]
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
    local_result_file = string.format("/tmp/result-%s.json", aws_utils.unique_string(7))
else
    local_result_file = string.format("%s/result.json", output)
end

local f, err = io.open(local_result_file, "w")
if not f then
    print("Failed to open local result file: ", err)
    return sys.quit(1) -- failure
end
f:write(script_result)
f:close()

print(string.format("Results written to: %s", local_result_file))

--------------------------------------------------
-- Put Result
--------------------------------------------------

if output:find("s3://") == 1 then
    local remote_result_file = string.format("%s/result%s.json", output_directory, array_index or "")
    print(string.format("Uploading bucket=%s, file=%s", output_bucket, remote_result_file))
    local result_upload_status = aws.s3upload(output_bucket, remote_result_file, local_result_file)
    if not result_upload_status then
        print("Failed to upload results to s3")
        return sys.quit(1) -- failure (with cleanup)
    end
end

sys.abort(0) -- success
