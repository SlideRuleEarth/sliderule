
local function config_aws ()
    local status = false
    local http_code = core.servicecheck("http://169.254.169.254/latest/meta-data/", 1)
    if (http_code >= 200 and http_code < 300) or (http_code == 401) then
        print("Executing in the cloud: "..tostring(http_code))
        status = true
    else
        print("Executing locally: "..tostring(http_code))
    end
    sys.setcfg("in_cloud", status)
    return status
end

return {
    config_aws = config_aws
}
