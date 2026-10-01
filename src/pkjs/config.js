// Custom HTML settings page for Ski Time (Clay-free).
// Served as a data: URI via Pebble.openURL; returns its values as JSON through
// the pebblejs://close redirect, parsed by index.js on 'webviewclosed'.

var PREVIEW_JS = require('./preview').PREVIEW_JS;

var DEFAULTS = {
  comp1: 1, comp2: 2, comp3: 0,          // weather, date, seconds
  use24: true,
  lead0: true,
  tracks: 0,                             // 0 = twin grooves, 1 = solid
  saverOn: false, saverStart: 1320, saverEnd: 420,  // battery saver 22:00-07:00
  dayCycle: true, dayMode: 0, fixedRise: 420, fixedSet: 1140,  // dayMode: 0 = follow sun, 1 = fixed times
  dayFixed: 1,                           // Daypart shown when the cycle is off: 1 = midday
  animMode: 1,                           // 0 = full, 1 = balanced, 2 = battery saver
  weatherOn: true,
  wxFixed: 0                             // WxKind shown when weather is off: 0 = clear
};

function toMessage(cfg) {
  return {
    COMP_1: cfg.comp1, COMP_2: cfg.comp2, COMP_3: cfg.comp3,
    TIME_24H: cfg.use24 ? 1 : 0,
    LEAD_ZERO: cfg.lead0 ? 1 : 0,
    TRACK_STYLE: cfg.tracks,
    SAVER_ON: cfg.saverOn ? 1 : 0, SAVER_START: cfg.saverStart, SAVER_END: cfg.saverEnd,
    DAYPART_MODE: cfg.dayCycle ? cfg.dayMode : 2, DAYPART_FIXED: cfg.dayFixed,
    SUN_FIXED_RISE: cfg.fixedRise, SUN_FIXED_SET: cfg.fixedSet,
    ANIM_MODE: cfg.animMode, WEATHER_ON: cfg.weatherOn ? 1 : 0, WX_FIXED: cfg.wxFixed
  };
}

function selRow(label, id, opts, rowId) {
  var o = '';
  for (var i = 0; i < opts.length; i++) o += '<option value="' + i + '">' + opts[i] + '</option>';
  return '<div class="row"' + (rowId ? ' id="' + rowId + '"' : '') + '><label>' + label + '</label><select id="' + id + '">' + o + '</select></div>';
}
function compRow(label, id) {
  return selRow(label, id, ['Seconds', 'Weather', 'Date', 'Steps', 'Heart rate', 'Battery']);
}
function toggleRow(label, id) {
  return '<div class="row"><label>' + label + '</label>' +
    '<div class="sw-toggle"><button id="' + id + '_on">On</button><button id="' + id + '_off">Off</button></div></div>';
}

var PAGE_JS = [
"if(CFG.dayMode===2){CFG.dayCycle=false;CFG.dayMode=0;CFG.dayFixed=1;}if(CFG.dayCycle===undefined)CFG.dayCycle=true;",
"var PV=document.getElementById('pv'),animT0=null,rafId=null,runPending=false;" +
"function pvSize(){var w=ENV.platform==='basalt'?144:200,h=ENV.platform==='basalt'?168:228;PV.style.width=(2*w)+'px';PV.style.height=(2*h)+'px';}" +
"function frame(ts){rafId=null;if(runPending){runPending=false;animT0=ts;}var animMs=animT0===null?null:ts-animT0;" +
"var r=SkiPreview.draw(PV,CFG,ENV,{now:new Date(),animMs:animMs,wxClock:ts/1000});" +
"if(animT0!==null&&animMs>=r.durationMs)animT0=null;" +
"var moving=CFG.animMode===0&&ENV.platform!=='basalt'&&SkiPreview.weatherFor(CFG,ENV)>=4;" +
"if(animT0!==null||moving)rafId=requestAnimationFrame(frame);}" +
"function redraw(){if(rafId===null)rafId=requestAnimationFrame(frame);}" +
"pvSize();SkiPreview.draw(PV,CFG,ENV,{now:new Date(),animMs:null,wxClock:0});" +
"document.getElementById('run').onclick=function(){runPending=true;redraw();};",
"function show(id,v){document.getElementById(id).style.display=v?'':'none';}",
"function syncRows(){show('wxFixedRow',!CFG.weatherOn);show('dayModeRow',CFG.dayCycle);" +
"show('fxRiseRow',CFG.dayCycle&&CFG.dayMode===1);show('fxSetRow',CFG.dayCycle&&CFG.dayMode===1);show('dayFixedRow',!CFG.dayCycle);}",
"function bindSel(id){var e=document.getElementById(id);e.value=CFG[id];e.onchange=function(){CFG[id]=+e.value;syncRows();redraw();};}",
"function bindToggle(id){var on=document.getElementById(id+'_on'),off=document.getElementById(id+'_off');" +
"function set(v){CFG[id]=v;syncRows();redraw();on.setAttribute('data-on',v?1:0);off.setAttribute('data-on',v?0:1);}" +
"on.onclick=function(){set(true);};off.onclick=function(){set(false);};set(!!CFG[id]);}",
"function m2t(m){var h=Math.floor(m/60),mm=m%60;return ('0'+h).slice(-2)+':'+('0'+mm).slice(-2);}",
"function t2m(s){var p=(s||'0:0').split(':');return (+p[0])*60+(+p[1]);}",
"bindSel('comp1');bindSel('comp2');bindSel('comp3');bindSel('tracks');bindSel('dayMode');bindSel('animMode');bindSel('wxFixed');bindSel('dayFixed');",
"bindToggle('use24');bindToggle('lead0');bindToggle('saverOn');bindToggle('weatherOn');bindToggle('dayCycle');",
"(function(){var a=document.getElementById('svStart'),b=document.getElementById('svEnd');a.value=m2t(CFG.saverStart);b.value=m2t(CFG.saverEnd);a.onchange=function(){CFG.saverStart=t2m(a.value);redraw();};b.onchange=function(){CFG.saverEnd=t2m(b.value);redraw();};})();",
"(function(){var a=document.getElementById('fxRise'),b=document.getElementById('fxSet');" +
"a.value=m2t(CFG.fixedRise);b.value=m2t(CFG.fixedSet);" +
"a.onchange=function(){CFG.fixedRise=t2m(a.value);redraw();};b.onchange=function(){CFG.fixedSet=t2m(b.value);redraw();};})();",
"syncRows();",
"document.getElementById('save').onclick=function(){location.href='pebblejs://close#'+encodeURIComponent(JSON.stringify(CFG));};",
"document.getElementById('cancel').onclick=function(){location.href='pebblejs://close#';};"
].join('\n');

function buildHtml(cfg, env) {
  return '<!DOCTYPE html><html><head><meta charset="utf-8">' +
'<meta name="viewport" content="width=device-width,initial-scale=1,maximum-scale=1,user-scalable=no">' +
'<title>Ski Time</title><style>' +
':root{--bg:#0e1622;--panel:#16202f;--panel2:#1e2a3d;--hair:#26344a;--hair2:#34465f;--ink:#e8eef8;--muted:#8a9ab3;--accent:#55aaff;}' +
'*{box-sizing:border-box;}html,body{margin:0;background:var(--bg);color:var(--ink);' +
'font-family:-apple-system,BlinkMacSystemFont,"Segoe UI",Roboto,Helvetica,Arial,sans-serif;}' +
'.wrap{max-width:560px;margin:0 auto;padding:16px 16px 120px;}' +
'h1{font-size:20px;margin:6px 0 14px;}' +
'section{background:var(--panel);border:1px solid var(--hair);border-radius:14px;padding:2px 16px 8px;margin-bottom:14px;}' +
'h2{font-size:12px;letter-spacing:.14em;text-transform:uppercase;color:var(--muted);' +
'border-bottom:1px solid var(--hair);padding:14px 0 10px;margin:0 0 8px;}' +
'.row{display:flex;align-items:center;justify-content:space-between;gap:12px;margin:12px 0;}' +
'.row label{font-size:14px;}select{font-size:14px;color:var(--ink);background:var(--panel2);' +
'border:1px solid var(--hair2);border-radius:8px;padding:8px 10px;min-width:150px;}' +
'input[type=time]{font-size:15px;color:var(--ink);background:var(--panel2);border:1px solid var(--hair2);border-radius:8px;padding:8px 10px;}' +
'.sw-toggle{display:flex;gap:8px;}.sw-toggle button{min-width:64px;padding:9px;border-radius:8px;border:1px solid var(--hair2);' +
'background:var(--panel2);color:var(--muted);font-size:14px;cursor:pointer;}.sw-toggle button[data-on="1"]{border-color:var(--accent);color:var(--ink);}' +
'.bar{position:fixed;left:0;right:0;bottom:0;display:flex;gap:10px;padding:12px 16px;background:rgba(14,22,34,.95);border-top:1px solid var(--hair);}' +
'.bar button{flex:1;padding:13px;border-radius:10px;font-size:15px;font-weight:600;border:1px solid var(--hair2);background:var(--panel2);color:var(--ink);}' +
'.bar button.save{background:var(--accent);color:#04121f;border-color:var(--accent);}' +
'.hdr{display:flex;align-items:center;justify-content:space-between;margin:6px 0 14px;}.hdr h1{margin:0;}' +
'.kofi{display:flex;align-items:center;gap:5px;background:#ff5e5b;color:#fff;text-decoration:none;' +
'padding:6px 10px;border-radius:14px;font-size:13px;font-weight:bold;white-space:nowrap;}' +
'.kofibtn{display:block;text-align:center;padding:12px;border-radius:10px;background:#ff5e5b;color:#fff;' +
'text-decoration:none;font-weight:600;font-size:15px;}' +
'.preview{position:sticky;top:0;z-index:5;background:var(--bg);padding:8px 0 10px;display:flex;flex-direction:column;align-items:center;gap:8px;}' +
'#pv{image-rendering:pixelated;image-rendering:crisp-edges;border-radius:12px;background:#000;}' +
'.runbtn{padding:8px 14px;border-radius:8px;border:1px solid var(--hair2);background:var(--panel2);color:var(--ink);font-size:14px;}' +
'</style></head><body><div class="wrap"><div class="hdr"><h1>Ski Time</h1>' +
'<a class="kofi" href="https://ko-fi.com/shadowpeak100" target="_blank" rel="noopener" title="Support Ski Time on Ko-fi">' +
'<svg width="17" height="17" viewBox="0 0 24 24" fill="none" stroke="#fff" stroke-width="2" stroke-linecap="round" stroke-linejoin="round">' +
'<path d="M4 8h11v6a4 4 0 0 1-4 4H8a4 4 0 0 1-4-4V8z"/><path d="M15 9h2.5a2.5 2.5 0 0 1 0 5H15"/><path d="M5 21h11"/></svg>Ko-fi</a></div>' +
'<div class="preview"><canvas id="pv"></canvas><button class="runbtn" id="run">&#9654; Run animation</button></div>' +
'<section><h2>Display</h2>' +
selRow('Animations', 'animMode', ['Full animations (may use more battery)', 'Balanced', 'Battery saver']) +
toggleRow('Weather', 'weatherOn') +
selRow('Show', 'wxFixed', ['Clear', 'Partly cloudy', 'Overcast', 'Fog', 'Drizzle', 'Rain', 'Snow', 'Thunderstorm'], 'wxFixedRow') + '</section>' +
'<section><h2>Complications</h2>' +
compRow('Left slot', 'comp1') + compRow('Center slot', 'comp2') + compRow('Right slot', 'comp3') + '</section>' +
'<section><h2>Clock</h2>' + toggleRow('24-hour', 'use24') + toggleRow('Leading zero (09:05)', 'lead0') + '</section>' +
'<section><h2>Ski tracks</h2>' + selRow('Track style', 'tracks', ['Twin grooves', 'Solid']) + '</section>' +
'<section><h2>Time of day</h2>' +
toggleRow('Daylight cycle', 'dayCycle') +
selRow('Cycle follows', 'dayMode', ['Sunrise and sunset', 'Fixed times'], 'dayModeRow') +
'<div class="row" id="fxRiseRow"><label>Sunrise</label><input type="time" id="fxRise"></div>' +
'<div class="row" id="fxSetRow"><label>Sunset</label><input type="time" id="fxSet"></div>' +
selRow('Show', 'dayFixed', ['Sunrise', 'Midday', 'Sunset', 'Night'], 'dayFixedRow') + '</section>' +
'<section><h2>Battery saver</h2>' +
'<p style="color:var(--muted);font-size:12px;margin:0 0 4px">During this window seconds show a dash and the minute change is instant.</p>' +
toggleRow('Use Battery saver on a schedule', 'saverOn') +
'<div class="row"><label>From</label><input type="time" id="svStart"></div>' +
'<div class="row"><label>To</label><input type="time" id="svEnd"></div></section>' +
'<section><h2>Support</h2><p style="color:var(--muted);font-size:13px;margin:0 0 10px">' +
'Ski Time is free, with no ads. If it\'s useful to you, you can support development. Thank you!</p>' +
'<a class="kofibtn" href="https://ko-fi.com/shadowpeak100" target="_blank" rel="noopener">&#9749; Buy me a coffee on Ko-fi</a></section>' +
'</div><div class="bar"><button id="cancel">Cancel</button><button class="save" id="save">Save</button></div>' +
'<script>var CFG=' + JSON.stringify(cfg) + ';var ENV=' + JSON.stringify(env || envFrom({}, 'emery')) + ';' + PREVIEW_JS + '\n' + PAGE_JS + '</script></body></html>';
}

// Preview inputs from the phone's cache; junk or missing values fall back to clear, calm, 07:00-19:00.
function envFrom(stored, platform) {
  function int(v, lo, hi, dflt) { return typeof v === 'number' && v % 1 === 0 && v >= lo && v <= hi ? v : dflt; }
  stored = stored || {};
  return {
    platform: platform === 'basalt' ? 'basalt' : 'emery',
    wmo: int(stored.wx, 0, 99, 0),
    wind: int(stored.wind, -1, 1, 0),
    rise: int(stored.rise, 0, 1439, 420),
    set: int(stored.set, 0, 1439, 1140)
  };
}

module.exports = { DEFAULTS: DEFAULTS, toMessage: toMessage, buildHtml: buildHtml, envFrom: envFrom };
