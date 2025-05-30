# Arbitrary Code Execution

2025-05-30

## Summary

SlideRule now supports executing user provided Lua code on private clusters.  This allows researchers to take advantage of the parallelization and cluster computing capabilities of SlideRule for code they develop.

Using the SlideRule Python Client, the following is an example of how to execute user provided code that returns "Hello World" back to the user:
```Python
sliderule.source("ace", 'return "Hello World"')
```

## Example Use Case - ATL13 Lake ID Mapping

The ATL13 inland lake data product contains along-track water surface characteristics for inland bodies of water.  Each measurement (i.e. variable) in the product is tagged with a reference ID which can be used as an index into an internal ATL13 global database of inland water bodies.  This database contains a geometry for each body of water and is used in the ATL13 processing to produce the ATL13 data product only over those bodies of water.

Researchers requested the ability to retrieve the exact set of ATL13 data generated for a given body of water when supplying one of three pieces of information: (1) the ATL13 reference ID, (2) the name of the body of water, (3) a coordinate contained within a body of water.

The ATL13 global database contains the reference ID, name, and geometry of each body of water, but does not contain a list of ATL13 granules that intersect (and therefore have data for) thoes bodies of water.  We needed some way to know which granules contained data for each body of water; and we came up with two possibilities:
1. Given a user query, use the global database to pull out the geometry.  Use the geometry to query CMR for a list of granules that intersect.
2. Build a reverse lookup table of reference IDs and granules but reading every ATL13 granule and pulling out which reference IDs are contained there in.  Given a user query, the ATL13 global database can be used to get a reference ID, and the reverse lookup table can be used to get all of the granules with data for that reference ID.

The first option was the simplest but suffered from relying on CMR which is relatively slow and the possibility of having granules returned for other nearby bodies of water due to buffering on the along-tracak polygons CMR uses for their spatial queries.  The second option would result in the best performance, but required every ATL13 granule to be read in order to build the reverse lookup table.

The second option was chosen, and the Arbitrary Code Execution functionality in SlideRule was used to build the lookup table.  

:::{note}
SlideRule still supports temporal/spatial queries of CMR for ATL13; it is only when a user wants to use the reference ID, name, or containing coordinate that the lookup table option is used.
:::

#### User Lua Script

```lua
-- 1. import modules
local json = require("json")

-- 2. create an h5coro object from the granule to be processed
local asset = core.getbyname("icesat2-atl13")
local h5obj = h5.file(asset, "ATL13_20250302152414_11692601_006_01.h5")

-- 3. read the reference id out of each of the 6 beams
local column_gt1l = h5obj:readp("gt1l/atl13refid")
local column_gt1r = h5obj:readp("gt1r/atl13refid")
local column_gt2l = h5obj:readp("gt2l/atl13refid")
local column_gt2r = h5obj:readp("gt2r/atl13refid")
local column_gt3l = h5obj:readp("gt3l/atl13refid")
local column_gt3r = h5obj:readp("gt3r/atl13refid")

-- 4. helper function that puts reference ids into a table
local function count(results, column)
    local values = column:unique()
    for k,_ in pairs(values) do
        results[tostring(k)] = true
    end
end

-- 5. combine the reference ids into a single table
local results = {}
count(results, column_gt1l)
count(results, column_gt1r)
count(results, column_gt2l)
count(results, column_gt2r)
count(results, column_gt3l)
count(results, column_gt3r)

-- 6. return the results as json
return json.encode(results)
```

The above user provided script was sent to the SlideRule `ace` api to execute on a private cluster (in this case, the cluster was the **developers** cluster).  For the purposes of this article, I've annotated the script with comments for assist in the explanation below. Here are the steps it performed:
1. User provided scripts can import external modules available to the SlideRule runtime; in this case the `json` module was needed and imported
2. The granule name is modified in the script prior to being sent for each granule that needs to be read.  The script opens that granule so that it can be read from later in the script.
3. Each of the reads to the datasets in the granule are asynchronous, meaning the code immediate returns and the read operation continues on in the background.
4. Helper functions can be defined just like any other Lua script.
5. The helper function is called and the reference IDs read from each of the beams are put into a single table.  This does two things: first it creates a unique list of reference IDs, and second, as soon as the helper function calls the `column:unique()` function, the code blocks until the dataset being read completes.  See the note below for more details.
6. The results are returned as a json serialized object.  Lua scripts don't have to return json, but it is a best practice.

:::{note}
The `h5obj:readp` function returns what is called an `H5Column` in SlideRule.  This can be somewhat be thought of like a numpy array, though the functionality is currently very limited.  An `H5Column` is an array of data read out of an H5 file that is internally represented as a _vector_ of _doubles_.  The following operations on `H%Columns` are currently supported: `sum`, `mean`, `median`, `mode`, `unique`.  In the future, more operations will be added.
:::

#### User Python Script

If the user provided script needs to only be run against a single granule, the no additional steps are necessary - the script can be set to the `ace` API as is and the results processed.  But if a user wants to execute the script against multiple granules and take advantage of the cluster computing capabilities of SlideRule, then the user must also write a Python program that manages the orchestration of those requests to SlideRule.

For the ATL13 use case, the Python program used to manage the execution of the above script against all ATL13 granules can be found here: [clients/python/utils/atl13_utils.py](https://github.com/SlideRuleEarth/sliderule/blob/main/clients/python/utils/atl13_utils.py).

This script queries CMR for a complete list of ATL13 granules and then creates a thread pool of workers that go through that list and issue `ace` API calls for each granule. The default concurrency is set to 8 in the script, but could easily be set to 100 for a private cluster of 10 nodes. 

As can be seen in the script, the results of each API call were added to a master lookup table (a _dictionary_ of _sets_ in Python) to produce the final lookup table that uses a reference ID to return a list of granules containing data that ID.

## Constraints

There are some constraints on how the Arbitrary Code Execution works:

* The `ace` API only supports non-streaming GET requests. This means that the user provided lua scripts must contain a __return__ statement, and only what is returned is passed back to the user.  In contrast, most of the high-level APIs provided by SlideRule are streaming and stream results back to the user via the __rspq__ response queue.

* The `ace` API requests are not proxied.  This means that the orchestrator does not partition out the request and utilize the locking mechanism to guarantee an even and high utilization of the full cluster.  The requests are still load balanaced, so to some degree they will be spread out over the cluster; but it is on the user to appropriately feed large sets of request to the cluster in order to maintain a high utilization without exceeding the resources of any one node.