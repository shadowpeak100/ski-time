// Minimal DOM + canvas 2D stub: every context method is a no-op recorder, so page code can run in node.
function makeCtx() {
  var calls = 0, byName = {}, lastImage = null;
  function note(k) { calls++; byName[k] = (byName[k] || 0) + 1; }
  var target = { canvas: null, calls: function() { return calls; }, count: function(name) { return byName[name] || 0; },
                 lastImage: function() { return lastImage; } };
  return new Proxy(target, {
    get: function(t, k) {
      if (k in t) return t[k];
      if (k === 'createImageData')
        return function(w, h) { note(k); return { width: w, height: h, data: new Uint8ClampedArray(4 * w * h) }; };
      if (k === 'getImageData')
        return function(x, y, w, h) { note(k); return { width: w || 1, height: h || 1, data: new Uint8ClampedArray(4 * (w || 1) * (h || 1)) }; };
      if (k === 'measureText') return function() { note(k); return { width: 10 }; };
      if (k === 'putImageData') return function(img) { note(k); lastImage = img; };
      return function() { note(k); };
    },
    set: function(t, k, v) { t[k] = v; return true; }
  });
}
function makeCanvas() {
  var c = { width: 300, height: 150, style: {}, attrs: {} };
  var ctx = makeCtx(); ctx.canvas = c;
  c.getContext = function() { return ctx; };
  c.setAttribute = function(k, v) { this.attrs[k] = v; };
  return c;
}
module.exports = { makeCanvas: makeCanvas };
