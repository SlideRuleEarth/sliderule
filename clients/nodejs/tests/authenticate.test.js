const sliderule = require('./sliderule.js');

test('authenticate to provisioning system', () => {
    sliderule.init({organization: 'developers'});
    return sliderule.authenticate().then(
        result => {
            expect(result).toBeGreaterThan(0);
        },
        error => {throw new Error(error);}
    );
});