const sliderule = require('../sliderule.js');

test('authenticate to provisioning system', () => {
    return sliderule.authenticate().then(
        result => {
            expect(result).toBeGreaterThan(0);
        },
        error => {throw new Error(error);}
    );
});