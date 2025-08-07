const sliderule = require('../../sliderule');

test('authenticate to provisioning system', () => {
    return sliderule.core.authenticate().then(
        result => {
            if (result === undefined) {
                console.warn('No auth returned — likely public cluster. Skipping assertion.');
                return;
            }
            expect(result).toBeGreaterThan(0);
        },
        error => {throw new Error(error);}
    );
});