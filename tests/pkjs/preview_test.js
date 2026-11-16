var assert = require('assert');
var config = require('../../src/pkjs/config.js');
var PREVIEW_JS = require('../../src/pkjs/preview.js').PREVIEW_JS;
var makeCanvas = require('./stub_dom.js').makeCanvas;

var sandbox = {};
new Function('window', PREVIEW_JS + ';window.SkiPreview=SkiPreview;')(sandbox);
var P = sandbox.SkiPreview;
function cfg(over) { var c = {}, k; for (k in config.DEFAULTS) c[k] = config.DEFAULTS[k]; for (k in over || {}) c[k] = over[k]; return c; }
var env = { platform: 'emery', wmo: 0, wind: 0, rise: 420, set: 1140 };

// daypartFor mirrors src/c/daypart.c
assert.strictEqual(P.daypartFor(cfg({ dayCycle: false, dayFixed: 3 }), env, 720), 3);
assert.strictEqual(P.daypartFor(cfg({ dayCycle: false, dayFixed: 9 }), env, 720), 1);
assert.strictEqual(P.daypartFor(cfg({ dayMode: 2 }), env, 0), 1);                         // legacy dayMode 2 renders dayFixed (default 1), like the C DPM_STATIC
assert.strictEqual(P.daypartFor(cfg(), env, 720), 1);
assert.strictEqual(P.daypartFor(cfg(), env, 0), 3);
assert.strictEqual(P.daypartFor(cfg({ dayMode: 1, fixedRise: 1200, fixedSet: 300 }), env, 0), 1);   // sunset before sunrise: wrap-aware
assert.strictEqual(P.daypartFor(cfg(), env, 390), 0);   // sunrise 420: 30 min before
assert.strictEqual(P.daypartFor(cfg(), env, 389), 3);
assert.strictEqual(P.daypartFor(cfg(), env, 449), 0);   // 29 min after
assert.strictEqual(P.daypartFor(cfg(), env, 450), 1);
assert.strictEqual(P.daypartFor(cfg(), env, 1110), 2);  // sunset 1140: 30 min before
assert.strictEqual(P.daypartFor(cfg(), env, 1169), 2);
assert.strictEqual(P.daypartFor(cfg(), env, 1170), 3);

// weatherFor
assert.strictEqual(P.weatherFor(cfg({ weatherOn: true }), { platform: 'emery', wmo: 63 }), 5);    // rain
assert.strictEqual(P.weatherFor(cfg({ weatherOn: false, wxFixed: 6 }), env), 6);
assert.strictEqual(P.weatherFor(cfg({ weatherOn: false, wxFixed: 6 }), { platform: 'basalt' }), 0);
assert.strictEqual(P.weatherFor(cfg({ weatherOn: false, wxFixed: 42 }), env), 0);

// fmtTime: noon, midnight, morning, with and without 24h / leading zero
[[true, true], [true, false], [false, true], [false, false]].forEach(function(f) {
  var c = cfg({ use24: f[0], lead0: f[1] });
  [[0, 5], [12, 5], [9, 5], [23, 59]].forEach(function(t) {
    var s = P.fmtTime(c, t[0], t[1]);
    assert.strictEqual(s.length, 4, 'four digit cells ' + JSON.stringify([f, t, s]));
  });
});
assert.strictEqual(P.fmtTime(cfg({ use24: true, lead0: true }), 9, 5), '0905');
assert.strictEqual(P.fmtTime(cfg({ use24: false, lead0: true }), 0, 5), '1205');

// draw: every daypart x weather x platform x anim mode, resting and mid-animation, without throwing
var now = new Date(2026, 9, 1, 12, 30, 38);
['emery', 'basalt'].forEach(function(platform) {
  for (var dp = 0; dp < 4; dp++) for (var wx = 0; wx < 8; wx++) for (var am = 0; am < 3; am++) {
    var c = cfg({ dayCycle: false, dayFixed: dp, weatherOn: false, wxFixed: wx, animMode: am });
    var e = { platform: platform, wmo: 0, wind: 1, rise: 420, set: 1140 };
    [null, 0, 450, 700, 1500, 2600, 4000, 9000].forEach(function(ms) {
      var cv = makeCanvas();
      var r = P.draw(cv, c, e, { now: now, animMs: ms, wxClock: 3.7 });
      assert.strictEqual(cv.width, platform === 'basalt' ? 144 : 200);
      assert.strictEqual(cv.height, platform === 'basalt' ? 168 : 228);
      assert(r && r.durationMs >= 4500 && r.durationMs <= 7500, 'duration ' + (r && r.durationMs));
      var gctx = cv.getContext('2d'), img = gctx.lastImage();
      assert(gctx.count('putImageData') >= 1, 'blitted');
      assert.strictEqual(img.width * img.height, platform === 'basalt' ? 144 * 168 : 200 * 228);
      assert(img.data.some(function(v) { return v !== 0; }), 'image not blank');
    });
  }
});

console.log('preview tests passed');
