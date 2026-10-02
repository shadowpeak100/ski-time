var assert = require('assert');
var config = require('../../src/pkjs/config.js');

function cfg(over) { var c = {}, k; for (k in config.DEFAULTS) c[k] = config.DEFAULTS[k]; for (k in over || {}) c[k] = over[k]; return c; }

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

function stubEl(id) {
  var kids = [], text = '';
  return { id: id, style: {}, value: '', children: kids,
    get textContent() { return text; },
    set textContent(v) { text = v; if (v === '') kids.length = 0; },
    appendChild: function(c) { kids.push(c); },
    setAttribute: function() {}, addEventListener: function() {} };
}

function runPage(c, env, opt) {
  opt = opt || {};
  var page = config.buildHtml(c, env);
  var els = {}, raf = [], ids = page.match(/id="([^"]+)"/g).map(function(s) { return s.slice(4, -1); });
  ids.forEach(function(id) {
    els[id] = id === 'pv' ? makeCanvas() : stubEl(id);
  });
  var win = {
    document: { getElementById: function(id) { if (!els[id]) throw new Error('missing #' + id); return els[id]; },
      createElement: function(tag) { var e = stubEl(''); e.tagName = tag; return e; } },
    location: {},
    requestAnimationFrame: opt.noRaf ? undefined : function(f) { raf.push(f); return raf.length; },
    cancelAnimationFrame: function() {},
    performance: { now: function() { return 0; } },
    Date: Date
  };
  if (opt.nullCtx) els.pv.getContext = function() { return null; };
  var js = page.match(/<script>([\s\S]*)<\/script>/)[1];
  new Function('window', 'document', 'location', 'requestAnimationFrame', 'cancelAnimationFrame', 'performance', 'XMLHttpRequest',
    js + ';window.CFG=CFG;window.SkiPreview=SkiPreview;')(win, win.document, win.location, win.requestAnimationFrame, win.cancelAnimationFrame, win.performance, opt.XMLHttpRequest);
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

// Animation loop: drained, then Run twice leaves one chain that ends after durationMs.
var lp = runPage(cfg({ animMode: 1, weatherOn: false, wxFixed: 0 }), envE);
var seen = [], realDraw = lp.win.SkiPreview.draw, durationMs = 0, ts = 1000;
lp.win.SkiPreview.draw = function(cv, c, env, opts) {
  var res = realDraw.apply(this, arguments);
  seen.push({ dayCycle: c.dayCycle, dayFixed: c.dayFixed, animMs: opts.animMs });
  durationMs = res.durationMs;
  return res;
};
function step() { ts += 40; lp.raf.shift()(ts); }
while (lp.raf.length) step();
assert.strictEqual(lp.raf.length, 0, 'drained');
lp.els.run.onclick(); lp.els.run.onclick();
assert.strictEqual(lp.raf.length, 1, 'one animation loop after double Run, got ' + lp.raf.length);
seen = [];
var frames = 0;
while (lp.raf.length && frames < 1000) { step(); frames++; }
assert(lp.raf.length === 0, 'chain stops after the animation');
assert(frames > 10, 'chain kept going during the animation, frames ' + frames);
assert(frames * 40 >= durationMs && frames * 40 <= durationMs + 80, 'ran for about durationMs: ' + frames * 40 + ' vs ' + durationMs);
assert.strictEqual(seen[0].animMs, 0, 'first frame is animMs 0');

// Changing a setting mid-animation draws the next frame with the new value and adds no second chain.
lp.els.run.onclick();
step(); step(); step();
assert.strictEqual(lp.raf.length, 1, 'still animating');
lp.els.dayCycle_off.onclick();
lp.els.dayFixed.value = '3'; lp.els.dayFixed.onchange();
assert(lp.raf.length <= 1, 'setting change left at most one pending frame, got ' + lp.raf.length);
seen = [];
step();
assert.strictEqual(seen.length, 1, 'one draw for one frame');
assert.strictEqual(seen[0].dayCycle, false, 'frame drew with the new cycle setting');
assert.strictEqual(seen[0].dayFixed, 3, 'frame drew with the new Show setting');
assert(seen[0].animMs > 0, 'animation continued rather than restarted');

// Old saved settings (no new fields, legacy dayMode 2) still render.
var old = { comp1: 1, comp2: 2, comp3: 0, use24: true, lead0: true, tracks: 0, saverOn: false, saverStart: 1320,
            saverEnd: 420, dayMode: 2, fixedRise: 420, fixedSet: 1140, animMode: 1, weatherOn: false };
runPage(old, config.envFrom({ wx: 'abc' }, 'basalt'));

// Every daypart x weather on a basalt and an emery page.
['emery', 'basalt'].forEach(function(p) {
  for (var dp = 0; dp < 4; dp++) for (var wx = 0; wx < 8; wx++)
    runPage(cfg({ dayCycle: false, dayFixed: dp, weatherOn: false, wxFixed: wx }), config.envFrom({}, p));
});

// A broken preview path must not stop Save and Cancel working.
(function() {
  var realSetTimeout = global.setTimeout;
  global.setTimeout = function() { return 0; };  // keep fallback frames from running after the test
  try {
    [{ noRaf: true }, { nullCtx: true }].forEach(function(opt) {
      var res;
      assert.doesNotThrow(function() { res = runPage(cfg(), envE, opt); }, JSON.stringify(opt));
      assert.strictEqual(typeof res.els.save.onclick, 'function', 'save bound ' + JSON.stringify(opt));
      assert.strictEqual(typeof res.els.cancel.onclick, 'function', 'cancel bound ' + JSON.stringify(opt));
    });
  } finally { global.setTimeout = realSetTimeout; }
})();

// Change length helpers (JS twin of src/c/anim_len.c)
assert.strictEqual(config.DEFAULTS.animLen, 255);
assert.strictEqual(config.toMessage(cfg()).ANIM_LEN, 255);
assert.strictEqual(config.toMessage(cfg({ animLen: 0 })).ANIM_LEN, 0);
assert.strictEqual(config.toMessage(cfg({ animLen: 125 })).ANIM_LEN, 125);
var oldCfg = cfg(); delete oldCfg.animLen;
assert.strictEqual(config.toMessage(oldCfg).ANIM_LEN, 255, 'old config without animLen sends natural');
assert.strictEqual(config.animTotalMs(6000, 255), 6000);
assert.strictEqual(config.animTotalMs(6000, 20), 2000);
assert.strictEqual(config.animTotalMs(6000, 0), 0);
assert.strictEqual(config.animScaledMs(1000, 6000, 20), 3000);
assert.strictEqual(config.animScaledMs(5000, 6000, 20), 6000);
assert.strictEqual(config.animScaledMs(1234, 6000, 255), 1234);
assert.strictEqual(config.lenFromSlider(60), 255);
assert.strictEqual(config.lenFromSlider(62), 255);
assert.strictEqual(config.lenFromSlider(63), 63);
assert.strictEqual(config.lenFromSlider(0), 0);
assert.strictEqual(config.lenFromSlider(200), 200);
assert.strictEqual(config.lenFromTyped('', 80), 80);
assert.strictEqual(config.lenFromTyped('abc', 80), 80);
assert.strictEqual(config.lenFromTyped('-3', 80), 0);
assert.strictEqual(config.lenFromTyped('25', 80), 200);
assert.strictEqual(config.lenFromTyped('6.04', 80), 60);   // typed values never snap
assert.strictEqual(config.lenFromTyped('12.35', 80), 124);
assert.strictEqual(config.lenLabel(255), 'About 6 s (recommended)');
assert.strictEqual(config.lenLabel(0), 'Instant');
assert.strictEqual(config.lenLabel(125), '12.5 s');

// Page wiring: slider <-> number box, label, disabled in Battery saver
var lp2 = runPage(cfg(), envE);
assert(lp2.page.indexOf('id="animLenR"') > 0 && lp2.page.indexOf('id="animLenN"') > 0, 'slider and number box');
assert(lp2.page.indexOf('Recommended') > 0, 'notch label');
assert.strictEqual(+lp2.els.animLenR.value, 60);
lp2.els.animLenR.value = '120'; lp2.els.animLenR.oninput();
assert.strictEqual(lp2.win.CFG.animLen, 120);
assert.strictEqual(lp2.els.animLenN.value, '12.0');
lp2.els.animLenN.value = '3.3'; lp2.els.animLenN.onchange();
assert.strictEqual(lp2.win.CFG.animLen, 33);
assert.strictEqual(+lp2.els.animLenR.value, 33);
lp2.els.animLenR.value = '59'; lp2.els.animLenR.oninput();
assert.strictEqual(lp2.win.CFG.animLen, 255, 'slider snaps to recommended');
var lp3 = runPage(cfg({ animMode: 2 }), envE);
assert.strictEqual(lp3.els.animLenR.disabled, true);
assert.strictEqual(lp3.els.animLenN.disabled, true);

assert.strictEqual(config.geoUrl('Park City'), 'https://geocoding-api.open-meteo.com/v1/search?name=Park%20City&count=5&language=en&format=json');
assert.strictEqual(config.geoUrl('a&b=c'), 'https://geocoding-api.open-meteo.com/v1/search?name=a%26b%3Dc&count=5&language=en&format=json');
assert.strictEqual(config.placeLabel({ name: 'Park City', admin1: 'Utah', country: 'United States' }), 'Park City, Utah, United States');
assert.strictEqual(config.placeLabel({ name: 'Monaco', country: 'Monaco' }), 'Monaco, Monaco');
assert.strictEqual(config.DEFAULTS.locAuto, true);
assert.strictEqual(config.DEFAULTS.locLat, null);

// Location UI: hidden when auto, shown when off; search results render as text; picking sets cfg; Save fixes up.
function xhrStub(respond) {   // respond(url) -> {status, body} | 'error'
  return function() {
    var x = this; x.open = function(m, u) { x.url = u; };
    x.send = function() { var r = respond(x.url); if (r === 'error') { x.onerror && x.onerror(); return; }
      x.status = r.status; x.responseText = r.body; x.onload && x.onload(); };
  };
}
var lp4 = runPage(cfg(), envE, { XMLHttpRequest: xhrStub(function() { return { status: 200, body: JSON.stringify({ results: [
  { name: 'Park <City>', admin1: 'Utah', country: 'United States', latitude: 40.646, longitude: -111.498 },
  { name: 'Park City', country: 'Canada', latitude: 50.1, longitude: -100.2 }] }) }; }) });
assert.strictEqual(lp4.els.locManual.style.display, 'none', 'manual location hidden while auto');
lp4.els.locAuto_off.onclick();
assert.notStrictEqual(lp4.els.locManual.style.display, 'none', 'manual location shown when off');
assert(/Pick a place/.test(lp4.els.locNote.textContent), 'warns when no place chosen');
lp4.els.locQ.value = 'Park City'; lp4.els.locFind.onclick();
var items = lp4.els.locList.children;
assert.strictEqual(items.length, 2);
assert.strictEqual(items[0].textContent, 'Park <City>, Utah, United States', 'rendered as text');
assert(!items[0].innerHTML || items[0].innerHTML.indexOf('<City>') < 0, 'not injected as HTML');
items[0].onclick();
assert.strictEqual(lp4.win.CFG.locName, 'Park <City>, Utah, United States');
assert.strictEqual(lp4.win.CFG.locLat, 40.646);
assert.strictEqual(lp4.win.CFG.locLon, -111.498);
assert(/Weather for: Park <City>/.test(lp4.els.locSel.textContent));

var lp5 = runPage(cfg(), envE, { XMLHttpRequest: xhrStub(function() { return { status: 200, body: '{}' }; }) });
lp5.els.locAuto_off.onclick(); lp5.els.locQ.value = 'zzz'; lp5.els.locFind.onclick();
assert.strictEqual(lp5.els.locStatus.textContent, 'No matches');
lp5.els.save.onclick();
var saved = JSON.parse(decodeURIComponent(lp5.win.location.href.split('#')[1]));
assert.strictEqual(saved.locAuto, true, 'off with no place saves as auto');

var lp6 = runPage(cfg(), envE, { XMLHttpRequest: xhrStub(function() { return 'error'; }) });
lp6.els.locAuto_off.onclick(); lp6.els.locQ.value = 'x'; lp6.els.locFind.onclick();
assert.strictEqual(lp6.els.locStatus.textContent, "Couldn't search — check your connection");


// Bad geocoding responses show the error string, never "No matches" or a stuck "Searching".
var ERR = "Couldn't search — check your connection";
function searchWith(resp) {
  var p = runPage(cfg(), envE, { XMLHttpRequest: xhrStub(function() { return resp; }) });
  p.els.locAuto_off.onclick(); p.els.locQ.value = 'x'; p.els.locFind.onclick();
  return p;
}
assert.strictEqual(searchWith({ status: 200, body: 'not json' }).els.locStatus.textContent, ERR, 'junk body');
assert.strictEqual(searchWith({ status: 500, body: '{}' }).els.locStatus.textContent, ERR, 'http 500');
assert.strictEqual(searchWith({ status: 200, body: JSON.stringify({ results: 'x' }) }).els.locStatus.textContent, ERR, 'results not an array');
var lp7 = searchWith({ status: 200, body: JSON.stringify({ results: [
  { name: 'NoLat', longitude: 1 }, { latitude: 1, longitude: 1 }, { name: 'Good', latitude: 2, longitude: 3 }] }) });
assert.strictEqual(lp7.els.locList.children.length, 1, 'only the valid result is listed');
assert.strictEqual(lp7.els.locList.children[0].textContent, 'Good');

console.log('config tests passed');
