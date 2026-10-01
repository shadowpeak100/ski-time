// Minimal DOM + canvas 2D stub: every context method is a no-op recorder, so page code can run in node.
function makeCtx() {
  var calls = 0;
  var target = { canvas: null, calls: function() { return calls; } };
  return new Proxy(target, {
    get: function(t, k) {
      if (k in t) return t[k];
      if (k === 'getImageData' || k === 'createImageData')
        return function(x, y, w, h) { calls++; return { width: w || 1, height: h || 1, data: new Uint8ClampedArray(4 * (w || 1) * (h || 1)) }; };
      if (k === 'measureText') return function() { calls++; return { width: 10 }; };
      return function() { calls++; };
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
