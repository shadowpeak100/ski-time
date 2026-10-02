var assert = require('assert');

function load(savedCfg, geo) {
  var handlers = {}, opened = [], sent = [], store = { cfg: savedCfg ? JSON.stringify(savedCfg) : null };
  global.localStorage = { getItem: function(k) { return k in store ? store[k] : null; }, setItem: function(k, v) { store[k] = v; } };
  global.Pebble = {
    addEventListener: function(n, f) { handlers[n] = f; },
    sendAppMessage: function(m, ok) { sent.push(m); ok && ok(); },
    openURL: function() {}, getActiveWatchInfo: function() { return { platform: 'emery' }; }
  };
  // Node 21+ defines navigator as a getter-only global, so plain assignment is ignored.
  Object.defineProperty(global, 'navigator', { configurable: true, writable: true,
    value: { geolocation: { getCurrentPosition: function(ok) { geo.calls++; ok({ coords: { latitude: 10.5, longitude: 20.25 } }); } } } });
  global.XMLHttpRequest = function() { var x = this; x.open = function(m, u) { opened.push(u); }; x.send = function() {}; };
  global.setInterval = function() {};
  delete require.cache[require.resolve('../../src/pkjs/index.js')];
  require('../../src/pkjs/index.js');
  return { handlers: handlers, opened: opened, sent: sent, store: store };
}

// No saved config: geolocation.
var geo = { calls: 0 }, h = load(null, geo);
h.handlers.ready();
assert.strictEqual(geo.calls, 1);
assert(/latitude=10\.5000&longitude=20\.2500/.test(h.opened[0]), h.opened[0]);

// Old config without location fields: geolocation.
geo = { calls: 0 }; h = load({ comp1: 1, animMode: 1 }, geo);
h.handlers.ready();
assert.strictEqual(geo.calls, 1);

// Manual place: no geolocation, its coordinates in the URL.
geo = { calls: 0 }; h = load({ locAuto: false, locName: 'Park City', locLat: 40.646, locLon: -111.498 }, geo);
h.handlers.ready();
assert.strictEqual(geo.calls, 0);
assert(/latitude=40\.6460&longitude=-111\.4980/.test(h.opened[0]), h.opened[0]);

// Saving a changed location refetches at once; saving without a change does not.
geo = { calls: 0 }; h = load({ locAuto: true }, geo);
var before = h.opened.length;
h.handlers.webviewclosed({ response: encodeURIComponent(JSON.stringify({ locAuto: false, locName: 'X', locLat: 1, locLon: 2 })) });
assert.strictEqual(h.opened.length, before + 1, 'refetch after location change');
assert(/latitude=1\.0000&longitude=2\.0000/.test(h.opened[h.opened.length - 1]));
before = h.opened.length;
h.handlers.webviewclosed({ response: encodeURIComponent(JSON.stringify({ locAuto: false, locName: 'X', locLat: 1, locLon: 2 })) });
assert.strictEqual(h.opened.length, before, 'no refetch when location unchanged');

console.log('index tests passed');
