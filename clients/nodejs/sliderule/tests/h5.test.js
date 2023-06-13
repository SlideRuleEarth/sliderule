const sliderule = require('../../sliderule');

test('read icesat2 dataset using h5coro', () => {
    return sliderule.core.h5("ancillary_data/atlas_sdp_gps_epoch", "ATL03_20181019065445_03150111_005_01.h5", "icesat2").then(
        result => {
            expect(result[0]).toBe(1198800018);
        },
        error => {
            throw new Error(error);
        }
    );
});

