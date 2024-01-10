const sliderule = require('../sliderule');


const http = require('http');
sliderule.core.init({domain:"localhost", organization: null, protocol: http});

sliderule.icesat2.atl06p(
    { "cnf": "atl03_high",
      "ats": 20.0,
      "cnt": 10,
      "len": 40.0,
      "res": 20.0,
      "maxi": 1 }, 
    resources=["ATL03_20181019065445_03150111_005_01.h5"]
).then(
    result => console.log('Results = ', result.length, result[0]),
    error => console.error('Error = ', error)
);

/*
sliderule.core.get_version().then(
    result => console.log(result),
    error => console.error(error)
);
*/

/*
sliderule.core.authenticate().then(
    result => console.log(result),
    error => console.error(error)
);
*/

/*
sliderule.core.h5("ancillary_data/atlas_sdp_gps_epoch", "ATL03_20181019065445_03150111_005_01.h5", "icesat2").then(
    result => console.log("GPS Epoch: ", result[0]),
    error => console.error(error)
);
*/

/*
let a = sliderule.core.source("definition", {"rectype" : "h5file"}).then(
    result => console.log(result),
    error => console.error(error)
);
*/
