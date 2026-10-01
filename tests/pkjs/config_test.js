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

var makeCanvas = require('./stub_dom.js').makeCanvas;

function runPage(c, env) {
  var page = config.buildHtml(c, env);
  var els = {}, raf = [], ids = page.match(/id="([^"]+)"/g).map(function(s) { return s.slice(4, -1); });
  ids.forEach(function(id) {
    els[id] = id === 'pv' ? makeCanvas() : { id: id, style: {}, value: '', setAttribute: function() {}, addEventListener: function() {} };
  });
  var win = {
    document: { getElementById: function(id) { if (!els[id]) throw new Error('missing #' + id); return els[id]; } },
    location: {},
    requestAnimationFrame: function(f) { raf.push(f); return raf.length; },
    cancelAnimationFrame: function() {},
    performance: { now: function() { return 0; } },
    Date: Date
  };
  var js = page.match(/<script>([\s\S]*)<\/script>/)[1];
  new Function('window', 'document', 'location', 'requestAnimationFrame', 'cancelAnimationFrame', 'performance',
    js + ';window.CFG=CFG;')(win, win.document, win.location, win.requestAnimationFrame, win.cancelAnimationFrame, win.performance);
  return { els: els, raf: raf, win: win, page: page };
}

var envE = config.envFrom({}, 'emery');
var r = runPage(cfg(), envE);
assert(r.page.indexOf('<canvas id="pv"') > 0, 'preview canvas');
assert(r.page.indexOf('class="preview"') > r.page.indexOf('class="hdr"'), 'preview under the header');
assert(r.page.length < 120000, 'page size ' + r.page.length);
assert(r.els.pv.width === 200, 'emery preview size');
var pvCtx = r.els.pv.getContext('2d'), pvImg = pvCtx.lastImage();
assert(pvCtx.count('putImageData') >= 1, 'preview drawn on load');
assert.strictEqual(pvImg.width * pvImg.height, 200 * 228, 'preview image size');
assert(pvImg.data.some(function(v) { return v !== 0; }), 'preview image not blank');
console.log('page length', r.page.length);

// Run twice: still only one RAF chain pending.
r.els.run.onclick(); r.els.run.onclick();
assert.strictEqual(r.raf.length, 1, 'one animation loop after double Run, got ' + r.raf.length);

// Old saved settings (no new fields, legacy dayMode 2) still render.
var old = { comp1: 1, comp2: 2, comp3: 0, use24: true, lead0: true, tracks: 0, saverOn: false, saverStart: 1320,
            saverEnd: 420, dayMode: 2, fixedRise: 420, fixedSet: 1140, animMode: 1, weatherOn: false };
runPage(old, config.envFrom({ wx: 'abc' }, 'basalt'));

// Every daypart x weather on a basalt and an emery page.
['emery', 'basalt'].forEach(function(p) {
  for (var dp = 0; dp < 4; dp++) for (var wx = 0; wx < 8; wx++)
    runPage(cfg({ dayCycle: false, dayFixed: dp, weatherOn: false, wxFixed: wx }), config.envFrom({}, p));
});

console.log('config tests passed');
