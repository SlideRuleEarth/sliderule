--
-- Provides functions to query CMR, STAC, TNM
--
--  NOTES:  The code below uses libcurl (via the netsvc package) to
--          issue http requests

local json = require("json")

--
-- Constants
--

-- default maximum number of resources to process in one request
DEFAULT_MAX_REQUESTED_RESOURCES = 300

-- best effort match of datasets to providers and versions for earthdata
DATASETS = {
    ATL03 =                                               {provider = "NSIDC_ECS",   version = "006",  api = "cmr",   formats = {".h5"},    collections = {}},
    ATL06 =                                               {provider = "NSIDC_ECS",   version = "006",  api = "cmr",   formats = {".h5"},    collections = {}},
    ATL08 =                                               {provider = "NSIDC_ECS",   version = "006",  api = "cmr",   formats = {".h5"},    collections = {}},
    ATL09 =                                               {provider = "NSIDC_ECS",   version = "006",  api = "cmr",   formats = {".h5"},    collections = {}},
    ATL13 =                                               {provider = "NSIDC_ECS",   version = "006",  api = "cmr",   formats = {".h5"},    collections = {}},
    GEDI01_B =                                            {provider = "LPDAAC_ECS",  version = "002",  api = "cmr",   formats = {".h5"},    collections = {}},
    GEDI02_A =                                            {provider = "LPDAAC_ECS",  version = "002",  api = "cmr",   formats = {".h5"},    collections = {}},
    GEDI02_B =                                            {provider = "LPDAAC_ECS",  version = "002",  api = "cmr",   formats = {".tiff"},  collections = {}},
    GEDI_L3_LandSurface_Metrics_V2_1952 =                 {provider = "ORNL_CLOUD",  version = nil,    api = "cmr",   formats = {".h5"},    collections = {}},
    GEDI_L4A_AGB_Density_V2_1_2056 =                      {provider = "ORNL_CLOUD",  version = nil,    api = "cmr",   formats = {".h5"},    collections = {}},
    GEDI_L4B_Gridded_Biomass_2017 =                       {provider = "ORNL_CLOUD",  version = nil,    api = "cmr",   formats = {".tiff"},  collections = {}},
    HLS =                                                 {provider = "LPCLOUD",     version = nil,    api = "stac",  formats = {".tiff"},  collections = {"HLSS30.v2.0", "HLSL30.v2.0"}},
    ["Digital Elevation Model (DEM) 1 meter"] =           {provider = "USGS",        version = nil,    api = "tnm",   formats = {".tiff"},  collections = {}},
    SWOT_SIMULATED_L2_KARIN_SSH_ECCO_LLC4320_CALVAL_V1 =  {provider = "POCLOUD",     version = nil,    api = "cmr",   formats = {".nc"},    collections = {"C2147947806-POCLOUD"}},
    SWOT_SIMULATED_L2_KARIN_SSH_GLORYS_CALVAL_V1 =        {provider = "POCLOUD",     version = nil,    api = "cmr",   formats = {".nc"},    collections = {"C2152046451-POCLOUD"}}
}

-- best effort match of sliderule assets to earthdata datasets
ASSETS_TO_DATASETS = {
    ["gedil4a"] = "GEDI_L4A_AGB_Density_V2_1_2056",
    ["gedil4b"] = "GEDI_L4B_Gridded_Biomass_2017",
    ["gedil3-elevation"] = "GEDI_L3_LandSurface_Metrics_V2_1952",
    ["gedil3-canopy"] = "GEDI_L3_LandSurface_Metrics_V2_1952",
    ["gedil3-elevation-stddev"] = "GEDI_L3_LandSurface_Metrics_V2_1952",
    ["gedil3-canopy-stddev"] = "GEDI_L3_LandSurface_Metrics_V2_1952",
    ["gedil3-counts"] = "GEDI_L3_LandSurface_Metrics_V2_1952",
    ["gedil2a"] = "GEDI02_A",
    ["gedil1b"] = "GEDI02_B",
    ["swot-sim-ecco-llc4320"] = "SWOT_SIMULATED_L2_KARIN_SSH_ECCO_LLC4320_CALVAL_V1",
    ["swot-sim-glorys"] = "SWOT_SIMULATED_L2_KARIN_SSH_GLORYS_CALVAL_V1",
    ["usgs3dep-1meter-dem"] = "Digital Elevation Model (DEM) 1 meter",
    ["landsat-hls"] = "HLS",
    ["icesat2"] = "ATL03",
    ["icesat2-atl06"] = "ATL06",
    ["icesat2-atl08"] = "ATL08",
    ["icesat2-atl09"] = "ATL09",
    ["icesat2-atl13"] = "ATL13",
    ["atlas-local"] = "ATL03",
    ["atlas-s3"] = "ATL03",
    ["nsidc-s3"] = "ATL03"
}

-- upper limit on resources returned from CMR query per request
CMR_PAGE_SIZE = 2000

-- upper limit on resources returned from STAC query per request
STAC_PAGE_SIZE = 100

-- upper limit on resources returned from TNM query per request
TNM_PAGE_SIZE = 100

-- response codes for all of the package functions
RC_SUCCESS = 0
RC_FAILURE = -1
RC_RQST_FAILED = -2
RC_RSPS_UNPARSEABLE = -3
RC_RSPS_UNEXPECTED = -4

--
-- Build GeoJSON
--
local function build_geojson(rsps)
    local status, geotable = pcall(json.decode, rsps)
    if status then
        if geotable["links"]                    then geotable["links"] = nil end
        if geotable["numberMatched"]            then geotable["numberMatched"] = nil end
        if geotable["numberReturned"]           then geotable["numberReturned"] = nil end
        if geotable["features"] then
            for i = 1, #geotable["features"] do
                local feature = geotable["features"][i]
                if feature["links"]             then feature["links"] = nil end
                if feature["stac_version"]      then feature["stac_version"] = nil end
                if feature["stac_extensions"]   then feature["stac_extensions"] = nil end
                if feature["collection"]        then feature["collection"] = nil end
                if feature["bbox"]              then feature["bbox"] = nil end
                local feature_assets = feature["assets"]
                if feature_assets["browse"]     then feature_assets["browse"] = nil end
                if feature_assets["metadata"]   then feature_assets["metadata"] = nil end
                local feature_properties = feature["properties"]
                for v,k in pairs(feature_assets) do
                    feature_properties[v] = feature_assets[v]["href"]
                end
                feature["assets"] = nil
            end
        end
    end
    return status, geotable
end

--
-- urlify
--
local function urlify(str)
    str = string.gsub(str, "\n", "\r\n")
    str = string.gsub(str, "([^%w %-%_%.%~])", function(c) return string.format("%%%02X", string.byte(c)) end)
    str = string.gsub(str, " ", "+")
    return str
end

---------------------------------------------------------------
-- PUBLIC PACKAGE
---------------------------------------------------------------

--
-- CMR
--
local function cmr (parms, poly, with_meta)

    local link_table = {}

    -- get parameters of request
    local short_name = parms["short_name"] or ASSETS_TO_DATASETS[parms["asset"]]
    local dataset = DATASETS[short_name] or {}
    local cmr_parms = parms["cmr"] or {}
    local provider = dataset["provider"] or error("unable to determine provider for query")
    local version = parms["version"] or dataset["version"]
    local polygon = cmr_parms["polygon"] or parms["poly"] or poly
    local t0 = parms["t0"] or '2018-01-01T00:00:00Z'
    local t1 = parms["t1"] or string.format('%04d-%02d-%02dT%02d:%02d:%02dZ', time.gps2date(time.gps()))
    local name_filter = parms["name_filter"]
    local max_resources = parms["max_resources"] or DEFAULT_MAX_REQUESTED_RESOURCES

    -- build name filter
    if (not name_filter) and parms["asset"] and (parms["asset"] == "icesat2") then
        local name_filter_enabled = false
        local rgt_filter = '????'
        if parms["rgt"] then
            rgt_filter = string.format("%04d", parms["rgt"])
            name_filter_enabled = true
        end
        local cycle_filter = '??'
        if parms["cycle"] then
            cycle_filter = string.format("%02d", parms["cycle"])
            name_filter_enabled = true
        end
        local region_filter = '??'
        if parms["region"] then
            region_filter = string.format("%02d", parms["region"])
            name_filter_enabled = true
        end
        if name_filter_enabled then
            name_filter = '*_' .. rgt_filter .. cycle_filter .. region_filter .. '_*'
        end
    end

    -- flatten polygon
    local polystr = ""
    if polygon then
        for i,coord in ipairs(polygon) do
            if i < #polygon then
                polystr = polystr .. string.format("%f,%f,", coord["lon"], coord["lat"]) -- with trailing comma
            else
                polystr = polystr .. string.format("%f,%f", coord["lon"], coord["lat"]) -- without trailing comma
            end
        end
    end

    -- build query request
    local cmr_query_url = table.concat({
        "https://cmr.earthdata.nasa.gov/search/granules.json",
        string.format("?provider=%s", provider),
        "&sort_key[]=start_date&sort_key[]=producer_granule_id&scroll=true",
        string.format("&page_size=%s", CMR_PAGE_SIZE),
        string.format("&short_name=%s", short_name),
        version and string.format("&version=%s", version) or "",
        (t0 and t1) and string.format("&temporal[]=%s,%s", t0, t1) or "",
        polygon and string.format("&polygon=%s", polystr) or "",
        name_filter and string.format("&options[producer_granule_id][pattern]=true&producer_granule_id[]=%s", name_filter) or "",
    })

    -- make requests and page through responses
    local headers = {}
    local total_links = 0
    while true do

        -- issue http request
        local rsps, hdrs, status = netsvc.get(cmr_query_url, nil, headers, false, false, true)
        if not status then
            return RC_RQST_FAILED, string.format("http request to CMR failed - %s", rsps)
        end

        -- add scroll id for future page requests
        if not headers["cmr-scroll-id"] then
            headers["cmr-scroll-id"] = hdrs["cmr-scroll-id"]
        end

        -- decode response text (json) into lua table
        local rc, results = pcall(json.decode, rsps)
        if not rc then
            return RC_RSPS_UNPARSEABLE, "could not parse json in response from CMR"
        end

        -- confirm necessary fields are present in the response
        if not results["feed"] or not results["feed"]["entry"] then
            return RC_RSPS_UNEXPECTED, "could not find expected fields in response from CMR"
        end

        -- loop through each response entry and pull out the desired links
        local num_links = 0
        for _,e in ipairs(results["feed"]["entry"]) do
            -- build metadata with polygon and time stamps
            local poly_table = {}
            if e["polygons"] then
                local polygon_list = {}
                local pattern = string.format("([^%s]+)", " ") -- builds a delimeter (' ')
                e["polygons"][1][1]:gsub(pattern, function(substring) -- splits string over delimeter
                    table.insert(polygon_list, substring)
                end)
                for i = 1,#polygon_list,2 do
                    table.insert(poly_table, {lat=polygon_list[i], lon=polygon_list[i+1]})
                end
            end
            local metadata = { poly = poly_table, t0 = e["time_start"], t1 = e["time_end"] }
            -- loop through links
            local links = e["links"]
            if links then
                for _,l in ipairs(links) do
                    local link = l["href"]
                    if link and (total_links < max_resources) then
                        -- grab only links with the desired format (checks extension match)
                        for _,format in ipairs(dataset["formats"]) do
                            if link:sub(-#format) == format then
                                -- split link over '/' and grab last element
                                local url = link
                                for token in link:gmatch("[^" .. "/"  .. "]+") do
                                    url = token
                                end
                                -- add url to table of links with metadata
                                if not link_table[url] then
                                    link_table[url] = metadata 
                                    num_links = num_links + 1
                                    total_links = total_links + 1
                                end
                                break
                            end
                        end
                    end
                end
            end
        end

        -- check if any links added with last request (else all done)
        if num_links == 0 then
            break
        elseif total_links >= max_resources then
            sys.log(core.WARNING, string.format("Number of matched resources truncated to %d for %s", total_links, short_name))
            break
        end
    end

    -- return results
    if with_meta then
        return RC_SUCCESS, link_table
    else
        local urls = {}
        for link,_ in pairs(link_table) do
            table.insert(urls, link)
        end
        return RC_SUCCESS, urls
    end

end

--
-- STAC
--
local function stac (parms, poly)

    local geotable = {}

    -- get parameters of request
    local short_name = parms["short_name"] or ASSETS_TO_DATASETS[parms["asset"]]
    local dataset = DATASETS[short_name] or {}
    local provider = dataset["provider"] or error("unable to determine provider for query")
    local collections = parms["collections"] or dataset["collections"]
    local polygon = parms["poly"] or poly
    local t0 = parms["t0"] or '2018-01-01T00:00:00Z'
    local t1 = parms["t1"] or string.format('%04d-%02d-%02dT%02d:%02d:%02dZ', time.gps2date(time.gps()))
    local max_resources = parms["max_resources"] or DEFAULT_MAX_REQUESTED_RESOURCES

    -- build stac request
    local url = string.format("https://cmr.earthdata.nasa.gov/stac/%s/search", provider)
    local headers = {["Content-Type"] = "application/json"}
    local rqst = {
        ["limit"] = STAC_PAGE_SIZE,
        ["datetime"] = string.format("%s/%s", t0, t1),
        ["collections"] = collections,
    }

    -- add polygon if provided
    if polygon then
        local coordinates = {}
        for _,v in pairs(polygon) do
            table.insert(coordinates, {v["lon"], v["lat"]})
        end
        rqst["intersects"] = {
            ["type"] = "Polygon",
            ["coordinates"] = {coordinates}
        }
    end

    -- post initial request
    local rsps, status = netsvc.post(url, json.encode(rqst), headers)
    if not status then
        return RC_RQST_FAILED, "http request to stac server failed"
    end

    -- parse response into a table
    status, geotable = build_geojson(rsps)
    if not status then
        return RC_RSPS_UNPARSEABLE, "could not parse json in response from stac"
    end

    -- iterate through additional pages if not all returned
    local num_returned = geotable["context"]["returned"]
    local num_matched = geotable["context"]["matched"]
    if num_matched > max_resources then
        sys.log(core.WARNING, string.format("Number of matched resources truncated from %d to %d",num_matched, max_resources))
        num_matched = max_resources
    end
    local num_pages = math.floor((num_matched  + (num_returned - 1)) / num_returned)
    for page = 2, num_pages do
        rqst["page"] = page

        -- post paged request
        local rsps, status = netsvc.post(url, json.encode(rqst), headers)
        if not status then
            return RC_RQST_FAILED, "http request to stac server failed"
        end

        -- parse paged response into table
        local status, paged_geotable = build_geojson(rsps)
        if not status then
            return RC_RSPS_UNPARSEABLE, "could not parse json in response from stac server"
        end
        table.move(paged_geotable["features"], 1, #paged_geotable["features"], #geotable["features"] + 1, geotable["features"])
    end
    geotable["context"]["returned"] = num_matched
    geotable["context"]["limit"] = max_resources

    -- return response
    return RC_SUCCESS, geotable

end

--
-- TNM
--
local function tnm (parms, poly)

    local geotable = {
        ["type"] = "FeatureCollection",
        ["features"] = {}
    }

    -- get parameters of request
    local short_name = parms["short_name"] or ASSETS_TO_DATASETS[parms["asset"]]
    local polygon = parms["poly"] or poly
    local t0 = parms["t0"] or '2018-01-01'
    local t1 = parms["t1"] or string.format('%04d-%02d-%02d', time.gps2date(time.gps()))
    local max_resources = parms["max_resources"] or DEFAULT_MAX_REQUESTED_RESOURCES

    -- flatten polygon
    local polystr = ""
    if polygon then
        for i,coord in ipairs(polygon) do
            if i < #polygon then
                polystr = polystr .. string.format("%f %f,", coord["lon"], coord["lat"]) -- with trailing comma
            else
                polystr = polystr .. string.format("%f %f", coord["lon"], coord["lat"]) -- without trailing comma
            end
        end
    end

    -- get GPS times of t0 and t1
    local t0_gps = time.gmt2gps(string.gsub(t0, '-', ':')..':00:00:00')
    local t1_gps = time.gmt2gps(string.gsub(t1, '-', ':')..':00:00:00')

    -- scroll through requests to get items
    local num_items = 0
    while true do

        -- build query request
        local tnm_query_url = table.concat({
            "https://tnmaccess.nationalmap.gov/api/v1/products",
            string.format("?datasets=%s", urlify(short_name)),
            string.format("&polygon=%s", urlify(polystr)),
            string.format("&prodFormats=%s", "GeoTIFF"),
            string.format("&outputFormat=%s", "JSON"),
            string.format("&max=%d", TNM_PAGE_SIZE),
            string.format("&offset=%d", num_items),
            string.format("&start=%s", t0),
            string.format("&stop=%s", t1),
        })

        -- make https request (with retries)
        local rsps = nil
        local rsps_status = false
        local attempts = 3
        local request_complete = false
        while not request_complete do
            rsps, rsps_status = netsvc.get(tnm_query_url, nil, {})
            if rsps_status then
                break
            end
            attempts = attempts - 1
            if attempts <= 0 then
                return RC_RQST_FAILED, rsps
            else
                sys.log(core.WARNING, string.format("TNM returned error <%d>, retrying... \n>>>\n%s\n<<<", rsps_status, rsps))
            end
        end

        -- build table from response
        local status, results = pcall(json.decode, rsps)
        if not status then
            return RC_RSPS_UNPARSEABLE, "could not parse json in response from TNM"
        end
        if not results["items"] then
            return RC_RSPS_UNEXPECTED, "no items returned in request"
        end

        -- then build the geojson from the response
        for _,item in ipairs(results["items"]) do
            -- filter on time
            local feature_gps = time.gmt2gps(item["lastUpdated"])
            if feature_gps < t0_gps or feature_gps > t1_gps then
                goto continue
            end
            -- build feature
            local minX = item["boundingBox"]["minX"]
            local maxX = item["boundingBox"]["maxX"]
            local minY = item["boundingBox"]["minY"]
            local maxY = item["boundingBox"]["maxY"]
            local feature = {
                ["type"] = "Feature",
                ["id"] = item["sourceId"],
                ["geometry"] = {
                    ["type"] = "Polygon",
                    ["coordinates"] = {{{minX, minY}, {maxX, minY}, {maxX, maxY}, {minX, maxY}, {minX, minY}}}
                },
                ["properties"] = {
                    ["datetime"] = string.format("%04d-%02d-%02dT%02d:%02d:%02d.%03d+00:00", time.gps2date(feature_gps)),
                    ["url"] = item["urls"]["TIFF"]
                }
            }
            -- add feature
            table.insert(geotable["features"], feature)
            ::continue::
        end

        -- check if complete
        num_items = num_items + #results["items"]
        if num_items == results["total"] then
            break
        elseif num_items >= max_resources then
            sys.log(core.WARNING, string.format("Number of matched resources truncated from %d to %d", results["total"], num_items))
            break
        end
    end

    -- return geotable
    return RC_SUCCESS, geotable

end

--
-- search
--
local function search (parms, poly)

    local short_name = parms["short_name"] or ASSETS_TO_DATASETS[parms["asset"]]
    local dataset = DATASETS[short_name] or {}
    local api = dataset["api"]
    if api == "cmr" then
        return cmr(parms, poly)
    elseif api == "stac" then
        return stac(parms, poly)
    elseif api == "tnm" then
        return tnm(parms, poly)
    else
        return RC_FAILURE, string.format("unsupport api: %s", api)
    end

end

--
-- Return Package --
--
return {
    cmr = cmr,
    stac = stac,
    tnm = tnm,
    search = search,
    SUCCESS = RC_SUCCESS,
    FAILURE = RC_FAILURE,
    RQST_FAILED = RC_RQST_FAILED,
    RSPS_UNPARSEABLE = RC_RSPS_UNPARSEABLE,
    RSPS_UNEXPECTED = RC_RSPS_UNEXPECTED,
}
