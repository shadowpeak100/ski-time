// Custom HTML settings page for Ski Time (Clay-free).
// Served as a data: URI via Pebble.openURL; returns its values as JSON through
// the pebblejs://close redirect, parsed by index.js on 'webviewclosed'.

var DEFAULTS = {
  comp1: 1, comp2: 2, comp3: 0,          // weather, date, seconds
  use24: true,
  lead0: true,
  tracks: 0,                             // 0 = twin grooves, 1 = solid
  saverOn: false, saverStart: 1320, saverEnd: 420,  // battery saver 22:00-07:00
  dayMode: 0, fixedRise: 420, fixedSet: 1140,       // 0 = follow sun, 1 = fixed, 2 = always midday
  animMode: 1,                           // 0 = full, 1 = balanced, 2 = battery saver
  weatherOn: true
};

function toMessage(cfg) {
  return {
    COMP_1: cfg.comp1, COMP_2: cfg.comp2, COMP_3: cfg.comp3,
    TIME_24H: cfg.use24 ? 1 : 0,
    LEAD_ZERO: cfg.lead0 ? 1 : 0,
    TRACK_STYLE: cfg.tracks,
    SAVER_ON: cfg.saverOn ? 1 : 0, SAVER_START: cfg.saverStart, SAVER_END: cfg.saverEnd,
    DAYPART_MODE: cfg.dayMode, SUN_FIXED_RISE: cfg.fixedRise, SUN_FIXED_SET: cfg.fixedSet,
    ANIM_MODE: cfg.animMode, WEATHER_ON: cfg.weatherOn ? 1 : 0
  };
}

function selRow(label, id, opts) {
  var o = '';
  for (var i = 0; i < opts.length; i++) o += '<option value="' + i + '">' + opts[i] + '</option>';
  return '<div class="row"><label>' + label + '</label><select id="' + id + '">' + o + '</select></div>';
}
function compRow(label, id) {
  return selRow(label, id, ['Seconds', 'Weather', 'Date', 'Steps', 'Heart rate', 'Battery']);
}
function toggleRow(label, id) {
  return '<div class="row"><label>' + label + '</label>' +
    '<div class="sw-toggle"><button id="' + id + '_on">On</button><button id="' + id + '_off">Off</button></div></div>';
}

var PAGE_JS = [
"function bindSel(id){var e=document.getElementById(id);e.value=CFG[id];e.onchange=function(){CFG[id]=+e.value;};}",
"function bindToggle(id){var on=document.getElementById(id+'_on'),off=document.getElementById(id+'_off');" +
"function set(v){CFG[id]=v;on.setAttribute('data-on',v?1:0);off.setAttribute('data-on',v?0:1);}" +
"on.onclick=function(){set(true);};off.onclick=function(){set(false);};set(!!CFG[id]);}",
"function m2t(m){var h=Math.floor(m/60),mm=m%60;return ('0'+h).slice(-2)+':'+('0'+mm).slice(-2);}",
"function t2m(s){var p=(s||'0:0').split(':');return (+p[0])*60+(+p[1]);}",
"bindSel('comp1');bindSel('comp2');bindSel('comp3');bindSel('tracks');bindSel('dayMode');bindSel('animMode');",
"bindToggle('use24');bindToggle('lead0');bindToggle('saverOn');bindToggle('weatherOn');",
"(function(){var a=document.getElementById('svStart'),b=document.getElementById('svEnd');a.value=m2t(CFG.saverStart);b.value=m2t(CFG.saverEnd);a.onchange=function(){CFG.saverStart=t2m(a.value);};b.onchange=function(){CFG.saverEnd=t2m(b.value);};})();",
"(function(){var mode=document.getElementById('dayMode'),a=document.getElementById('fxRise'),b=document.getElementById('fxSet');" +
"a.value=m2t(CFG.fixedRise);b.value=m2t(CFG.fixedSet);" +
"function sync(){a.disabled=b.disabled=(CFG.dayMode!==1);}" +
"mode.onchange=function(){CFG.dayMode=+mode.value;sync();};" +
"a.onchange=function(){CFG.fixedRise=t2m(a.value);};b.onchange=function(){CFG.fixedSet=t2m(b.value);};sync();})();",
"document.getElementById('save').onclick=function(){location.href='pebblejs://close#'+encodeURIComponent(JSON.stringify(CFG));};",
"document.getElementById('cancel').onclick=function(){location.href='pebblejs://close#';};"
].join('\n');

function buildHtml(cfg) {
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
'</style></head><body><div class="wrap"><h1>Ski Time</h1>' +
'<section><h2>Display</h2>' +
selRow('Animations', 'animMode', ['Full animations (may use more battery)', 'Balanced', 'Battery saver']) +
toggleRow('Show weather', 'weatherOn') + '</section>' +
'<section><h2>Complications</h2>' +
compRow('Left slot', 'comp1') + compRow('Center slot', 'comp2') + compRow('Right slot', 'comp3') + '</section>' +
'<section><h2>Clock</h2>' + toggleRow('24-hour', 'use24') + toggleRow('Leading zero (09:05)', 'lead0') + '</section>' +
'<section><h2>Ski tracks</h2>' + selRow('Track style', 'tracks', ['Twin grooves', 'Solid']) + '</section>' +
'<section><h2>Time of day</h2>' +
selRow('Sky follows', 'dayMode', ['Follow sunrise and sunset', 'Fixed times', 'Always midday']) +
'<div class="row"><label>Sunrise</label><input type="time" id="fxRise"></div>' +
'<div class="row"><label>Sunset</label><input type="time" id="fxSet"></div></section>' +
'<section><h2>Battery saver</h2>' +
'<p style="color:var(--muted);font-size:12px;margin:0 0 4px">During this window seconds show a dash and the minute change is instant.</p>' +
toggleRow('Use Battery saver on a schedule', 'saverOn') +
'<div class="row"><label>From</label><input type="time" id="svStart"></div>' +
'<div class="row"><label>To</label><input type="time" id="svEnd"></div></section>' +
'</div><div class="bar"><button id="cancel">Cancel</button><button class="save" id="save">Save</button></div>' +
'<script>var CFG=' + JSON.stringify(cfg) + ';' + PAGE_JS + '</script></body></html>';
}

module.exports = { DEFAULTS: DEFAULTS, toMessage: toMessage, buildHtml: buildHtml };
