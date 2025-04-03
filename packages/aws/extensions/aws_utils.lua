
local function config_aws ()
    local status = false
    local aws_rsps_code = core.servicecheck("http://169.254.169.254/latest/meta-data/", 1)
    if aws_rsps_code >= 200 and aws_rsps_code < 500 then
        print("Executing in the cloud: "..tostring(aws_rsps_code))
        status = true
    else
        print("Executing locally: "..tostring(aws_rsps_code))
    end
    sys.setincloud(status)
    return status
end

return {
    config_aws = config_aws
}
