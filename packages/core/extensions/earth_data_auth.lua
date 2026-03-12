--
-- Maintains up-to-date credentials for earthdata login
--
--  NOTES:  The code below uses libcurl to
--          issue a request to earthdatacloud.nasa.gov to get S3 credentials.
--          It then saves those credentials into SlideRule's credential store
--          so that when H5Coro issues a S3 request, it can use them to authenticate
--
--          The equivalent curl command (which uses a netrc file) to get the credentials is:
--
--              curl -L -n -c .cookies https://data.{daac}.earthdatacloud.nasa.gov/s3credentials
--

local json = require("json")
local parm = json.decode(arg[1] or "{}")

local earthdata = parm["earthdata"]
local identity = parm["identity"]

-- maintain earth data credentials
while sys.alive() do
    sys.log(core.DEBUG, "Fetching Earthdata Credentials...")

    -- get new credentials
    local response, status = core.get(earthdata, nil, nil, nil, nil, nil, aws.secret("secrets", "edl_username"), aws.secret("secrets", "edl_password"))

    -- convert reponse to credential table
    if status then
        local rc, credential = pcall(json.decode, response)

        -- store credentials and wait for next time to update
        if rc and credential and credential.expiration then
            sys.log(core.INFO, string.format("New earthdata credentials fetched for %s, expiration: %s", identity, credential.expiration))

            -- store credentials for use by server
            aws.csput(identity, credential)

            -- calculate gps times
            local now = time.gps()
            local credential_gps = time.gmt2gps(credential.expiration)

            -- calculate how long credentials are good for
            local credential_duration = credential_gps - now
            if credential_duration < 120000 then -- fetch no faster than once per minute (120 seconds / 2 = 1 minute)
                sys.log(core.CRITICAL, string.format("Invalid earthdata credentials expiration of %f seconds detected for %s, retrying in 60 seconds", credential_duration / 1000.0, identity))
                credential_duration = 120000
            elseif credential_duration > 43200000 then -- fetch no slower than once per 6 hours (12 hours / 2 = 6 hours)
                sys.log(core.CRITICAL, string.format("Invalid earthdata credentials expiration of %f hours detected for %s, retrying in 6 hours", credential_duration / 1000.0 / 60.0 / 60.0, identity))
                credential_duration = 43200000
            end

            -- calculate next fetch time
            local next_fetch_time = now + (credential_duration / 2)

            -- wait until next fetch time
            while sys.alive() and time.gps() < next_fetch_time do
                sys.wait(5)
            end
        else
            sys.log(core.CRITICAL, string.format("Failed to decode earthdata credentials for %s, retrying in 60 seconds", identity))
            sys.wait(60)
        end
    else
        sys.log(core.CRITICAL, string.format("Failed to retrieve earthdata credentials for %s, retrying in 60 seconds", identity))
        sys.wait(60)
    end
end
