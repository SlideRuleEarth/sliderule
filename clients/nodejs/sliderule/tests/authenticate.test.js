const sliderule = require('../../sliderule');

test('authenticate to provisioning system', () => {
    return sliderule.core.authenticate().then(
        result => {
            expect(result).toBeGreaterThan(0);
        },
        error => {throw new Error(error);}
    );
});