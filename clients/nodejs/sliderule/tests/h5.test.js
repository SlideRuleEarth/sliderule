const sliderule = require('../../sliderule');

test('read icesat2 dataset using h5coro', () => {
    return sliderule.core.h5().then(
        result => result,
        error => {throw new Error(error);}
    );
});

