--
-- Maintains up-to-date credentials for IAM role access from an EC2 instance
--

local json = require("json")

local aws_meta_url = "http://169.254.169.254/latest/meta-data/iam/security-credentials"
local asset = arg[1] or "iam-role"

-- get current EC2 role
local role, status = netsvc.get(aws_meta_url)
if status then

    -- maintain aws credentials
    while sys.alive() do
        sys.log(core.DEBUG, "Fetching IAM role credentials...")

        -- get new credentials
        local response, status = netsvc.get(aws_meta_url.."/"..role)

        -- convert reponse to credential table
        if status then
            local rc, credential = pcall(json.decode, response)

            -- store credentials and wait for next time to update
            if rc and credential and credential.Expiration then
                sys.log(core.INFO, string.format("New IAM role %s credentials fetched, expiration: %s", role, credential.Expiration))

                -- store credentials for use by server
                rc = aws.csput(asset, credential)

                if rc then
                    -- calculate next fetch time
                    local now = time.gps()
                    local credential_gps = time.gmt2gps(credential.Expiration)
                    local credential_duration = credential_gps - now
                    local next_fetch_time = now + (credential_duration / 2)

                    -- wait until next fetch time
                    while sys.alive() and time.gps() < next_fetch_time do
                        sys.wait(5)
                    end
                else
                    sys.log(core.CRITICAL, string.format("Failed to process IAM role %s credentials, retrying in 60 seconds", role))
                    sys.wait(60)
                end
            else
                sys.log(core.CRITICAL, string.format("Failed to decode IAM role %s response, retrying in 60 seconds", role))
                sys.wait(60)
            end
        else
            sys.log(core.CRITICAL, string.format("Failed to retrieve IAM role %s credentials, retrying in 60 seconds", role))
            sys.wait(60)
        end
    end
end
