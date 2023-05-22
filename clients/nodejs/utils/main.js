const sliderule = require('../sliderule');

/*
const http = require('http')
sliderule.core.init({domain:"localhost", organization: null, protocol: http})
*/

/*
sliderule.core.get_version().then(
    result => console.log(result),
    error => console.error(error)
);
*/

sliderule.core.authenticate().then(
    result => console.log(result),
    error => console.error(error)
);