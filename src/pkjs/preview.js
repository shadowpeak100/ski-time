// Settings-page preview: a port of the watch's drawing code (src/c/) to canvas, inlined into the page as a string.
// Everything is drawn into a software framebuffer of 0xRRGGBB pixels, the way the C draws into the Pebble
// framebuffer, then blitted as one ImageData so the pixels stay crisp. Prototype: docs/prototypes/weather.html.

function skiPreviewFactory() {
  'use strict';
  var PI = Math.PI, floor = Math.floor, sqrt = Math.sqrt, sin = Math.sin, cos = Math.cos;
  var FALL = { x: -0.6, y: 0.8 }, ACROSS = { x: 0.8, y: 0.6 };
  var GLYPH_SLANT = 0.16;

  // Timings (avalanche.h, skier.h).
  var AV_CRACK_MS = 450, AV_CRACK_DRAW_MS = 500, AV_FLOW_START_MS = 1000, AV_FLOW_END_MS = 2800, AV_FADE_MS = 450;
  var CRACK_END_MS = AV_FLOW_START_MS + 150, AV_MAX_S = 320;
  var SKI_DRAW_START_MS = 2600, SKI_STAGGER_MS = 200, SKI_PAIR_MS = 150, SKI_STOP_MS = 300, SKI_EXIT_GAP_MS = 30;
  var SKI_MAX = 8, SKI_EXIT_SEGS = 16, SKI_REST_MS = 0xFFFF;

  var WHITE = 0xFFFFFF, LIBERTY = 0x5555AA, OXFORD = 0x000055, DARKGRAY = 0x555555, LIGHTGRAY = 0xAAAAAA,
    BULGARIAN = 0x550000, CHROME = 0xFFAA00, WINDSOR = 0xAA5500, ICTERINE = 0xFFFF55, PASTEL = 0xFFFFAA, ORANGE = 0xFF5500;
  var JACKETS = [0xFF0000, 0xFFAA00, 0x0055FF, 0xAA00FF, 0xFF5500, 0x00AAFF, 0xFF0055, 0x00AA55];

  // theme.c, indexed by Daypart: 0 sunrise, 1 midday, 2 sunset, 3 night. body: kind (0 sun, 1 moon), x, y, r, halo, core, glow.
  function theme(sky, body, night, far, near, forest, pineLit, pineShade, pineSnow, snow, snowShade, snowShadow, text, cloud) {
    return { sky: sky, body: body, stars: night, night: night, far: far, near: near, forest: forest, pineLit: pineLit,
      pineShade: pineShade, pineSnow: pineSnow, snow: snow, snowShade: snowShade, snowShadow: snowShadow, powder: WHITE,
      text: text, cloud: cloud };
  }
  var THEMES = [
    theme([0xAAAAFF, 0xFFAAFF, 0xFFFFAA], [0, 0.10, 0.25, 8, 10, 0xFFFF55, 0xFFFFAA], false,
      [0xFFAAFF, 0xAA55AA, 0xFFFFAA, 0xFFAAFF], [0xAA55AA, 0x550055, 0xFFAAAA, 0xFFAAFF],
      0x5555AA, 0x005500, 0x005555, 0xFFAAFF, 0xFFFFFF, 0xFFAAFF, 0xAAAAFF, 0xAA55AA, [0xFFFFAA, 0xFFAAFF, 0xAA55AA]),
    theme([0x55AAFF, 0xAAFFFF], [0, 0.13, 0.08, 7, 6, 0xFFFF55, 0xFFFFAA], false,
      [0xAAAAFF, 0x5555AA, 0xFFFFFF, 0xAAFFFF], [0x0055AA, 0x000055, 0xFFFFFF, 0xAAAAFF],
      0x55AAAA, 0x005500, 0x005555, 0xFFFFFF, 0xFFFFFF, 0xAAFFFF, 0xAAAAFF, 0x5555AA, [0xFFFFFF, 0xAAFFFF, 0xAAAAFF]),
    theme([0xAA0055, 0xFF0000, 0xFF5500, 0xFFAA00], [0, 0.30, 0.20, 10, 8, 0xFFFF55, 0xFFAA00], false,
      [0xFFAA55, 0xAA55AA, 0xFFFFAA, 0xFFAAAA], [0xAA0055, 0x550055, 0xFFAA55, 0xFFAAAA],
      0x550055, 0x005500, 0x000000, 0xFFAAAA, 0xFFFFFF, 0xFFAAAA, 0xFFAAFF, 0xAA0055, [0xFFAA55, 0xFF5500, 0xAA0055]),
    theme([0x000000, 0x000000, 0x000055], [1, 0.15, 0.10, 6, 0, 0, 0], true,
      [0x5555AA, 0x000055, 0xAAAAFF, 0x5555AA], [0x000055, 0x000000, 0x5555AA, 0x000055],
      0x000055, 0x005555, 0x000000, 0xAAAAFF, 0xAAAAFF, 0x5555AA, 0x5555AA, 0x000055, [0x5555AA, 0xAAAAFF, 0x000055])
  ];

  // weather.c. WxKind 0 clear, 1 partly, 2 overcast, 3 fog, 4 drizzle, 5 rain, 6 snow, 7 storm.
  function wxd(clouds, deck, sky, fog256, precip, rate, len, speed, lightning) {
    return { clouds: clouds, deck: deck, sky: sky, fog256: fog256, precip: precip, rate: rate, len: len, speed: speed, lightning: lightning };
  }
  var WX = [wxd(0, false, 0, 0, 0, 0, 0, 0, false), wxd(3, false, 0, 0, 0, 0, 0, 0, false),
    wxd(0, true, 1, 0, 0, 0, 0, 0, false), wxd(0, true, 1, 218, 0, 0, 0, 0, false),
    wxd(0, true, 1, 0, 1, 32, 2, 110, false), wxd(0, true, 1, 0, 1, 70, 4, 190, false),
    wxd(0, true, 1, 0, 2, 95, 0, 26, false), wxd(0, true, 2, 0, 1, 90, 5, 230, true)];
  var OVC = [[0xAAAAAA, 0xFFAAFF], [0xAAAAAA, 0xFFFFFF], [0x555555, 0xAA5555, 0xFFAA55], [0x000000, 0x000055]];
  var STM = [[0x555555, 0xAA55AA], [0x555555, 0xAAAAAA], [0x000000, 0x550000, 0xAA5555], [0x000000, 0x555555]];
  var STORM_CLOUD = [[0xAA55AA, 0x550055, 0x550055], [0xAAAAAA, 0x555555, 0x555555], [0xAA5555, 0x550000, 0x550000],
    [0x555555, 0x000055, 0x000055]];
  var RAIN = [0xAA55AA, 0x5555AA, 0x550055, 0xAAAAFF], FOG = [0xFFAAFF, 0xFFFFFF, 0xFFAAAA, 0x555555];

  function wxFromWmo(c) {
    if (c === 0) return 0;
    if (c === 1 || c === 2) return 1;
    if (c === 3) return 2;
    if (c === 45 || c === 48) return 3;
    if (c >= 51 && c <= 57) return 4;
    if ((c >= 61 && c <= 67) || (c >= 80 && c <= 82)) return 5;
    if ((c >= 71 && c <= 77) || c === 85 || c === 86) return 6;
    if (c >= 95 && c <= 99) return 7;
    return 0;
  }
  function wxSky(d, w) { var s = WX[w].sky; return s === 0 ? THEMES[d].sky : s === 2 ? STM[d] : OVC[d]; }
  function wxCloudRoles(d, w) { return WX[w].sky === 2 ? STORM_CLOUD[d] : THEMES[d].cloud; }

  // ---------- maths ----------
  function clamp(v, a, b) { return v < a ? a : v > b ? b : v; }
  function smooth(t) { return t * t * (3 - 2 * t); }
  function fround(v) { return floor(v + 0.5); }
  function trunc(v) { return v < 0 ? Math.ceil(v) : floor(v); }
  function norm(x, y) { var l = sqrt(x * x + y * y) || 1; return { x: x / l, y: y / l }; }
  function wrapPi(a) { while (a > PI) a -= 2 * PI; while (a < -PI) a += 2 * PI; return a; }
  var BAYER = [0, 8, 2, 10, 12, 4, 14, 6, 3, 11, 1, 9, 15, 7, 13, 5];
  function bayer(x, y) { return BAYER[((y & 3) << 2) | (x & 3)]; }
  function dith(x, y, p) { return bayer(x, y) * 16 + 8 < p * 256; }
  function dith256(x, y, p256) { return bayer(x, y) * 16 + 8 < p256; }
  function mulberry(a) {   // geom.c mulberry_next
    return function() {
      a = (a + 0x6D2B79F5) | 0;
      var t = Math.imul(a ^ (a >>> 15), 1 | a);
      t = (t + Math.imul(t ^ (t >>> 7), 61 | t)) ^ t;
      return ((t ^ (t >>> 14)) >>> 0) / 4294967296;
    };
  }
  function noise(x) { return clamp(0.5 + 0.3 * sin(x) + 0.2 * sin(x * 2.3 + 1.7), 0, 1); }   // aval_noise
  function sinL(a) { return Math.round(sin((a & 0xFFFF) / 65536 * 2 * PI) * 65536); }       // Pebble sin_lookup
  function cosL(a) { return Math.round(cos((a & 0xFFFF) / 65536 * 2 * PI) * 65536); }
  function cubic(a, b, c, d, t) { var m = 1 - t; return m * m * m * a + 3 * m * m * t * b + 3 * m * t * t * c + t * t * t * d; }

  // ---------- framebuffer ----------
  var W = 0, H = 0, FB = null;
  function put(x, y, c) { if (x >= 0 && y >= 0 && x < W && y < H) FB[y * W + x] = c; }
  function hspan(x0, x1, y, c) {   // [x0, x1)
    if (y < 0 || y >= H) return;
    if (x0 < 0) x0 = 0;
    if (x1 > W) x1 = W;
    for (var x = x0; x < x1; x++) FB[y * W + x] = c;
  }
  function rect(x, y, w, h, c) { for (var j = 0; j < h; j++) hspan(x, x + w, y + j, c); }
  // gpath_draw_filled stand-in: integer vertices, the top row and the boundary pixels of each span left out.
  // Of the rules tried, this one matched emulator screenshots most closely.
  function fillPoly(xy, c) {
    var n = xy.length / 2, px = [], py = [], i, y0 = 1e9, y1 = -1e9;
    for (i = 0; i < n; i++) {
      px[i] = floor(xy[2 * i]); py[i] = floor(xy[2 * i + 1]);
      if (py[i] < y0) y0 = py[i];
      if (py[i] > y1) y1 = py[i];
    }
    for (var y = y0; y <= y1; y++) {
      var xs = [];
      for (i = 0; i < n; i++) {
        var j = (i + 1) % n, ay = py[i], by = py[j];
        if ((ay < y && by >= y) || (by < y && ay >= y)) xs.push(px[i] + (y - ay) * (px[j] - px[i]) / (by - ay));
      }
      xs.sort(function(a, b) { return a - b; });
      for (i = 0; i + 1 < xs.length; i += 2) hspan(floor(xs[i]) + 1, floor(xs[i + 1]), y, c);
    }
  }
  function fillTri(x0, y0, x1, y1, x2, y2, c) { fillPoly([x0, y0, x1, y1, x2, y2], c); }
  function fillEllipse(cx, cy, rx, ry, c) {   // fb_fill_ellipse
    for (var y = floor(cy - ry); y <= floor(cy + ry); y++) {
      var dy = (y + 0.5 - cy) / ry;
      if (dy * dy > 1) continue;
      var hw = rx * sqrt(1 - dy * dy);
      hspan(fround(cx - hw), fround(cx + hw), y, c);
    }
  }
  function fillRotEllipse(cx, cy, rx, ry, rot, c) {   // scene.c fill_rot_ellipse
    var cs = cos(rot), sn = sin(rot), irx = 1 / (rx * rx), iry = 1 / (ry * ry);
    var A = cs * cs * irx + sn * sn * iry, Bk = 2 * sn * cs * (irx - iry), Ck = sn * sn * irx + cs * cs * iry;
    var ext = sqrt(rx * rx * sn * sn + ry * ry * cs * cs);
    for (var y = floor(cy - ext); y <= floor(cy + ext); y++) {
      var Y = y + 0.5 - cy, B = Bk * Y, C = Ck * Y * Y - 1, disc = B * B - 4 * A * C;
      if (disc < 0) continue;
      var sq = sqrt(disc);
      hspan(fround(cx + (-B - sq) / (2 * A)), fround(cx + (-B + sq) / (2 * A)), y, c);
    }
  }
  function fillCircle(cx, cy, r, c) {   // graphics_fill_circle stand-in
    for (var dy = -r; dy <= r; dy++)
      for (var dx = -r; dx <= r; dx++) if (dx * dx + dy * dy <= r * r + r) put(cx + dx, cy + dy, c);
  }
  // Round-capped stroke of width w: pixels whose centre lies within w/2 of the segment (scene.c capsule).
  function capsule(x0, y0, x1, y1, w, c) {
    var r = w / 2, dx = x1 - x0, dy = y1 - y0, ll = dx * dx + dy * dy;
    for (var y = floor(Math.min(y0, y1) - r); y <= floor(Math.max(y0, y1) + r); y++)
      for (var x = floor(Math.min(x0, x1) - r); x <= floor(Math.max(x0, x1) + r); x++) {
        var px = x + 0.5 - x0, py = y + 0.5 - y0, t = ll > 0 ? clamp((px * dx + py * dy) / ll, 0, 1) : 0;
        var ex = px - t * dx, ey = py - t * dy;
        if (ex * ex + ey * ey <= r * r) put(x, y, c);
      }
  }
  function line(x, y, x1, y1, c) {   // graphics_draw_line, integer pixels (Bresenham as wx_draw.c core_polyline)
    var dx = Math.abs(x1 - x), sx = x < x1 ? 1 : -1, dy = -Math.abs(y1 - y), sy = y < y1 ? 1 : -1, err = dx + dy;
    for (;;) {
      put(x, y, c);
      if (x === x1 && y === y1) break;
      var e2 = 2 * err;
      if (e2 >= dy) { err += dy; x += sx; }
      if (e2 <= dx) { err += dx; y += sy; }
    }
  }
  function linef(x0, y0, x1, y1, c) { line(floor(x0), floor(y0), floor(x1), floor(y1), c); }

  // ---------- layout.c ----------
  function makeLayout(w, h) {
    var L = { W: w, H: h, k: w / 200 }, k = L.k;
    L.compH = fround(h * 0.14); L.compTop = h - L.compH;
    L.yL = trunc((h * 34 + 50) / 100); L.yR = trunc((h * 15 + 50) / 100); L.bulge = h * 13 / 200;
    L.gw = fround(w * 0.21); L.gh = fround(L.gw * 1.2);
    var gx = fround(w * 0.035), gy = fround(h * 0.028);
    var row1 = fround(h / 2 - (2 * L.gh + gy) / 2), row2 = row1 + L.gh + gy;   // block centred on the screen
    L.exitCeiling = L.compTop - 8;
    var stag = fround(w * 0.07), rowW = L.gw * 2 + gx, cw = fround(w * 0.24), chh = fround(cw * 0.95);
    L.chalet = { x: w - cw - fround(3 * k), y: L.compTop - chh - 1, w: cw, h: chh };
    var mx = L.chalet.x - fround(8 * k) - rowW, hx = mx + stag;
    L.boxes = [{ x: hx, y: row1 }, { x: hx + L.gw + gx, y: row1 }, { x: mx, y: row2 }, { x: mx + L.gw + gx, y: row2 }];
    var cx = w - 26 * k, cy = ridgeY(L, cx) + 9 * k;
    L.u0 = cx * FALL.x + cy * FALL.y;
    L.s0 = cx * ACROSS.x + cy * ACROSS.y;
    L.u1 = -6 * FALL.x + L.compTop * FALL.y + 16 * k;
    L.sMin = Math.min(0, w * ACROSS.x, h * ACROSS.y, w * ACROSS.x + h * ACROSS.y);
    L.sMax = Math.max(0, w * ACROSS.x, h * ACROSS.y, w * ACROSS.x + h * ACROSS.y);
    L.ridgeRows = []; L.bermRows = [];   // scene_init
    for (var x = 0; x < w; x++) {
      L.ridgeRows[x] = Math.ceil(ridgeY(L, x) + 0.5);
      L.bermRows[x] = Math.min(h, floor(bermY(L, x)));
    }
    return L;
  }
  function ridgeY(L, x) { var f = x / L.W; return L.yL + (L.yR - L.yL) * f - sin(PI * clamp(f, 0, 1)) * L.bulge; }
  function bermY(L, x) { return L.compTop - 1 + sin(x * 0.07) * 1.2 * L.k; }

  // ---------- scene.c background ----------
  function pine(th, x, base, h) {
    rect(fround(x - 0.5), fround(base - h * 0.16), 1, Math.ceil(h * 0.16), BULGARIAN);
    for (var i = 0; i < 3; i++) {
      var top = base - h + i * h * 0.27, bot = top + h * 0.46, hw = h * (0.2 + i * 0.1), t = bot - top;
      fillTri(x - hw, bot, x, top, x + hw, bot, th.pineShade);
      fillTri(x - hw, bot, x, top, x, bot, th.pineLit);
      fillPoly([x, top, x - hw * 0.55, top + t * 0.55, x - hw * 0.2, top + t * 0.42, x + hw * 0.25, top + t * 0.45], th.pineSnow);
    }
  }
  function peak(px, py, hw, base, c) {
    var split = px + hw * 0.12;
    fillTri(px - hw, base, px, py, px + hw, base, c[1]);
    fillTri(px - hw, base, px, py, split, base, c[0]);
    var dep = (base - py) * 0.3, lx = px - hw * 0.3, rx = px + hw * 0.3, ly = py + dep, ry = py + dep * 1.1;
    var cl = [px, py, lx, ly, lx + (px - lx) * 0.35, ly - dep * 0.25, px - hw * 0.05, ly + dep * 0.1, px + hw * 0.02, ly - dep * 0.2];
    var cr = [px + hw * 0.12, ry - dep * 0.05, px + hw * 0.2, ry - dep * 0.35, rx, ry];
    fillPoly(cl.concat(cr), c[3]);
    fillPoly(cl, c[2]);
  }
  function skyPass(L, th, d, w, emery) {
    var B = th.body, sky = emery ? wxSky(d, w) : th.sky, ns = sky.length - 1, k = L.k;
    var sx = W * B[1], sy = H * B[2], sr = B[3] * k, halo = B[4] * k, horizon = ridgeY(L, 0) * 0.92;
    var sun = B[0] === 0 && !(emery && WX[w].deck);   // cloud cover hides the sun
    for (var y = 0; y < H; y++) {
      var f = clamp(y / horizon, 0, 1), t = Math.pow(f, 1.2) * ns, dy = y + 0.5 - sy;
      var i = Math.min(floor(t), ns - 1), lo = sky[i], hi = sky[i + 1];
      t -= i;
      for (var x = 0; x < W; x++) {
        FB[y * W + x] = dith(x, y, t) ? hi : lo;
        if (!sun) continue;
        var dx = x + 0.5 - sx, d2 = dx * dx + dy * dy;
        if (d2 < sr * sr) FB[y * W + x] = B[5];
        else if (d2 < (sr + halo) * (sr + halo)) {
          var q = 1 - (sqrt(d2) - sr) / halo;
          if (dith(x, y, Math.pow(q, 1.4) * 0.85)) FB[y * W + x] = B[6];
        }
      }
    }
  }
  function starsPass(L) {
    var r = mulberry(99);
    for (var n = 0; n < 48; n++) {
      var x = floor(r() * W), y = floor(r() * (H * 0.36)), c = r();
      if (y > ridgeY(L, x) - 3) continue;
      put(x, y, c < 0.7 ? 0xFFFFFF : c < 0.9 ? 0xAAFFFF : 0xFFFFAA);
      if (n % 9) continue;
      put(x + 1, y, LIBERTY); put(x - 1, y, LIBERTY); put(x, y + 1, LIBERTY); put(x, y - 1, LIBERTY);
    }
  }
  function moonPass(L, th, moon) {
    var B = th.body, k = L.k, mx = W * B[1], my = H * B[2], mr = B[3] * k, rim = 3 * k / mr, kx = cos(2 * PI * moon);
    for (var y = floor(my - mr - 3 * k); y <= my + mr + 3 * k; y++)
      for (var x = floor(mx - mr - 3 * k); x <= mx + mr + 3 * k; x++) {
        if (x < 0 || y < 0 || x >= W || y >= H) continue;
        var xn = (x + 0.5 - mx) / mr, yn = (y + 0.5 - my) / mr, r2 = xn * xn + yn * yn, rr = sqrt(r2);
        if (rr > 1) {
          if (rr < 1 + rim && dith(x, y, 0.3 * (1 - (rr - 1) / rim))) put(x, y, OXFORD);
          continue;
        }
        var xt = sqrt(Math.max(0, 1 - yn * yn)) * kx, lit = moon < 0.5 ? xn > xt : xn < -xt;
        put(x, y, !lit ? OXFORD : r2 > 0.72 ? LIGHTGRAY : WHITE);
      }
  }
  function shadePass(L, th) {
    var k = L.k, x, y;
    for (x = 0; x < W; x++) {
      var ry = ridgeY(L, x);
      for (y = Math.ceil(ry); y < ry + 8 * k && y < H; y++) if (dith(x, y, (1 - (y - ry) / (8 * k)) * 0.6)) put(x, y, th.snowShade);
      var by = bermY(L, x);
      for (y = floor(by - 1); y < by + 5 * k && y < H; y++) {
        var dy = y - by, p = dy <= 0 ? 0.75 : (1 - dy / (5 * k)) * 0.5;
        if (y >= 0 && dith(x, y, p)) put(x, y, th.snowShade);
      }
    }
  }

  // flood.c
  var FLOODS = [[0.95, 0.30, 0.40, 0.36], [0.97, 0.50, 0.38, 0.53], [0.965, 0.64, 0.40, 0.70], [0.07, 0.45, 0.62, 0.36], [0.05, 0.76, 0.62, 0.62]];
  var COSH = 0.9135455;
  function floodHeads(k) {
    return FLOODS.map(function(f) {
      var bx = f[0] * W, by = f[1] * H, hy = by - 16 * k, d = norm(f[2] * W - bx, f[3] * H - hy);
      return { bx: bx, by: by, hx: bx, hy: hy, dx: d.x, dy: d.y };
    });
  }
  function floodAt(heads, k, x, y) {
    var best = 0, len = 215 * k;
    for (var i = 0; i < heads.length; i++) {
      var h = heads[i], vx = x - h.hx, vy = y - h.hy, d2 = vx * vx + vy * vy;
      if (d2 > len * len || d2 < 1) continue;
      var dot = vx * h.dx + vy * h.dy;
      if (dot <= 0 || dot * dot <= COSH * COSH * d2) continue;
      var d = sqrt(d2), c = dot / d, edge = smooth(clamp((c - COSH) / (1 - COSH) * 2.2, 0, 1));
      var f = 1 - d / len, fall = 0.5 * (f + sqrt(f) * sqrt(sqrt(f)));
      var v = edge * fall * smooth(clamp((d - 4 * k) / (12 * k), 0, 1));
      if (v > best) best = v;
    }
    return best;
  }
  // Night: window light on the snow in front of the chalet, then the floodlight cones (scene.c night_pass).
  function nightPass(L, heads) {
    var k = L.k, c = L.chalet, cb = c.y + c.h, x, y;
    for (y = fround(cb - 1); y < cb + 5 * k && y < H; y++)
      for (x = fround(c.x - 6 * k); x < c.x + c.w * 0.6 && x < W; x++) {
        var ax = x - (c.x + c.w * 0.2), p = 0.5 * (1 - (y - cb) / (5 * k)) * (1 - Math.abs(ax) / (c.w * 0.55));
        if (x >= 0 && p > 0 && dith(x, y, p)) put(x, y, PASTEL);
      }
    var ylo = [];
    for (x = 0; x < W; x++) ylo[x] = Math.ceil(ridgeY(L, x) + 1);
    for (var x0 = 0; x0 < W; x0 += 2)
      for (var y0 = 0; y0 < H; y0 += 2) {
        var cream = 0, white = 0, evaluated = false;
        for (x = x0; x < x0 + 2 && x < W; x++)
          for (y = Math.max(y0, ylo[x]); y < y0 + 2 && y < L.bermRows[x]; y++) {
            if (!evaluated) {
              var I = floodAt(heads, k, x0 + 1, y0 + 1) * 1.3;
              evaluated = true;
              if (I > 0) { cream = Math.ceil((I - 0.68) * 256); white = Math.ceil(I * 256); }
            }
            if (white <= 0) break;
            var b = bayer(x, y) * 16 + 8;
            if (b < cream) put(x, y, PASTEL);
            else if (b < white) put(x, y, WHITE);
          }
      }
  }
  function floodPoles(heads) {
    heads.forEach(function(f) {
      var bx = fround(f.bx), by = fround(f.by), hx = fround(f.hx), hy = fround(f.hy), sx = f.dx < 0 ? -1 : 1;
      rect(bx, hy + 1, 1, by - hy, OXFORD);
      rect(bx - 1, by - 1, 3, 1, OXFORD);
      rect(hx - (sx < 0 ? 2 : 0), hy - 1, 3, 3, OXFORD);
      rect(hx + 3 * sx, hy - 1, 1, 3, ICTERINE);
      rect(hx + 4 * sx, hy, 1, 1, PASTEL);
    });
  }

  // decor.c: Emery open-slope pines, back to front. behind = 1: drawn each frame over the skiers.
  var SLOPE_TREES = [
    [130, 41, 9, 0], [136, 41, 8, 0], [123, 42, 15, 0], [109, 44, 9, 0], [118, 45, 11, 0], [50, 62, 12, 0], [44, 64, 8, 0],
    [39, 65, 14, 0], [54, 67, 9, 0], [37, 71, 8, 0], [21, 73, 10, 0], [15, 76, 13, 0], [9, 78, 11, 0], [23, 79, 10, 0],
    [50, 80, 16, 0], [41, 86, 16, 0], [46, 99, 8, 0], [30, 100, 16, 0], [38, 104, 9, 0], [183, 114, 7, 1], [181, 120, 11, 1],
    [175, 121, 7, 1], [151, 128, 8, 1], [157, 128, 10, 1], [152, 133, 14, 1], [159, 133, 11, 1], [38, 151, 11, 1],
    [32, 153, 12, 1], [26, 154, 8, 1], [107, 181, 11, 1], [117, 182, 9, 1], [54, 184, 8, 1], [99, 184, 7, 1], [64, 187, 7, 1],
    [106, 187, 9, 1], [113, 187, 13, 1], [46, 188, 9, 1], [9, 189, 8, 1], [22, 189, 7, 1], [120, 189, 8, 1], [39, 190, 9, 1],
    [51, 190, 11, 1], [16, 191, 7, 1], [58, 191, 7, 1]];
  function slopeTrees(th, which) {
    SLOPE_TREES.forEach(function(t) {
      if (t[3] !== which) return;
      fillEllipse(t[0] + 2, t[1], t[2] * 0.42, 1.6, th.snowShade);
      pine(th, t[0], t[1], t[2]);
    });
  }

  // Partly-cloudy cloud: five bumps over a flat-based ellipse, in shade, mid and lit layers.
  var BUMPS = [[-20, 1, 6, 4], [-10, -1, 8, 5], [1, -2, 9, 6], [12, -1, 8, 5], [22, 1, 6, 4]];
  var FLAT_LAYER = [[1, 1.5, 1, 1, 4, 3.2], [0, 0.6, 0.96, 0.95, 3, 2.4], [-0.8, -0.8, 0.82, 0.75, 0, 0]];
  function flatCloud(x, y, c, s) {
    for (var l = 0; l < 3; l++) {
      var q = FLAT_LAYER[l], col = c[2 - l];
      for (var i = 0; i < 5; i++) {
        var b = BUMPS[i];
        fillEllipse(x + b[0] * s + q[0], y + b[1] * s + q[1], b[2] * s * q[2], b[3] * s * q[3], col);
      }
      if (q[5] > 0) fillEllipse(x + s, y + q[4] * s, 28 * s, q[5] * s, col);
    }
  }
  var PUFFS = [[-14, 4, 7], [-6, -2, 9], [5, -4, 10], [15, 1, 8], [22, 5, 6], [0, 6, 9], [-18, 7, 5]];
  var DECK_LAYER = [[1, 2, 1], [0, 0.8, 0.96], [-0.8, -0.8, 0.8]];
  function deckCloud(x, y, c, s) {
    for (var l = 0; l < 3; l++)
      for (var i = 0; i < 7; i++) {
        var r = PUFFS[i][2] * s * DECK_LAYER[l][2];
        fillEllipse(x + PUFFS[i][0] * s + DECK_LAYER[l][0], y + PUFFS[i][1] * s + DECK_LAYER[l][1], r, r, c[2 - l]);
      }
  }
  function deckPass(k, cl) {
    var back = [cl[1], cl[2], cl[2]], r = mulberry(5), x, y;
    for (x = -4; x < W + 20; x += 18 * k) { y = (16 + r() * 6) * k; deckCloud(x, y, back, (0.65 + r() * 0.25) * k); }
    for (x = -10; x < W + 20; x += 15 * k) { y = (3 + r() * 6) * k; deckCloud(x, y, cl, (0.75 + r() * 0.3) * k); }
  }
  function fogPass(L, fog256, c) {
    for (var y = 0; y < L.compTop; y++) {
      var p256 = fog256 * (1 - clamp((y - H * 0.08) / (H * 0.5), 0, 1));
      if (p256 <= 0) continue;
      for (var x = 0; x < W; x++) if (dith256(x, y, p256)) FB[y * W + x] = c;
    }
  }

  function buildBg(L, d, w, moon, emery) {
    var th = THEMES[d], df = WX[w], k = L.k, i, x;
    skyPass(L, th, d, w, emery);
    if (!(emery && df.deck)) {
      if (th.stars) starsPass(L);
      if (th.body[0] === 1) moonPass(L, th, moon);
    }
    if (emery) {
      var flat = [[0.62, 0.07, 1], [0.2, 0.06, 0.8], [0.9, 0.13, 0.7]];
      for (i = 0; i < df.clouds && i < 3; i++) flatCloud(flat[i][0] * W, flat[i][1] * H, wxCloudRoles(d, w), flat[i][2] * k);
      if (df.deck) deckPass(k, wxCloudRoles(d, w));
    }
    [[0.05, 0.17, 0.17], [0.22, 0.12, 0.15], [0.40, 0.10, 0.17], [0.58, 0.06, 0.15], [0.74, 0.035, 0.12]].forEach(function(p) {
      peak(p[0] * W, p[1] * H, p[2] * W, H * 0.45, th.far);
    });
    [[-0.04, 0.215, 0.2], [0.18, 0.22, 0.15], [0.34, 0.19, 0.13]].forEach(function(p) {
      peak(p[0] * W, p[1] * H, p[2] * W, H * 0.5, th.near);
    });
    var r = mulberry(7);
    for (x = W * 0.16; x < W * 0.8; x += 3.5 * k + r() * 3 * k) {
      var b = ridgeY(L, x) + 1, h = (4 + r() * 5) * k * (x > W * 0.62 ? 0.6 : 1), hw = h * 0.35;
      fillTri(x - hw, b, x, b - h, x + hw, b, th.forest);
    }
    var r3 = mulberry(33);
    for (x = W * 0.17; x < W * 0.86; x += (4 + r3() * 5) * k) {
      var base = ridgeY(L, x) + 2 * k, ph = (8 + r3() * 7) * k;
      pine(th, x, base, Math.min(ph, base - 2));
    }
    var slope = [-2, floor(ridgeY(L, 0))];
    for (i = 0; i <= trunc((W + 1) / 2); i++) slope.push(i * 2, floor(ridgeY(L, i * 2)));
    slope.push(W + 2, floor(ridgeY(L, W)), W + 2, H + 2, -2, H + 2);
    fillPoly(slope, th.snow);
    shadePass(L, th);
    if (emery && th.night) {
      var heads = floodHeads(k);
      nightPass(L, heads);
      floodPoles(heads);
    }
    var trees = [], r2 = mulberry(21);
    for (x = W * 0.2; x < W * 0.8 && trees.length < 40; x += (5 + r2() * 6) * k) {
      var tb = ridgeY(L, x) + 1.5 * k, th2 = Math.min((11 + r2() * 11) * k, tb - 3), tx = x + r2() * 3 * k, j = trees.length;
      trees.push(null);
      for (; j > 0 && trees[j - 1][1] > tb; j--) trees[j] = trees[j - 1];
      trees[j] = [tx, tb, th2];
    }
    trees.forEach(function(t) { pine(th, t[0], t[1], t[2]); });
    if (emery) {
      if (df.fog256) fogPass(L, df.fog256, FOG[d]);
      slopeTrees(th, 0);
    }
  }

  // scene_draw_chalet
  function drawChalet(L, th) {
    var k = L.k, c = L.chalet, fw = c.w * 0.52, fx0 = c.x, fx1 = c.x + fw, ex = c.x + c.w, b = c.y + c.h, rise = c.h * 0.1, run = ex - fx1;
    var ey = c.y + c.h * 0.5, ax = fx0 + fw / 2, ay = c.y + c.h * 0.06, i, x;
    function up(x) { return rise * (x - fx1) / run; }
    pine(th, ex - 3 * k, b - rise - c.h * 0.25, c.h * 0.9);
    fillRotEllipse(fx1 + run * 0.55, b - rise * 0.4 + 1, c.w * 0.42, 2.5 * k, -0.1, th.snowShade);
    fillPoly([fx1, ey, ex, ey - rise, ex, b - rise, fx1, b], BULGARIAN);
    var wh = (b - ey) * 0.36;
    [0.18, 0.58].forEach(function(f) {
      var x0 = fx1 + run * f, x1 = x0 + run * 0.26, y0 = ey + (b - ey) * 0.28;
      fillPoly([x0, y0 - up(x0), x1, y0 - up(x1), x1, y0 + wh - up(x1), x0, y0 + wh - up(x0)], CHROME);
      var xm = (x0 + x1) / 2;
      rect(fround(xm), fround(y0 - up(xm)), 1, fround(wh), BULGARIAN);
    });
    fillPoly([ax, ay + 2 * k, fx0, ey, fx0, b, fx1, b, fx1, ey], WINDSOR);
    var apex = ay + 2 * k;   // front logs, clipped to the gable-ended front face
    for (var y = ay + 4 * k; y < b; y += 3 * k) {
      var ry = fround(y), yc = ry + 0.5, hw = yc >= ey ? fw / 2 : (yc - apex) / (ey - apex) * fw / 2;
      if (hw > 0) hspan(fround(ax - hw), fround(ax + hw), ry, BULGARIAN);
    }
    var dw = fround(fw * 0.3), dh = fround((b - ey) * 0.58);
    rect(fround(ax - dw / 2), fround(b - dh), dw, dh, BULGARIAN);
    rect(fround(ax + dw / 2) - 2, fround(b - dh * 0.5), 1, 1, CHROME);
    rect(fround(fx0 - 1), fround(ey), fround(fw + 2), 2, BULGARIAN);
    for (x = fx0; x <= fx1; x += 3 * k) rect(fround(x), fround(ey - 3 * k), 1, fround(3 * k), BULGARIAN);
    rect(fround(fx0 - 1), fround(ey - 3 * k), fround(fw + 2), 1, BULGARIAN);
    var ws = Math.max(4, fround(fw * 0.26)), wx = fround(ax - ws / 2), wy = fround(ay + (ey - ay) * 0.42);
    rect(wx, wy, ws, ws, CHROME);
    rect(wx + 1, wy + 1, ws - 2, ws - 2, ICTERINE);
    rect(fround(ax), wy, 1, ws, BULGARIAN);
    var o = 2 * k;
    fillPoly([ax, ay, ax + run, ay - rise, ex + o, ey - rise + o, fx1 + o, ey + o], th.powder);
    fillPoly([fx1 + o, ey + o, ex + o, ey - rise + o, ex + o, ey - rise + o - 2.5 * k, fx1 + o, ey + o - 2.5 * k], th.snowShade);
    for (x = floor(fx1 + o); x < fround(ex + o); x++) {
      var edge = ey + o - rise * (x + 0.5 - (fx1 + o)) / run;
      rect(x, Math.ceil(edge - 0.5), 1, 1, BULGARIAN);
    }
    for (x = fx1 + o + 2 * k; x < ex; x += 3 * k) rect(fround(x), fround(ey + o + 1 - up(x)), 1, 1 + (trunc(x * 7) % 2), th.snowShade);
    [[fx0 - o, ey + o], [fx1 + o, ey + o]].forEach(function(e) {
      capsule(ax, ay, e[0], e[1], 2 * k, BULGARIAN);
      capsule(ax, ay - k, e[0], e[1] - 1.2 * k, 1.4 * k, th.powder);
    });
    var cx = ax + run * 0.62, cy = ay - rise * 0.62 + (ey - ay) * 0.3;
    rect(fround(cx - 1.5 * k), fround(cy - 6 * k), fround(3 * k), fround(7 * k), DARKGRAY);
    rect(fround(cx - 2 * k), fround(cy - 6 * k - 1), fround(4 * k), 2, th.powder);
    L.chimX = cx; L.chimY = cy - 6 * k - 2;
  }
  function drawSmoke(L, th, ms) {
    var k = L.k, clock = ms / 1000;
    for (var i = 0; i < 4; i++) {
      var p = clock * 0.22 + i / 4;
      p -= floor(p);
      if (p > 0.9) continue;
      var r = (0.8 + p * 2.4) * k;
      fillEllipse(L.chimX + p * 8 * k + sin(p * 6 + i) * 1.2 * k, L.chimY - p * 16 * k, r, r, p < 0.45 ? LIGHTGRAY : th.snowShade);
    }
  }
  function ditherEllipse(cx, cy, rx, ry, c, p) {
    for (var y = floor(cy - ry); y <= floor(cy + ry); y++) {
      var dy = (y + 0.5 - cy) / ry;
      if (y < 0 || y >= H || dy * dy > 1) continue;
      var hw = rx * sqrt(1 - dy * dy);
      for (var x = fround(cx - hw); x < fround(cx + hw); x++) if (dith(x, y, p)) put(x, y, c);
    }
  }
  function tongue(x, y, h, sway, s, k, c) {   // two quadratic curves from the base corners to a swaying tip
    var N = 6, px = [], py = [], w = 1.9 * k * s, cw = 1.2 * k * s, cy = y - h * 0.6 * s, ty = y - h * s, i;
    for (i = 0; i <= N; i++) {
      var t = i / N, m = 1 - t, a = m * m, b = 2 * m * t, d = t * t;
      px[i] = a * (x - w) + b * (x - cw) + d * (x + sway); py[i] = a * y + b * cy + d * ty;
      px[N + i] = a * (x + sway) + b * (x + cw) + d * (x + w); py[N + i] = a * ty + b * cy + d * y;
    }
    for (var row = floor(ty); row <= floor(y); row++) {
      var yc = row + 0.5, lo = 1e9, hi = -1e9;
      for (i = 0; i < 2 * N; i++) {
        var y0 = py[i], y1 = py[i + 1];
        if ((y0 <= yc) === (y1 <= yc)) continue;
        var xc = px[i] + (yc - y0) * (px[i + 1] - px[i]) / (y1 - y0);
        if (xc < lo) lo = xc;
        if (xc > hi) hi = xc;
      }
      if (hi >= lo) hspan(fround(lo), fround(hi), row, c);
    }
  }
  function drawCampfire(L, ms) {
    var k = L.k, clock = ms / 1000, fx = L.chalet.x - 9 * k, fy = L.compTop - 5 * k, i;
    ditherEllipse(fx, fy, 10 * k, 3.2 * k, 0xFFAA55, 0.35);
    ditherEllipse(fx, fy, 5 * k, 1.8 * k, PASTEL, 0.3);
    linef(fx - 4 * k, fy, fx + 4 * k, fy - 1.5 * k, BULGARIAN);
    linef(fx - 4 * k, fy - 1.5 * k, fx + 4 * k, fy, BULGARIAN);
    var flame = [0xFF0000, 0xFF5500, 0xFFAA00, 0xFFFF55], scale = [1, 0.72, 0.46, 0.24];
    for (i = 0; i < 3; i++) {
      var ox = (i - 1) * 1.8 * k, h = (5 + 2.4 * noise(clock * 6 + i * 2.1)) * k * (i === 1 ? 1.3 : 0.85), sw = sin(clock * 9 + i * 1.7) * 0.9 * k;
      for (var j = 0; j < (i === 1 ? 4 : 3); j++) tongue(fx + ox, fy - 0.8 * k, h, sw, scale[j], k, flame[j]);
    }
    for (var x = fx - 5 * k; x <= fx + 5 * k; x += 2.5 * k) rect(fround(x), fround(fy), 1, 1, DARKGRAY);
    for (i = 0; i < 3; i++) {
      var q = clock * 0.8 + i / 3;
      q -= floor(q);
      if (q > 0.8) continue;
      rect(fround(fx + sin(q * 9 + i) * 2 * k), fround(fy - 7 * k - q * 10 * k), 1, 1, q < 0.45 ? CHROME : ORANGE);
    }
  }

  // ---------- glyphs.c, skier.c ----------
  var GLYPHS = [
    [[.50, .02, .15, .02, .02, .55, .22, .86, .36, 1.06, .66, 1.00, .88, .93], [.56, .02, .92, .04, 1.00, .55, .78, .86, .64, 1.06, .34, 1.00, .12, .93]],
    [[.84, .02, .72, .02, .64, .12, .60, .30, .55, .55, .50, .78, .46, .90, .42, 1.00, .32, 1.02, .22, .96]],
    [[.16, .20, .20, .03, .46, -.02, .64, .04, .86, .12, .86, .36, .66, .52, .46, .68, .24, .80, .14, .90, .06, .98, .14, 1.04, .30, .96, .44, .90, .62, .98, .92, .96]],
    [[.18, .12, .30, -.02, .72, -.04, .76, .16, .80, .34, .60, .44, .42, .46, .34, .47, .34, .52, .46, .50, .70, .48, .88, .60, .84, .76, .80, .96, .46, 1.04, .10, .88]],
    [[.54, .00, .40, .22, .22, .44, .10, .58, .04, .66, .08, .70, .20, .66, .44, .62, .70, .60, .94, .62], [.76, .08, .72, .40, .66, .72, .60, .90, .56, 1.00, .46, 1.02, .36, .96]],
    [[.90, .02, .70, .05, .50, .02, .36, .04, .31, .18, .29, .32, .27, .44, .46, .34, .86, .40, .84, .68, .82, .96, .46, 1.04, .10, .88]],
    [[.80, .04, .46, .10, .18, .44, .20, .74, .22, 1.00, .70, 1.04, .80, .78, .88, .56, .60, .46, .42, .54, .26, .62, .16, .80, .04, .98]],
    [[.10, .08, .30, .00, .60, .06, .84, .03, .97, .02, .94, .12, .86, .20, .70, .38, .50, .62, .40, .86, .36, .98, .26, 1.02, .16, .95]],
    [[.50, .02, .16, .02, .14, .38, .50, .50, .86, .62, .90, 1.00, .50, 1.00, .30, 1.00, .18, .94, .12, .86], [.54, .02, .88, .02, .86, .38, .50, .50, .14, .62, .10, 1.00, .50, 1.00, .70, 1.00, .82, .94, .88, .86]],
    [[.80, .06, .62, -.04, .24, -.02, .18, .22, .12, .46, .58, .54, .78, .30, .84, .20, .94, .14, .90, .30, .84, .56, .72, .82, .64, .94, .56, 1.04, .44, 1.02, .34, .95]]];

  // time_slots: digits per slot (0 hour tens .. 3 minute ones); without a leading zero a one-digit hour leaves slot 0 empty.
  function timeSlots(cfg, h24, m) {
    var hh = cfg.use24 ? h24 : (h24 % 12 === 0 ? 12 : h24 % 12), out = [];
    if (hh >= 10 || cfg.lead0) out.push([floor(hh / 10), 0]);
    out.push([hh % 10, 1], [floor(m / 10), 2], [m % 10, 3]);
    return out;
  }
  function fmtTime(cfg, h, m) {
    var s = [' ', ' ', ' ', ' '];
    timeSlots(cfg, h, m).forEach(function(d) { s[d[1]] = String(d[0]); });
    return s.join('');
  }

  function sampleChain(ch, map, step, out, max) {   // geom_sample_chain
    if (max < 1 || ch.length < 8) return 0;
    var prev = map(ch[0], ch[1]), need = step, x0 = ch[0], y0 = ch[1], start = out.length;
    out.push(prev);
    for (var i = 2; i + 5 < ch.length; i += 6) {
      for (var s = 1; s <= 60; s++) {
        var t = s / 60, cur = map(cubic(x0, ch[i], ch[i + 2], ch[i + 4], t), cubic(y0, ch[i + 1], ch[i + 3], ch[i + 5], t));
        var ax = prev.x, ay = prev.y, seg = sqrt((cur.x - ax) * (cur.x - ax) + (cur.y - ay) * (cur.y - ay));
        while (seg >= need) {
          var r = need / seg;
          ax += (cur.x - ax) * r; ay += (cur.y - ay) * r;
          if (out.length - start < max) out.push({ x: ax, y: ay });
          seg -= need; need = step;
        }
        need -= seg;
        prev = cur;
      }
      x0 = ch[i + 4]; y0 = ch[i + 5];
    }
    if (out.length - start < max) out.push(prev);
    return out.length - start;
  }
  function tangent(p, i, w, q) {   // geom_tangent / tangent_pts (q: points in 1/16 px)
    var n = p.length, a = p[Math.max(0, i - w)], b = p[Math.min(n - 1, i + w)], s = q ? 16 : 1;
    return norm((b.x - a.x) / s, (b.y - a.y) / s);
  }
  function q4(v) { return trunc(v * 16 + (v >= 0 ? 0.5 : -0.5)); }
  function ptHeading(p, i) { var a = p[i < 2 ? 0 : i - 2], b = p[Math.min(p.length - 1, i + 2)]; return Math.atan2(b.y - a.y, b.x - a.x); }
  function cub(c, t) {
    var m = 1 - t, a = m * m * m, b = 3 * m * m * t, d = 3 * m * t * t, e = t * t * t;
    return { x: a * c[0].x + b * c[1].x + d * c[2].x + e * c[3].x, y: a * c[0].y + b * c[1].y + d * c[2].y + e * c[3].y };
  }
  function cubD(c, t) {
    var m = 1 - t;
    return { x: 3 * m * m * (c[1].x - c[0].x) + 6 * m * t * (c[2].x - c[1].x) + 3 * t * t * (c[3].x - c[2].x),
      y: 3 * m * m * (c[1].y - c[0].y) + 6 * m * t * (c[2].y - c[1].y) + 3 * t * t * (c[3].y - c[2].y) };
  }
  var REST = norm(0.92, 0.39);

  function buildOne(set, L, ch, slot, t0, color) {
    var k = L.k, capPts = trunc(L.W * 7 / 4) + 24, b = L.boxes[slot], st = [], en = [];
    var ns = sampleChain(ch, function(u, v) { return { x: b.x + (u + (1 - v) * GLYPH_SLANT - GLYPH_SLANT / 2) * L.gw, y: b.y + v * L.gh }; }, 1, st, capPts);
    if (ns < 2) return null;
    var start = st[0], d0 = tangent(st, 0, 3);
    var P0 = { x: L.W + 10, y: clamp(start.y - (L.W + 10 - start.x) * 0.45, ridgeY(L, L.W) + 8 * k, L.compTop - 10) };
    var len = sqrt((start.x - P0.x) * (start.x - P0.x) + (start.y - P0.y) * (start.y - P0.y)), a = norm(-1, 0.45);
    var ne = sampleChain([P0.x, P0.y, P0.x + a.x * len * 0.35, P0.y + a.y * len * 0.35,
      start.x - d0.x * len * 0.4, start.y - d0.y * len * 0.4, start.x, start.y], function(u, v) { return { x: u, y: v }; }, 1, en, capPts - ns);
    var cap = set.poolCap - set.poolUsed, p = [], i;
    for (i = 0; i < ne - 1 && p.length < cap; i++) p.push({ x: q4(en[i].x), y: q4(en[i].y), t: 0 });
    var sIdx = p.length;
    for (i = 0; i < ns && p.length < cap; i++) p.push({ x: q4(st[i].x), y: q4(st[i].y), t: 0 });
    var n = p.length;
    if (n <= sIdx + 1) return null;
    var t = t0;
    p[0].t = t0;
    for (i = 1; i < n; i++) {
      var dx = (p[i].x - p[i - 1].x) / 16, dy = (p[i].y - p[i - 1].y) / 16, ds = sqrt(dx * dx + dy * dy);
      var tg = tangent(p, i, 2, true), down = tg.x * FALL.x + tg.y * FALL.y;
      var v = 78 * k * (0.72 + 0.55 * Math.max(0, down)) * (1 - 0.35 * (tg.y < 0 ? -tg.y : 0));
      if (i < sIdx) v = v + (170 * k - v) * smooth(clamp((sIdx - i) / (30 * k), 0, 1));
      t += ds / v * 1000;
      p[i].t = floor(t + 0.5) & 0xFFFF;
    }
    var sk = { pts: p, n: n, sIdx: sIdx, t0: t0, tEnd: p[n - 1].t, color: color };
    sk.end = { x: p[n - 1].x / 16, y: p[n - 1].y / 16 };
    sk.dEnd = tangent(p, n - 1, 3, true);
    sk.endH = Math.atan2(sk.dEnd.y, sk.dEnd.x);
    var rd = sk.dEnd.x * REST.x + sk.dEnd.y * REST.y >= 0 ? REST : { x: -REST.x, y: -REST.y };
    sk.restH = sk.endH + wrapPi(Math.atan2(rd.y, rd.x) - sk.endH);
    sk.stopLen = 4 * k;
    var P = sk.restPos = { x: sk.end.x + sk.dEnd.x * sk.stopLen, y: sk.end.y + sk.dEnd.y * sk.stopLen };
    var X = { x: -14, y: P.y + (P.x + 14) * 0.35 };   // build_exit
    sk.ex = [P, { x: P.x + FALL.x * 14 * k, y: P.y + FALL.y * 14 * k }, { x: X.x - FALL.x * 22 * k, y: X.y - FALL.y * 22 * k }, X];
    for (i = 1; i < 4; i++) if (sk.ex[i].y > L.exitCeiling) sk.ex[i] = { x: sk.ex[i].x, y: L.exitCeiling };
    sk.exLen = [0];
    var prev = P;
    for (i = 1; i <= SKI_EXIT_SEGS; i++) {
      var q = cub(sk.ex, i / SKI_EXIT_SEGS);
      sk.exLen[i] = sk.exLen[i - 1] + sqrt((q.x - prev.x) * (q.x - prev.x) + (q.y - prev.y) * (q.y - prev.y));
      prev = q;
    }
    set.poolUsed += n;
    return sk;
  }
  function buildSet(L, cfg, h, m) {
    var set = { sk: [], poolCap: L.W * 9, poolUsed: 0 };
    timeSlots(cfg, h, m).forEach(function(ds) {
      var strokes = GLYPHS[ds[0]], slot = ds[1];
      for (var j = 0; j < strokes.length && set.sk.length < SKI_MAX; j++) {
        var sk = buildOne(set, L, strokes[j], slot, SKI_DRAW_START_MS + slot * SKI_STAGGER_MS + j * SKI_PAIR_MS, (slot * 2 + j) % 8);
        if (sk) set.sk.push(sk);
      }
    });
    return set;
  }
  function skiersDuration(set) {
    var m = 0;
    set.sk.forEach(function(s) { if (s.tEnd > m) m = s.tEnd; });
    return m + SKI_STOP_MS + 300;
  }
  function skierState(s, t) {   // x, y in 1/16 px
    if (s.n < 2 || t < s.t0) return null;
    if (t <= s.tEnd) {
      var lo = 0, hi = s.n - 1;
      while (hi - lo > 1) { var m = (lo + hi) >> 1; if (s.pts[m].t <= t) lo = m; else hi = m; }
      var a = s.pts[lo], b = s.pts[hi], span = b.t - a.t, f = span > 0 ? floor(((t - a.t) << 8) / span) : 0;
      var h0 = ptHeading(s.pts, lo), h1 = ptHeading(s.pts, hi);
      return { x: a.x + ((b.x - a.x) * f >> 8), y: a.y + ((b.y - a.y) * f >> 8), h: h0 + wrapPi(h1 - h0) * f / 256, idx: lo, spray: 0 };
    }
    var u = Math.min(1, (t - s.tEnd) / SKI_STOP_MS), e = 1 - (1 - u) * (1 - u), d = s.stopLen * e;
    return { x: q4(s.end.x + s.dEnd.x * d), y: q4(s.end.y + s.dEnd.y * d), h: s.endH + (s.restH - s.endH) * smooth(u),
      idx: s.n - 1, spray: u < 1 ? trunc(1 + u * 254) : 0 };
  }
  function skierExitState(s, order, t, k) {
    var teMs = t - order * SKI_EXIT_GAP_MS;
    if (teMs <= 0) return { x: q4(s.restPos.x), y: q4(s.restPos.y), h: s.restH };
    var te = teMs / 1000, dist = 0.5 * 380 * k * te * te + 20 * k * te;
    if (dist >= s.exLen[SKI_EXIT_SEGS]) return null;
    var i = 0;
    while (s.exLen[i + 1] < dist) i++;
    var tt = (i + (dist - s.exLen[i]) / (s.exLen[i + 1] - s.exLen[i])) / SKI_EXIT_SEGS;
    var q = cub(s.ex, tt), dv = cubD(s.ex, tt), hp = s.restH + wrapPi(Math.atan2(dv.y, dv.x) - s.restH);
    return { x: q4(q.x), y: q4(q.y), h: s.restH + (hp - s.restH) * smooth(clamp(te / 0.35, 0, 1)) };
  }

  // sprites.c
  function polyline(s, from, to, fn) {
    var a = s.pts[from];
    for (var i = from + 2;; i += 2) {
      if (i > to) i = to;
      var b = s.pts[i];
      fn(a.x >> 4, a.y >> 4, b.x >> 4, b.y >> 4);
      a = b;
      if (i === to) break;
    }
  }
  function drawTrack(s, from, to, solid) {
    if (to <= from || to >= s.n) return;
    polyline(s, from, to, function(x0, y0, x1, y1) { capsule(x0 + 0.5, y0 + 0.5, x1 + 0.5, y1 + 0.5, 3, DARKGRAY); });
    if (!solid) polyline(s, from, to, function(x0, y0, x1, y1) { line(x0, y0, x1, y1, WHITE); });
  }
  function drawSkier(xq, yq, h, color, k) {
    var x = xq / 16, y = yq / 16, s = Math.max(k, 0.85), len = Math.max(3.6 * k, 3.2), cx = cos(h), sy = sin(h), px = -sy, py = cx, o;
    for (o = -1; o <= 1; o += 2)
      linef(x + px * o * 2.4 * s - cx * 0.5, y + py * o * 2.4 * s - sy * 0.5 - 1.6 * s, x + px * o * 2.8 * s - cx * 3.2 * s, y + py * o * 2.8 * s - sy * 3.2 * s, DARKGRAY);
    for (o = -1; o <= 1; o += 2) {
      var ox = px * o * 1.05 * s, oy = py * o * 1.05 * s;
      linef(x - cx * len + ox, y - sy * len + oy, x + cx * len * 1.15 + ox, y + sy * len * 1.15 + oy, OXFORD);
    }
    var bx = x + cx * 0.5, by = y + sy * 0.5 - 1.7 * s;
    fillCircle(floor(bx), floor(by), 2, JACKETS[color & 7]);
    fillCircle(floor(bx + cx * 0.5), floor(by - 1.9 * s), 1, OXFORD);
  }
  var SPRAY_HASH = [210, 27, 205, 161, 199, 144];
  function drawSpray(th, xq, yq, h, spray, k) {
    var x = xq / 16, y = yq / 16, u = spray / 255, c = u < 0.5 ? th.snowShadow : th.snowShade;
    for (var j = 0; j < 6; j++) {
      var a = h + PI / 2 + (2 * j - 5) * 0.14, r = (2 + u * 5) * k * (0.7 + SPRAY_HASH[j] / 256 * 0.5);
      put(fround(x + cos(a) * r), fround(y + sin(a) * r), c);
    }
  }

  // ---------- avalanche.c, aval_draw.c ----------
  var STREAK = [];
  for (var si = 0; si < AV_MAX_S; si++) { var sv = 0.5 + 0.5 * sin(si * 0.55 + 3 * noise(si * 0.1)); STREAK[si] = trunc(Math.pow(sv, 8) * 255); }
  function avalFrame(L, t) {
    var A = { active: t >= AV_FLOW_START_MS && t <= AV_FLOW_END_MS + AV_FADE_MS };
    if (!A.active) return A;
    var k = L.k, a = Math.min(1, (t - AV_FLOW_START_MS) / (AV_FLOW_END_MS - AV_FLOW_START_MS));
    A.uf = L.u0 + (L.u1 - L.u0) * Math.pow(a, 1.25);
    A.fade = t > AV_FLOW_END_MS ? 1 - (t - AV_FLOW_END_MS) / AV_FADE_MS : 1;
    A.grow = Math.min(1, a * 5) * A.fade;
    var amp = Math.min(1, a * 2.5), tau = t / 1000;
    A.front5 = [];
    for (var s = 0; s < AV_MAX_S; s++)
      A.front5[s] = trunc((A.uf + amp * k * (14 * (noise(s * 0.045 + 1.3) - 0.5) + 6 * (noise(s * 0.13 + tau * 1.7) - 0.5))) * 5);
    A.body5 = trunc((26 * k * A.grow + 1) * 5);
    A.sh0 = trunc(-45 * k); A.sh1 = trunc(-10 * k); A.wake5 = trunc(200 * k);
    A.grow256 = trunc(A.grow * 256); A.fade256 = trunc(A.fade * 256);
    return A;
  }
  function avalFront(A, s) { return A.front5[clamp(trunc(s + 0.5), 0, AV_MAX_S - 1)] / 5; }
  function churn(s, u5, t) {   // Q8 churned-snow texture
    var a1 = s * 4068 + trunc(20861 * sinL(438 * u5 - 66 * t) / 65536);
    var a2 = 688 * u5 - 103 * t + trunc(15646 * cosL(2816 * s) / 65536);
    return clamp(128 + trunc(72 * sinL(a1) / 65536) + trunc(56 * sinL(a2) / 65536), 0, 255);
  }
  // 0 none, 1 shadow, 2 restore, 3 white, 4 celeste, 5 blue (aval_pixel)
  function avalPixel(A, x, y, t) {
    var u5 = 4 * y - 3 * x, s = clamp(trunc((4 * x + 3 * y) / 5), 0, AV_MAX_S - 1), dep = A.front5[s] - u5, p;
    if (dep < A.sh0) return 0;
    if (dep < 0) {
      if (dep < A.sh1) {
        p = trunc(trunc(trunc(128 * (dep - A.sh0) / (A.sh1 - A.sh0)) * A.grow256) / 256);
        if (dith256(x, y, p)) return 1;
      }
      return 0;
    }
    if (dep < A.body5) {
      var t256 = trunc(dep * 256 / A.body5), c = churn(s, u5, t);
      if (c > 220 && t256 < 154) return 5;
      p = trunc(trunc(trunc((c - 102) * 461 / 256) * (256 - t256)) / 256);
      return dith256(x, y, p) ? 4 : 3;
    }
    if (dep < A.body5 + A.wake5) {
      var w256 = trunc(trunc((256 - trunc((dep - A.body5) * 256 / A.wake5)) * A.fade256) / 256);
      p = trunc(trunc(trunc(STREAK[s] * w256 / 256) * 205) / 256);
      if (dith256(x, y, p)) return 4;
    }
    return 2;
  }
  var H53 = [210, 127, 250, 243, 41, 26, 90, 240, 95, 58, 82, 171, 117, 32, 194, 222, 191, 118, 77, 51, 85, 60, 32, 145, 190, 95, 165, 162];
  var H91 = [210, 131, 174, 119, 18, 242, 204, 43, 132, 227, 126, 17, 242, 254, 166, 233, 26, 64, 10, 106, 1, 12, 132, 129, 128, 73, 231, 114];
  var H37 = [210, 253, 6, 177, 232, 240, 229, 18, 109, 179, 13, 33, 161, 185, 129, 200, 57, 122, 144, 76,
    36, 123, 240, 195, 115, 223, 117, 88, 126, 154, 22, 199, 145, 250, 49, 115, 59, 93, 174, 34, 122, 221, 139, 212, 29, 54,
    123, 209, 124, 122, 200, 15, 218, 220, 160, 198, 205, 20, 182, 144, 163, 83, 139, 153, 253, 178, 124, 140, 13, 22];
  function hash2(m, fr) {
    var h = (Math.imul(m, 0x9E3779B1) ^ Math.imul((fr + 0x7F4A7C15) >>> 0, 0x85EBCA77)) >>> 0;
    h = (h ^ (h >>> 15)) >>> 0; h = Math.imul(h, 0x2C1B3C6D) >>> 0; h = (h ^ (h >>> 12)) >>> 0;
    return (h & 0xFF) / 256;
  }
  function apt(u, s) { return { x: FALL.x * u + ACROSS.x * s, y: FALL.y * u + ACROSS.y * s }; }
  function inView(L, q) { return q.x > -14 && q.x < L.W + 14 && q.y > -14 && q.y < L.H + 14; }
  function onSlope(L, x, y) { return x >= 0 && x < L.W && y >= L.ridgeRows[x] && y < L.bermRows[x]; }
  function disc(x, y, r, c) { fillCircle(floor(x), floor(y), Math.max(1, fround(r)), c); }
  function crackPt(L, s, step) {
    var j = ((fround(s / step) & 1) ? 1 : -1) * 1.1 * L.k, q = apt(L.u0 + j, L.s0 + s);
    return { x: floor(q.x), y: floor(q.y) };
  }
  // The crack runs across the visible slope from its low-s end, easing out, over AV_CRACK_DRAW_MS.
  function drawCrack(L, t) {
    var k = L.k, step = 2.5 * k, half = 70 * k, grow = Math.min(1, (t - AV_CRACK_MS) / AV_CRACK_DRAW_MS), s, p;
    grow = 1 - (1 - grow) * (1 - grow);
    var lo = half, hi = -half;
    for (s = -half; s <= half; s += step) { p = crackPt(L, s, step); if (onSlope(L, p.x, p.y)) { if (s < lo) lo = s; hi = s; } }
    if (hi < lo) return;
    var end = lo + (hi - lo) * grow, prev = null;
    for (s = -half; s <= end; s += step) {
      p = crackPt(L, s, step);
      if (prev && onSlope(L, prev.x, prev.y) && onSlope(L, p.x, p.y)) line(prev.x, prev.y, p.x, p.y, LIBERTY);
      prev = p;
    }
  }
  function avalDraw(L, th, bg, t) {
    var A = avalFrame(L, t), crack = t >= AV_CRACK_MS && t < CRACK_END_MS, x, y, s, m;
    if (A.active) {
      var cols = [0, th.snowShadow, 0, th.powder, th.snowShade, th.snowShadow];
      for (y = 0; y < L.compTop; y++)
        for (x = 0; x < W; x++) {
          if (y < L.ridgeRows[x] || y >= L.bermRows[x]) continue;
          var v = avalPixel(A, x, y, t), i = y * W + x;
          if (v === 2) FB[i] = bg[i];
          else if (v) FB[i] = cols[v];
        }
    }
    if (crack) drawCrack(L, t);
    if (A.active) {
      var k = L.k, tau = t / 1000, grow = A.grow, span = L.sMax - L.sMin, r, q;
      for (var top = 0; top < 2; top++)   // second rank of billows, riding on the moving snow
        for (s = L.sMin; s <= L.sMax; s += 5 * k) {
          r = k * (2.5 + 4 * noise(s * 0.17 + tau * 1.4)) * grow;
          if (r < 0.6) continue;
          q = apt(avalFront(A, s) - (11 + 5 * noise(s * 0.09)) * k * grow, s);
          if (!inView(L, q)) continue;
          if (top) disc(q.x, q.y, r * 0.82, th.powder);
          else disc(q.x + 0.8 * k, q.y + 1.1 * k, r, th.snowShade);
        }
      for (var layer = 0; layer < 4; layer++)   // leading billows: halo, shade, mid, top
        for (s = L.sMin - 10; s <= L.sMax + 10; s += 3.2 * k) {
          r = k * (3.5 + 5 * noise(s * 0.11 + 3.1) + 1.8 * noise(s * 0.23 + tau * 2.6)) * grow;
          if (r < 0.6) continue;
          q = apt(avalFront(A, s) - r * 0.7 + sin(tau * 6 + s * 0.3) * 1.2 * k, s);
          if (!inView(L, q)) continue;
          if (layer === 0) ditherCircle(L, floor(q.x), floor(q.y), fround(r + 2 * k), th.snowShade, 77);
          else if (layer === 1) disc(q.x + 0.8 * k, q.y + 1.2 * k, r, th.snowShadow);
          else if (layer === 2) disc(q.x + 0.3 * k, q.y + 0.5 * k, r * 0.95, th.snowShade);
          else disc(q.x - 0.4 * k, q.y - 0.5 * k, r * 0.8, th.powder);
        }
      var cs = Math.max(1, fround(2 * k));   // tumbling debris thrown ahead of the front
      for (m = 0; m < 28; m++) {
        s = L.sMin + H53[m] / 256 * span;
        q = apt(avalFront(A, s) + (2 + H91[m] / 256 * 10) * k * grow, s);
        if (!inView(L, q)) continue;
        var hop = Math.abs(sin(tau * 9 + m * 1.7)) * 2.5 * k, qx = fround(q.x), qy = fround(q.y), hy = fround(q.y - hop);
        rect(qx, qy + 1, cs, 1, th.snowShadow);
        rect(qx, hy - cs + 1, cs, cs, th.powder);
        rect(qx + cs - 1, hy, 1, 1, th.snowShade);
      }
      var fr = trunc(tau * 14);   // loose snow sparkling just ahead of the front
      for (m = 0; m < 70; m++) {
        s = L.sMin + H37[m] / 256 * span;
        q = apt(avalFront(A, s) + hash2(m, fr) * 9 * k * grow, s);
        put(fround(q.x), fround(q.y), m % 3 ? th.snowShadow : LIGHTGRAY);
      }
    }
    if (crack || A.active)   // scene_restore_outside_slope
      for (y = 0; y < H; y++)
        for (x = 0; x < W; x++) if (y < L.ridgeRows[x] || y >= L.bermRows[x]) FB[y * W + x] = bg[y * W + x];
  }
  function ditherCircle(L, cx, cy, r, c, p256) {   // fb_dither_circle, limited to the slope rows
    for (var y = Math.max(0, cy - r); y <= Math.min(H - 1, cy + r); y++)
      for (var x = Math.max(0, cx - r); x <= Math.min(W - 1, cx + r); x++) {
        var dx = x - cx, dy = y - cy;
        if (dx * dx + dy * dy > r * r || y < L.ridgeRows[x] || y >= L.bermRows[x]) continue;
        if (dith256(x, y, p256)) FB[y * W + x] = c;
      }
  }

  // ---------- weather.c, bolt.c, wx_draw.c (Emery only) ----------
  function mix(x) {
    x = (x ^ (x >>> 16)) >>> 0; x = Math.imul(x, 0x7feb352d) >>> 0;
    x = (x ^ (x >>> 15)) >>> 0; x = Math.imul(x, 0x846ca68b) >>> 0;
    return (x ^ (x >>> 16)) >>> 0;
  }
  function hsh(i, salt) { return (mix((Math.imul(i, 0x9E3779B9) ^ salt) >>> 0) >>> 8) / 16777216; }
  function fmodp(a, m) { var r = a - trunc(a / m) * m; return r < 0 ? r + m : r; }
  function dropCount(d, still) {
    if (d.precip === 1) return still ? (d.len < 3 ? d.rate * 2 : trunc(d.rate * 17 / 10)) : d.rate;
    if (d.precip === 2) return still ? trunc(d.rate * 3 / 2) : d.rate;
    return 0;
  }
  function displace(m, pts, depth, rough) {   // midpoint displacement; pts holds the two endpoints
    for (var d = 0; d < depth; d++) {
      var amp = rough / (1 << d), out = [pts[0]];
      for (var i = 1; i < pts.length; i++) {
        var a = pts[i - 1], c = pts[i], rx = m(), ry = m();
        out.push({ x: (a.x + c.x) / 2 + (rx - 0.5) * 2 * amp, y: (a.y + c.y) / 2 + (ry - 0.5) * amp * 0.5 }, c);
      }
      pts = out;
    }
    return pts;
  }
  function buildBolt(L) {
    var m = mulberry(307), k = L.k, x1 = 0.1 * L.W, br = [];
    var main = displace(m, [{ x: 0.2 * L.W, y: 6 * k }, { x: x1, y: ridgeY(L, x1) - 2 }], 5, 9 * k);
    [0.28, 0.45, 0.62].forEach(function(f, j) {
      var s = main[trunc(main.length * f)], side = j % 2 ? 1 : -1, len = (12 + m() * 12) * k;
      var ex = s.x + side * len * 0.7, ridge = ridgeY(L, ex) - 3;
      br.push(displace(m, [s, { x: ex, y: Math.min(s.y + len, ridge) }], 4, 4 * k));
    });
    return { main: main, br: br };
  }
  function ditherPolyline(p, w, c, p256) {
    var r2 = w * w / 4;
    for (var i = 1; i < p.length; i++) {
      var a = p[i - 1], b = p[i], dx = b.x - a.x, dy = b.y - a.y, len2 = dx * dx + dy * dy, r = w / 2;
      for (var y = floor(Math.min(a.y, b.y) - r); y <= floor(Math.max(a.y, b.y) + r); y++)
        for (var x = floor(Math.min(a.x, b.x) - r); x <= floor(Math.max(a.x, b.x) + r); x++) {
          var px = x + 0.5 - a.x, py = y + 0.5 - a.y, u = len2 > 0 ? clamp((px * dx + py * dy) / len2, 0, 1) : 0;
          var ex = px - u * dx, ey = py - u * dy;
          if (ex * ex + ey * ey < r2 && dith256(x, y, p256)) put(x, y, c);
        }
    }
  }
  function corePolyline(p, n, c) { for (var i = 1; i < n; i++) linef(p[i - 1].x, p[i - 1].y, p[i].x, p[i].y, c); }
  function drawFlake(x, y, big) {
    if (big) {
      put(x + 1, y + 2, LIBERTY);
      put(x - 1, y, 0xAAAAFF); put(x + 1, y, 0xAAAAFF); put(x, y - 1, 0xAAAAFF); put(x, y + 1, 0xAAAAFF);
      put(x, y, WHITE);
    } else {
      put(x + 1, y + 1, LIBERTY);
      put(x, y, 0xAAAAFF);
    }
  }
  function wxDraw(L, w, d, still, wind, ms) {
    var df = WX[w], k = L.k, t = still ? 2.3 : (ms % 600000) / 1000, top = L.compTop - 2, n, i, x, y;
    var c = ms % 5000, flash = !still && c < 60, boltOn = df.lightning && (still || c < 120 || (c > 200 && c < 340));
    if (flash) {
      var rows = floor(ridgeY(L, L.W / 2) + 0.5);
      for (y = 0; y < rows && y < H; y++) for (x = 0; x < W; x++) if (dith256(x, y, 90)) FB[y * W + x] = WHITE;
    }
    if (boltOn) {
      var b = L.bolt || (L.bolt = buildBolt(L));
      ditherPolyline(b.main, 3, 0xAAAAFF, 141);
      b.br.forEach(function(p) { ditherPolyline(p, 2, LIBERTY, 128); });
      b.br.forEach(function(p) { corePolyline(p, trunc((p.length * 4 + 4) / 5), 0xAAFFFF); });
      corePolyline(b.main, b.main.length, WHITE);
    }
    n = dropCount(df, still);
    if (df.precip === 2)
      for (i = 0; i < n; i++) {
        var yy = fmodp(hsh(i, 0xC3) * top + t * df.speed * k * (0.6 + hsh(i, 0xD4) * 0.8), top);
        if (yy > top - 3) continue;
        drawFlake(trunc(fmodp(hsh(i, 0xB2) * W + sin(t * 1.3 + i) * 3 * k + 0.3 * wind * yy, W)), trunc(yy), i % 3 === 0);
      }
    if (df.precip === 1) {
      var span = top + 20, slant = 0.3 * wind, len = df.len * k;
      for (i = 0; i < n; i++) {
        var x0 = hsh(i, 0xA1) * (W + 40) - (wind > 0 ? 40 : wind === 0 ? 20 : 0);
        y = fmodp(hsh(i, 0xB7) * span + t * df.speed * k, span) - 10;
        if (y + len >= top) continue;
        x = x0 + (y + 10) * slant;
        linef(x, y, x + len * slant, y + len, RAIN[d]);
      }
    }
  }

  // ---------- daypart.c ----------
  var TWILIGHT_MIN = 30;   // mirrors src/c/daypart.c
  function nearEvent(m, ev) { var d = ((m - ev) % 1440 + 1440) % 1440; return d < TWILIGHT_MIN || d >= 1440 - TWILIGHT_MIN; }
  function daypartAt(m, sr, ss) {
    if (nearEvent(m, sr)) return 0;
    if (nearEvent(m, ss)) return 2;
    var since = ((m - (sr + TWILIGHT_MIN)) % 1440 + 1440) % 1440, span = ((ss - sr - 2 * TWILIGHT_MIN) % 1440 + 1440) % 1440;
    return since < span ? 1 : 3;   // wrap-aware: sunset may precede sunrise
  }
  // The watch's DAYPART_MODE is dayCycle ? dayMode : 2 (config.js toMessage); 2 is static and shows dayFixed.
  function daypartFor(cfg, env, m) {
    var mode = cfg.dayCycle ? cfg.dayMode : 2;
    if (mode === 2) return cfg.dayFixed >= 0 && cfg.dayFixed <= 3 ? cfg.dayFixed | 0 : 1;
    return mode === 1 ? daypartAt(m, cfg.fixedRise, cfg.fixedSet) : daypartAt(m, env.rise, env.set);
  }
  function weatherFor(cfg, env) {
    if (env.platform === 'basalt') return 0;
    if (cfg.weatherOn) return wxFromWmo(env.wmo);
    return cfg.wxFixed >= 0 && cfg.wxFixed <= 7 ? cfg.wxFixed | 0 : 0;
  }
  function jdn(y, mo, d) {
    var a = trunc((14 - mo) / 12), yy = y + 4800 - a, mm = mo + 12 * a - 3;
    return d + trunc((153 * mm + 2) / 5) + 365 * yy + trunc(yy / 4) - trunc(yy / 100) + trunc(yy / 400) - 32045;
  }
  function moonPhase(now) {   // daypart.c moon_phase, from the local date and time as the watch passes them
    var days = jdn(now.getFullYear(), now.getMonth() + 1, now.getDate()) - jdn(2000, 1, 6) + (now.getHours() * 60 + now.getMinutes() - 1094) / 1440;
    var p = days / 29.530588853;
    p -= trunc(p);
    return p < 0 ? p + 1 : p;
  }

  // ---------- comps.c ----------
  var MO = ['JAN', 'FEB', 'MAR', 'APR', 'MAY', 'JUN', 'JUL', 'AUG', 'SEP', 'OCT', 'NOV', 'DEC'];
  function compText(type, now, saver) {   // sample values for the slots the phone cannot know
    switch (type) {
      case 0: return saver ? '-' : ':38';
      case 1: return '61\u00B0';
      case 2: return MO[now.getMonth()] + ' ' + now.getDate();
      case 3: return '8412';
      case 4: return '72';
      case 5: return '80%';
      default: return '';
    }
  }
  function hex(c) { return '#' + ('00000' + c.toString(16)).slice(-6); }
  function drawComps(g, L, th, cfg, now, saver) {
    var big = L.W >= 200, k4 = trunc(4 * L.k + 0.5);
    g.fillStyle = hex(th.text);
    g.font = 'bold ' + (big ? 19 : 15) + 'px sans-serif';   // about the cap height of Gothic 24 / 18 bold
    g.textBaseline = 'middle';
    var y = L.compTop + L.compH / 2 - 1, xs = [k4, L.W / 2, L.W - k4], al = ['left', 'center', 'right'];
    [cfg.comp1, cfg.comp2, cfg.comp3].forEach(function(type, i) {
      var s = compText(type, now, saver);
      if (!s) return;
      g.textAlign = al[i];
      g.fillText(s, xs[i], y);
    });
  }

  // ---------- frame ----------
  var cache = {};
  function cached(key, make) {
    if (!(key in cache)) {
      if (Object.keys(cache).length > 24) cache = {};
      cache[key] = make();
    }
    return cache[key];
  }
  function blit(g) {   // the framebuffer as one ImageData; the page scales the canvas with CSS
    var img = g.createImageData(W, H), px = img.data;
    for (var i = 0, j = 0; i < W * H; i++, j += 4) {
      var c = FB[i];
      px[j] = c >> 16 & 255; px[j + 1] = c >> 8 & 255; px[j + 2] = c & 255; px[j + 3] = 255;
    }
    g.putImageData(img, 0, 0);
  }

  function draw(canvas, cfg, env, opts) {
    opts = opts || {};
    var emery = env.platform !== 'basalt', w = emery ? 200 : 144, h = emery ? 228 : 168;
    if (canvas.width !== w) canvas.width = w;
    if (canvas.height !== h) canvas.height = h;
    W = w; H = h;
    var now = opts.now || new Date(), hr = now.getHours(), mn = now.getMinutes();
    var L = cached('L' + w, function() { return makeLayout(w, h); });
    var d = daypartFor(cfg, env, hr * 60 + mn), wx = weatherFor(cfg, env), th = THEMES[d];
    var moon = d === 3 ? moonPhase(now) : 0, saver = cfg.animMode === 2, still = cfg.animMode !== 0;
    var t = opts.animMs != null && !saver ? clamp(fround(opts.animMs), 0, 65534) : SKI_REST_MS;
    var anim = t !== SKI_REST_MS, fmt = (cfg.use24 ? 1 : 0) + (cfg.lead0 ? 2 : 0);
    var neu = cached('S' + w + ':' + fmt + ':' + hr + ':' + mn, function() { return buildSet(L, cfg, hr, mn); });
    var old = null;
    if (anim) {
      var prev = new Date(now.getTime() - 60000), ph = prev.getHours(), pm = prev.getMinutes();
      old = cached('S' + w + ':' + fmt + ':' + ph + ':' + pm, function() { return buildSet(L, cfg, ph, pm); });
    }
    var bg = cached('B' + w + ':' + d + ':' + wx + ':' + moon.toFixed(3), function() {
      FB = new Int32Array(w * h);
      buildBg(L, d, wx, moon, emery);
      return FB;
    });
    FB = new Int32Array(bg);
    var smokeMs = anim ? t : 0, solid = cfg.tracks === 1, i, s, st;

    if (old && t < AV_FLOW_END_MS + AV_FADE_MS) old.sk.forEach(function(s) { drawTrack(s, s.sIdx, s.n - 1, solid); });
    if (old) avalDraw(L, th, bg, t);
    var states = neu.sk.map(function(s) { return skierState(s, t); });
    for (i = 0; i < neu.sk.length; i++) { s = neu.sk[i]; st = states[i]; if (st && st.idx > s.sIdx) drawTrack(s, s.sIdx, st.idx, solid); }
    drawChalet(L, th);
    if (emery && th.night) drawCampfire(L, smokeMs);
    drawSmoke(L, th, smokeMs);
    if (old && t < 1500)
      for (i = 0; i < old.sk.length; i++) {
        st = skierExitState(old.sk[i], i, t, L.k);
        if (st) drawSkier(st.x, st.y, st.h, old.sk[i].color, L.k);
      }
    for (i = 0; i < neu.sk.length; i++) {
      st = states[i];
      if (!st) continue;
      drawSkier(st.x, st.y, st.h, neu.sk[i].color, L.k);
      if (st.spray) drawSpray(th, st.x, st.y, st.h, st.spray, L.k);
    }
    if (emery) {
      slopeTrees(th, 1);
      wxDraw(L, wx, d, still, env.wind | 0, still ? smokeMs : fround((opts.wxClock || 0) * 1000));
    }
    var g = canvas.getContext('2d');
    blit(g);
    drawComps(g, L, th, cfg, now, saver);   // the strip text: nothing drawn after it on the watch reaches the strip
    return { durationMs: skiersDuration(neu) };
  }

  return { draw: draw, daypartFor: daypartFor, weatherFor: weatherFor, fmtTime: fmtTime };
}

var PREVIEW_JS = 'var SkiPreview=(' + skiPreviewFactory.toString() + ')();';

module.exports = { PREVIEW_JS: PREVIEW_JS };
