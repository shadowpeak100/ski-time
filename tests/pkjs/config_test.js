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

function runPage(c, env, opt) {
  opt = opt || {};
  var page = config.buildHtml(c, env);
  var els = {}, raf = [], ids = page.match(/id="([^"]+)"/g).map(function(s) { return s.slice(4, -1); });
  ids.forEach(function(id) {
    els[id] = id === 'pv' ? makeCanvas() : { id: id, style: {}, value: '', setAttribute: function() {}, addEventListener: function() {} };
  });
  var win = {
    document: { getElementById: function(id) { if (!els[id]) throw new Error('missing #' + id); return els[id]; } },
    location: {},
    requestAnimationFrame: opt.noRaf ? undefined : function(f) { raf.push(f); return raf.length; },
    cancelAnimationFrame: function() {},
    performance: { now: function() { return 0; } },
    Date: Date
  };
  if (opt.nullCtx) els.pv.getContext = function() { return null; };
  var js = page.match(/<script>([\s\S]*)<\/script>/)[1];
  new Function('window', 'document', 'location', 'requestAnimationFrame', 'cancelAnimationFrame', 'performance',
    js + ';window.CFG=CFG;window.SkiPreview=SkiPreview;')(win, win.document, win.location, win.requestAnimationFrame, win.cancelAnimationFrame, win.performance);
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

console.log('config tests passed');
