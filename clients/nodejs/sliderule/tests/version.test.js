const sliderule = require('sliderule');

function checkVersion (result) {
    expect(result.server.duration).toBeGreaterThan(0);
    expect(result).toHaveProperty('server');
    expect(result.server).toHaveProperty('environment', 'version', 'commit', 'packages', 'launch');
    expect(result.server.packages).toContain('core');
    expect(result.server.commit.includes('v')).toBeTruthy();
}

test('node version api', () => {
    return sliderule.core.get_version().then(
        result => checkVersion(result),
        error => {throw new Error(error);}
    );
});

test('source version api', () => {
    return sliderule.core.source('version').then(
        result => checkVersion(result),
        error => {throw new Error(error);}
    );
});

