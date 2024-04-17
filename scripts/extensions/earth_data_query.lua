--
-- Provides functions to query CMR
--
--  NOTES:  The code below uses libcurl (via the netsvc package) to
--          issue a requests

local json = require("json")
local prettyprint = require("prettyprint")

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
    ["atlas-local"] = "ATL03",
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
RC_RQST_FAILED = -1
RC_RSPS_UNPARSEABLE = -2
RC_RSPS_UNEXPECTED = -3

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
local function cmr (parms)

    local urls = {}

    -- get parameters of request
    local short_name = parms["short_name"] or ASSETS_TO_DATASETS[parms["asset"]]
    local dataset = DATASETS[short_name] or {}
    local provider = dataset["provider"] or error("unable to determine provider for query")
    local version = parms["version"] or dataset["version"]
    local polygon = parms["polygon_for_cmr"] or parms["polygon"]
    local t0 = parms["t0"] or '2018-01-01T00:00:00Z'
    local t1 = parms["t1"] or string.format('%04d-%02d-%02dT%02d:%02d:%02dZ', time.gps2date(time.gps()))
    local name_filter = parms["name_filter"]

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
    while true do

        -- issue http request
        local rsps, hdrs, status = netsvc.get(cmr_query_url, nil, headers, false, false, true)
        if not status then
            return RC_RQST_FAILED, "http request to CMR failed"
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
            local links = e["links"]
            if links then
                for _,l in ipairs(links) do
                    local link = l["href"]
                    if link then
                        -- grab only links with the desired format (checks extension match)
                        for _,format in ipairs(dataset["formats"]) do
                            if link:sub(-#format) == format then
                                -- split link over '/' and grab last element
                                local url = link
                                for token in link:gmatch("[^" .. "/"  .. "]+") do
                                    url = token
                                end
                                -- add url to list of urls to return
                                table.insert(urls, url)
                                num_links = num_links + 1
                                break;
                            end
                        end
                    end
                end
            end
        end

        -- check if any links added with last request (else all done)
        if num_links == 0 then
            break
        end
    end

    -- return urls
    return RC_SUCCESS, urls
end

--
-- STAC
--
local function stac (parms)

    local geotable = {}

    -- get parameters of request
    local short_name = parms["short_name"] or ASSETS_TO_DATASETS[parms["asset"]]
    local dataset = DATASETS[short_name] or {}
    local provider = dataset["provider"] or error("unable to determine provider for query")
    local collections = parms["collections"] or dataset["collections"]
    local polygon = parms["polygon"]
    local t0 = parms["t0"] or '2018-01-01T00:00:00Z'
    local t1 = parms["t1"] or string.format('%04d-%02d-%02dT%02d:%02d:%02dZ', time.gps2date(time.gps()))

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
        for k,v in pairs(polygon) do
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
    if num_matched > DEFAULT_MAX_REQUESTED_RESOURCES then
        sys.log(core.WARNING, string.format("Number of matched resources truncated from %d to %d",num_matched, DEFAULT_MAX_REQUESTED_RESOURCES))
        num_matched = DEFAULT_MAX_REQUESTED_RESOURCES
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
    geotable["context"]["limit"] = DEFAULT_MAX_REQUESTED_RESOURCES

    -- return response
    return RC_SUCCESS, geotable 
end

--
-- TNM
--
local function tnm (parms)

    local geotable = {
        ["type"] = "FeatureCollection",
        ["features"] = {}
    }

    -- get parameters of request
    local short_name = parms["short_name"] or ASSETS_TO_DATASETS[parms["asset"]]
    local polygon = parms["polygon"]
    local t0 = parms["t0"] or '2018-01-01'
    local t1 = parms["t1"] or string.format('%04d-%02d-%02d', time.gps2date(time.gps()))

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

        -- issue http request
        local rsps, status = netsvc.get(tnm_query_url, nil, {})
        if not status then
            return RC_RQST_FAILED, "http request to TNM failed"
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
                    ["datetime"] = string.format("%04d-%02d-%02dT%02d:%02d:%02d.%03d+00:00", time.gps2date(time.gmt2gps(item["lastUpdated"]))),
                    ["url"] = item["urls"]["TIFF"]
                }
            }
            table.insert(geotable["features"], feature)
        end

        -- check if complete
        num_items = num_items + #results["items"]
        if num_items == results["total"] then
            break
        end
    end

    -- return geotable
    return RC_SUCCESS, geotable
end

--
-- Return Package --
--
return {
    cmr = cmr,
    stac = stac,
    tnm = tnm,
    SUCCESS = RC_SUCCESS,
    RQST_FAILED = RC_RQST_FAILED,
    RSPS_UNPARSEABLE = RC_RSPS_UNPARSEABLE,
    RSPS_UNEXPECTED = RC_RSPS_UNEXPECTED,
}
