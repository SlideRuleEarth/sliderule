const sliderule = require('../../sliderule');
if (process.env.ORGANIZATION == "null") {
    const http = require('http')
    sliderule.core.init({domain: "localhost", organization: null, protocol: http});
} else {
    sliderule.core.init({domain: process.env.DOMAIN, organization: process.env.ORGANIZATION});
}