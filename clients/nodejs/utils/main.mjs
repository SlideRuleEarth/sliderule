import {core,h5coro,icesat2} from '../sliderule/index.js';

import http from 'http'
core.init({domain:"localhost", organization: null, protocol: http});

icesat2.atl06p(
    { "cnf": "atl03_high",
      "ats": 20.0,
      "cnt": 10,
      "len": 40.0,
      "res": 20.0,
      "maxi": 1 }, 
      ["ATL03_20181019065445_03150111_005_01.h5"]
).then(
    result => console.log('Results = ', result.length, result[0]),
    error => console.error('Error = ', error)
);

