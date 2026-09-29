// Phone-side JS companion for Ski Time watch face.
// Fetches the current outdoor temperature (°F) from Open-Meteo and sends it
// to the watch via AppMessage on app launch, then every 30 minutes.

var config = require('./config');

var REFRESH_MS = 30 * 60 * 1000;   // 30 minutes
var REQUEST_TIMEOUT_MS = 15000;

function logErr(prefix, e) {
  console.log('[ski-time] ' + prefix + ': ' + (e && e.message ? e.message : e));
}

function sendTemp(t) {
  Pebble.sendAppMessage(
    { TEMP_F: t },
    function() { console.log('[ski-time] sent TEMP_F=' + t); },
    function(e) { logErr('sendAppMessage failed', e); }
  );
}

function fetchWeather() {
  navigator.geolocation.getCurrentPosition(
    function(pos) {
      var lat = pos.coords.latitude.toFixed(4);
      var lon = pos.coords.longitude.toFixed(4);
      var url = 'https://api.open-meteo.com/v1/forecast' +
                '?latitude=' + lat +
                '&longitude=' + lon +
                '&current=temperature_2m' +
                '&temperature_unit=fahrenheit';
      var req = new XMLHttpRequest();
      req.open('GET', url, true);
      req.timeout = REQUEST_TIMEOUT_MS;
      req.onload = function() {
        if (req.status >= 200 && req.status < 300) {
          try {
            var data = JSON.parse(req.responseText);
            var t = Math.round(data.current.temperature_2m);
            sendTemp(t);
          } catch (e) { logErr('parse', e); }
        } else {
          logErr('http', req.status);
        }
      };
      req.onerror = function() { logErr('network', null); };
      req.ontimeout = function() { logErr('timeout', null); };
      req.send();
    },
    function(err) { logErr('geo', err); },
    { timeout: REQUEST_TIMEOUT_MS, maximumAge: 10 * 60 * 1000 }
  );
}

Pebble.addEventListener('ready', function() {
  console.log('[ski-time] pkjs ready');
  fetchWeather();
  setInterval(fetchWeather, REFRESH_MS);
});

// ---- Settings page ----

function loadCfg() {
  var cfg = {}, k;
  for (k in config.DEFAULTS) cfg[k] = config.DEFAULTS[k];
  try {
    var saved = JSON.parse(localStorage.getItem('cfg') || '{}');
    for (k in saved) if (saved[k] !== undefined) cfg[k] = saved[k];
  } catch (e) { logErr('loadCfg', e); }
  return cfg;
}

Pebble.addEventListener('showConfiguration', function() {
  Pebble.openURL('data:text/html;charset=utf-8,' +
                 encodeURIComponent(config.buildHtml(loadCfg())));
});

Pebble.addEventListener('webviewclosed', function(e) {
  if (!e || !e.response) return;                       // dismissed without saving
  var cfg;
  try { cfg = JSON.parse(decodeURIComponent(e.response)); }
  catch (err) { logErr('parse config', err); return; }
  localStorage.setItem('cfg', JSON.stringify(cfg));
  Pebble.sendAppMessage(config.toMessage(cfg),
    function() { console.log('[ski-time] settings sent'); },
    function(err) { logErr('settings send', err); });
});
