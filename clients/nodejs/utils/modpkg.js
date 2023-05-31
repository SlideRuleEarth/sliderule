let version = process.argv[2];
if (version == undefined) {
    throw new Error("Must supply version as command line argument");
}
else if ((!version.includes(".")) || (version[0] != 'v')) {
    throw new Error("Version is not in the correct format (vX.Y.Z): ", version);
}
version = version.substring(1);
console.log("Setting version: ", version);
const fs = require('fs')
const pkgfile = __dirname + '/../sliderule/package.json';
const pkgdata = fs.readFileSync(pkgfile, { encoding: 'utf8', flag: 'r' });
let pkgjson = JSON.parse(pkgdata);
pkgjson["version"] = version;
const pkgout = JSON.stringify(pkgjson, null, 2);
fs.writeFileSync(pkgfile, pkgout);
