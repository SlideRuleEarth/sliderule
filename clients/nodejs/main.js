const sliderule = require('./sliderule.js');

/*
const http = require('http')
sliderule.init({domain:"localhost", organization: null, protocol: http})
*/

/*
sliderule.get_version().then(
    result => console.log(result),
    error => console.error(error)
);
*/

sliderule.authenticate().then(
    result => console.log(result),
    error => console.error(error)
);