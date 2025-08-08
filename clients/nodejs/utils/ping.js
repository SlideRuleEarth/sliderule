const sliderule = require('../sliderule');

let url = process.argv[2];

async function main() {
    if (url == "localhost") {
        const http = require('http')
        await sliderule.core.init({domain: "localhost", organization: null, protocol: http});
    } else {
        const parts = url.split(".")
        const organization = parts[0]
        const domain = parts.slice(1,3).join(".")
        await sliderule.core.init({domain: domain, organization: organization});
    }
    sliderule.core.get_version().then(
        result => console.log(result),
        error => {throw new Error(error);}
    );
}

main()