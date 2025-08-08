const sliderule = require('../../sliderule');

module.exports = async () => {
    if (process.env.ORGANIZATION == "null") {
        const http = require('http');
        await sliderule.core.init({
            domain: "localhost",
            organization: null,
            protocol: http });
    } else {
        await sliderule.core.init({
            domain: process.env.DOMAIN,
            organization: process.env.ORGANIZATION
        });
    }
};