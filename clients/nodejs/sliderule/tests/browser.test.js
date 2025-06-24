const sliderule = require('../../sliderule');
const { chromium } = require('playwright');

async function browserFetchCurrentNodes() {
    const browser = await chromium.launch();
    const page = await browser.newPage();
    await page.goto(`https://client.slideruleearth.io`);
    const result = await page.evaluate(async () => {
        const response = await fetch('https://sliderule.slideruleearth.io/discovery/status', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({ service: 'sliderule' }),
        });
        return await response.json();
    });
    await browser.close();
    return result.nodes;
}

test('fetch number of nodes', () => {
    return browserFetchCurrentNodes().then(
        result => {
            console.log('Your message here:', result);
            expect(result).toBeGreaterThan(0);
        },
        error => {throw new Error(error);}
    );
});
