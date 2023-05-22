const sliderule = require('../sliderule.js');

function checkVersion (result) {
    expect(result.server.duration).toBeGreaterThan(0);
    expect(result).toHaveProperty('server');
    expect(result.server).toHaveProperty('environment', 'version', 'commit', 'packages', 'launch');
    expect(result.server.packages).toContain('core');
    expect(result.server.commit.includes('v')).toBeTruthy();
}

test('node version api', () => {
    return sliderule.get_version().then(
        result => checkVersion(result),
        error => {throw new Error(error);}
    );
});

test('source version api', () => {
    return sliderule.source('version').then(
        result => checkVersion(result),
        error => {throw new Error(error);}
    );
});

