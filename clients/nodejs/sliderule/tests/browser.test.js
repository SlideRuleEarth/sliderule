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

async function browserFetchVersion() {
    const browser = await chromium.launch();
    const page = await browser.newPage();
    await page.goto(`https://client.slideruleearth.io`);
    const result = await page.evaluate(async () => {
        const response = await fetch('https://sliderule.slideruleearth.io/source/version', {
            method: 'GET',
        });
        return await response.json();
    });
    await browser.close();
    return result;
}

test('fetch number of nodes', () => {
    return browserFetchCurrentNodes().then(
        result => {
            expect(result).toBeGreaterThan(0);
        },
        error => {throw new Error(error);}
    );
});

test('fetch server version', () => {
    return browserFetchVersion().then(
        result => {
            expect(result).toHaveProperty('server');
            expect(result.server.packages).toContain('core');
        },
        error => {throw new Error(error);}
    );
});

