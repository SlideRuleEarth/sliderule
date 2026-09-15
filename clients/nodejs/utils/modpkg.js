// modpkg.js
const { execSync } = require('child_process');
const fs = require('fs');

function getVersionFromGit() {
    // Exact tag match -> "v5.6.0". Otherwise -> "v5.6.0-3-gabcdef" (3 commits past tag)
    const raw = execSync('git describe --tags --always --dirty').toString().trim();
    const m = raw.match(/^v(\d+\.\d+\.\d+)(?:-(\d+)-g([0-9a-f]+))?(-dirty)?$/);
    if (!m) {
        throw new Error(`Unable to parse git describe output: ${raw}`);
    }
    const [, base, distance, sha, dirty] = m;
    if (!distance) return base;                       // exact release tag
    return `${base}-dev.${distance}+${sha}${dirty ? '.dirty' : ''}`; // semver-legal prerelease
}

const version = getVersionFromGit();
console.log("Setting version:", version);

const pkgfile = __dirname + '/sliderule/package.json';
const pkgjson = JSON.parse(fs.readFileSync(pkgfile, 'utf8'));
pkgjson.version = version;
fs.writeFileSync(pkgfile, JSON.stringify(pkgjson, null, 2) + '\n');