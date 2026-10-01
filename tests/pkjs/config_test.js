var assert = require('assert');
var config = require('../../src/pkjs/config.js');

function cfg() { var c = {}, k; for (k in config.DEFAULTS) c[k] = config.DEFAULTS[k]; return c; }

var html = config.buildHtml(cfg(), {});
assert(html.indexOf('class="kofi"') > 0, 'header Ko-fi pill');
assert(html.indexOf('href="https://ko-fi.com/shadowpeak100"') > 0, 'Ko-fi link');
assert(html.split('https://ko-fi.com/shadowpeak100').length - 1 === 2, 'Ko-fi link top and bottom');
assert(html.indexOf('Buy me a coffee on Ko-fi') > 0, 'Support card button');
assert(html.indexOf('Ski Time is free, with no ads.') > 0, 'Support card text');
assert(html.indexOf('<h2>Support</h2>') < html.indexOf('id="save"'), 'Support card above the Save bar');

var e = config.envFrom({}, undefined);
assert.deepStrictEqual(e, { platform: 'emery', wmo: 0, wind: 0, rise: 420, set: 1140 });
e = config.envFrom({ wx: 61, wind: -1, rise: 400, set: 1200 }, 'basalt');
assert.deepStrictEqual(e, { platform: 'basalt', wmo: 61, wind: -1, rise: 400, set: 1200 });
e = config.envFrom({ wx: 150, wind: 7, rise: 'x', set: 2000 }, 'chalk');
assert.deepStrictEqual(e, { platform: 'emery', wmo: 0, wind: 0, rise: 420, set: 1140 });
e = config.envFrom({ wx: 'abc', wind: null }, 'emery');
assert.strictEqual(e.wmo, 0); assert.strictEqual(e.wind, 0);

console.log('config tests passed');
