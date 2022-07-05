--
-- Maintains up-to-date credentials for earthdata login
--
--  NOTES:  The code below uses libcurl (via the netsvc package) to
--          issue a request to earthdatacloud.nasa.gov to get S3 credentials.
--          It then saves those credentials into SlideRule's credential store
--          so that when H5Coro issues a S3 request, it can use them to authenticate
--
--          The equivalent curl command to get the credentials is:
--
--              curl -L -n -c .cookies https://data.nsidc.earthdatacloud.nasa.gov/s3credentials
--

local json = require("json")

local earthdata_url = "https://data.nsidc.earthdatacloud.nasa.gov/s3credentials"
local asset = "nsidc-s3"

-- download netrc
netrc_written = false
netrc = aws.s3get("icesat2-sliderule", "/config/netrc")
if netrc then
    local home = os.getenv("HOME")
    local f = io.open(home.."/.netrc", "w")
    if f ~= nil then
        f:write(netrc)
        f:close()
        netrc_written = true
        sys.log(core.INFO, "Copied netrc file to "..home)
    else
        sys.log(core.CRITICAL, "Failed to write netrc file")
    end
else
    sys.log(core.CRITICAL, "Failed to retrieve netrc file")
end

-- maintain earth data credentials
while netrc_written and sys.alive() do
    sys.log(core.DEBUG, "Fetching Earth Data Credentials...")

    -- get new credentials
    response, status = netsvc.get(earthdata_url)

    -- convert reponse to credential table
    if status then
        rc, credential = pcall(json.decode, response)
    end

    -- store credentials and wait for next time to update
    if status and rc and credential and credential.expiration then
        sys.log(core.INFO, string.format("New credentials fetched, expiration: %s", credential.expiration))

        -- store credentials for use by server
        aws.csput(asset, credential)

        -- calculate gps times
        now = time.gps()
        credential_gps = time.gmt2gps(credential.expiration)

        -- calculate how long credentials are good for
        credential_duration = credential_gps - now
        if credential_duration < 120000 then -- fetch no faster than once per minute (120 seconds / 2 = 1 minute)
            sys.log(core.CRITICAL, string.format("Invalid expiration of %f seconds detected, retrying in 60 seconds", credential_duration / 1000.0))
            credential_duration = 120000
        elseif credential_duration > 43200000 then -- fetch no slower than once per 6 hours (12 hours / 2 = 6 hours)
            sys.log(core.CRITICAL, string.format("Invalid expiration of %f hours detected, retrying in 6 hours", credential_duration / 1000.0 / 60.0 / 60.0))
            credential_duration = 43200000
        end

        -- calculate next fetch time
        next_fetch_time = now + (credential_duration / 2)

        -- wait until next fetch time
        while sys.alive() and time.gps() < next_fetch_time do
            sys.wait(5)
        end
    else
        sys.log(core.CRITICAL, "Failed to retrieve credentials, retrying in 60 seconds")
        sys.wait(60)
    end
end
