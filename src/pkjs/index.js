var configPage = require('./config-page');

// --- Message keys (must match src/c/main.c)
var KEY_TRIGGER = 0;
var KEY_UPDATE = 1;          // start of a settings sync
var KEY_STATUS = 2;          // result of a webhook call
var KEY_COUNT = 3;           // number of webhooks in this sync
var KEY_ITEM_START = 4;      // index of the first webhook in an item batch
var KEY_AUTO_CLOSE = 30;
var KEY_HEADER_COLOR = 31;
var KEY_HIGHLIGHT_COLOR = 32;
var KEY_DARK_LIST = 33;
var KEY_TOUCH = 34;
var KEY_SOUND_FEEDBACK = 40;
var KEY_NAME_BASE = 100;     // + slot inside a batch
var KEY_DESC_BASE = 200;     // + slot inside a batch
var KEY_COLOR_BASE = 300;    // + slot inside a batch (RGB int, -1 = none)
var KEY_FLAGS_BASE = 400;    // + slot inside a batch (bit 0 = ask before running)

var BATCH_SIZE = 6;          // must match ITEM_BATCH_SIZE in main.c
var MAX_NAME_BYTES = 31;     // watch buffer: 32
var MAX_DESC_BYTES = 39;     // watch buffer: 40

var DEFAULT_HEADER_COLOR = '#0055AA';
var DEFAULT_HIGHLIGHT_COLOR = '#00FFFF';
var METHODS = ['GET', 'POST', 'PUT', 'PATCH', 'DELETE'];

var STORAGE_KEY = 'hooky-config';
var LEGACY_KEY = 'clay-settings';   // settings of the old Clay based version (5 fixed webhooks)

// --- Utility
function asString(v) {
  if (typeof v === 'string') return v;
  if (typeof v === 'number') return String(v);
  if (typeof v === 'boolean') return v ? 'true' : 'false';
  return '';
}
function asBool(v) {
  if (typeof v === 'boolean') return v;
  if (typeof v === 'number') return !!v;
  if (typeof v === 'string') return (v === 'true' || v === '1');
  return false;
}
function asColor(v, fallback) {
  var s = asString(v).trim().toUpperCase();
  return /^#[0-9A-F]{6}$/.test(s) ? s : fallback;
}
function colorToInt(hex) {
  return parseInt(hex.substr(1), 16);
}

// Cut a string so that its UTF-8 encoding fits into maxBytes.
function truncateUtf8(str, maxBytes) {
  str = asString(str);
  var out = '';
  var bytes = 0;
  for (var i = 0; i < str.length; i++) {
    var c = str.charCodeAt(i);
    var ch = str.charAt(i);
    var b;
    if (c < 0x80) b = 1;
    else if (c < 0x800) b = 2;
    else if (c >= 0xD800 && c <= 0xDBFF) { ch = str.substr(i, 2); i++; b = 4; }
    else b = 3;
    if (bytes + b > maxBytes) break;
    out += ch;
    bytes += b;
  }
  return out;
}

// --- Config model
// {
//   version: 3, autoClose, soundFeedback, touchEnabled, darkList, headerColor, highlightColor,
//   webhooks: [{ id, name, desc, showDesc, url, method, body, headers: [{name, value}],
//                useAuth, clientId, clientSecret, color, confirm }]
// }
// Older configs (version 2) are upgraded automatically by filling in defaults.
function sanitizeConfig(raw) {
  var cfg = {
    version: 3,
    autoClose: false,
    soundFeedback: false,
    touchEnabled: true,
    darkList: false,
    headerColor: DEFAULT_HEADER_COLOR,
    highlightColor: DEFAULT_HIGHLIGHT_COLOR,
    webhooks: []
  };
  if (!raw || typeof raw !== 'object') return cfg;
  cfg.autoClose = asBool(raw.autoClose);
  cfg.soundFeedback = asBool(raw.soundFeedback);
  cfg.touchEnabled = raw.touchEnabled === undefined ? true : asBool(raw.touchEnabled);
  cfg.darkList = asBool(raw.darkList);
  cfg.headerColor = asColor(raw.headerColor, DEFAULT_HEADER_COLOR);
  cfg.highlightColor = asColor(raw.highlightColor, DEFAULT_HIGHLIGHT_COLOR);

  var list = Array.isArray(raw.webhooks) ? raw.webhooks : [];
  var seen = {};
  list.forEach(function (w, i) {
    if (!w || typeof w !== 'object') return;
    var id = asString(w.id) || ('w' + i);
    while (seen[id]) id += '_';
    seen[id] = true;

    var method = asString(w.method).toUpperCase();
    var headers = [];
    if (Array.isArray(w.headers)) {
      w.headers.forEach(function (h) {
        if (!h || typeof h !== 'object') return;
        var name = asString(h.name).trim();
        if (name) headers.push({ name: name, value: asString(h.value) });
      });
    }

    cfg.webhooks.push({
      id: id,
      name: asString(w.name),
      desc: asString(w.desc),
      showDesc: w.showDesc === undefined ? true : asBool(w.showDesc),
      url: asString(w.url).trim(),
      method: METHODS.indexOf(method) === -1 ? 'POST' : method,
      body: asString(w.body),
      headers: headers,
      useAuth: asBool(w.useAuth),
      clientId: asString(w.clientId).trim(),
      clientSecret: asString(w.clientSecret).trim(),
      color: asColor(w.color, ''),
      confirm: asBool(w.confirm)
    });
  });
  return cfg;
}

// Convert settings of the old Clay version (enabled1..5, name1..5, webhook1..5, ...).
function migrateLegacy() {
  var cfg = sanitizeConfig(null);
  var old = {};
  try { old = JSON.parse(localStorage.getItem(LEGACY_KEY) || '{}') || {}; } catch (err) { old = {}; }

  cfg.autoClose = asBool(old.KEY_AUTO_CLOSE);
  cfg.soundFeedback = asBool(old.KEY_SOUND_FEEDBACK);

  var raw = { webhooks: [] };
  for (var i = 1; i <= 5; i++) {
    if (!asBool(old['enabled' + i])) continue;   // disabled slots were never shown on the watch
    var clientId = asString(old['clientid' + i]).trim();
    var clientSecret = asString(old['clientsecret' + i]).trim();
    raw.webhooks.push({
      id: 'legacy' + i,
      name: asString(old['name' + i]) || ('Webhook ' + i),
      desc: 'Trigger Webhook ' + i,
      showDesc: true,
      url: asString(old['webhook' + i]).trim(),
      useAuth: !!(clientId || clientSecret),
      clientId: clientId,
      clientSecret: clientSecret
    });
  }
  cfg.webhooks = sanitizeConfig(raw).webhooks;
  return cfg;
}

function loadConfig() {
  try {
    var stored = localStorage.getItem(STORAGE_KEY);
    if (stored) return sanitizeConfig(JSON.parse(stored));
  } catch (err) {
    console.log('Error reading config:', err);
  }
  var migrated = migrateLegacy();
  saveConfig(migrated);
  return migrated;
}

function saveConfig(cfg) {
  try {
    localStorage.setItem(STORAGE_KEY, JSON.stringify(cfg));
  } catch (err) {
    console.log('Error saving config:', err);
  }
}

// --- Sync to the watch
// 1st message: KEY_UPDATE + KEY_COUNT + options + theme
// then:        batches of up to BATCH_SIZE webhooks (name, description, color, flags)
var syncGeneration = 0;

function buildSyncMessages(cfg) {
  var messages = [];
  var head = {};
  head[KEY_UPDATE] = 1;
  head[KEY_COUNT] = cfg.webhooks.length;
  head[KEY_AUTO_CLOSE] = cfg.autoClose ? 1 : 0;
  head[KEY_SOUND_FEEDBACK] = cfg.soundFeedback ? 1 : 0;
  head[KEY_HEADER_COLOR] = colorToInt(cfg.headerColor);
  head[KEY_HIGHLIGHT_COLOR] = colorToInt(cfg.highlightColor);
  head[KEY_DARK_LIST] = cfg.darkList ? 1 : 0;
  head[KEY_TOUCH] = cfg.touchEnabled ? 1 : 0;
  messages.push(head);

  for (var start = 0; start < cfg.webhooks.length; start += BATCH_SIZE) {
    var msg = {};
    msg[KEY_ITEM_START] = start;
    for (var slot = 0; slot < BATCH_SIZE && (start + slot) < cfg.webhooks.length; slot++) {
      var w = cfg.webhooks[start + slot];
      msg[KEY_NAME_BASE + slot] = truncateUtf8(w.name || ('Webhook ' + (start + slot + 1)), MAX_NAME_BYTES);
      msg[KEY_DESC_BASE + slot] = (w.showDesc && w.desc) ? truncateUtf8(w.desc, MAX_DESC_BYTES) : '';
      msg[KEY_COLOR_BASE + slot] = w.color ? colorToInt(w.color) : -1;
      msg[KEY_FLAGS_BASE + slot] = w.confirm ? 1 : 0;
    }
    messages.push(msg);
  }
  return messages;
}

function sendSequentially(messages, generation) {
  var i = 0;
  var attempts = 0;
  function next() {
    if (generation !== syncGeneration) return;   // a newer sync replaced this one
    if (i >= messages.length) return;
    Pebble.sendAppMessage(messages[i],
      function () { i++; attempts = 0; next(); },
      function (e) {
        attempts++;
        console.log('Sync message ' + i + ' failed (attempt ' + attempts + '):', JSON.stringify(e));
        if (attempts < 4) setTimeout(next, 300 * attempts);
      }
    );
  }
  next();
}

function sendSettingsToWatch(cfg) {
  syncGeneration++;
  sendSequentially(buildSyncMessages(cfg), syncGeneration);
}

function sendStatus(code) {
  var msg = {};
  msg[KEY_STATUS] = code;
  Pebble.sendAppMessage(msg, function () {}, function () {});
}

// --- Configuration page
Pebble.addEventListener('showConfiguration', function () {
  Pebble.openURL(configPage.buildConfigUrl(loadConfig()));
});

Pebble.addEventListener('webviewclosed', function (e) {
  if (!e || !e.response || e.response === 'CANCELLED') {
    console.log('Configuration closed without response');
    return;
  }
  try {
    var cfg = sanitizeConfig(JSON.parse(decodeURIComponent(e.response)));
    saveConfig(cfg);
    sendSettingsToWatch(cfg);
    console.log('Configuration saved, webhooks:', cfg.webhooks.length);
  } catch (err) {
    console.log('Error processing configuration:', err);
  }
});

Pebble.addEventListener('ready', function () {
  console.log('Pebble ready');
  sendSettingsToWatch(loadConfig());
});

// --- AppMessage: receive trigger -> call webhook
Pebble.addEventListener('appmessage', function (e) {
  var trigger = null;
  if (e && e.payload) {
    if (typeof e.payload[KEY_TRIGGER] !== 'undefined') trigger = e.payload[KEY_TRIGGER];
    else if (typeof e.payload.KEY_TRIGGER !== 'undefined') trigger = e.payload.KEY_TRIGGER;
  }
  if (trigger === null || typeof trigger === 'undefined') {
    console.log('No trigger in payload');
    return;
  }

  var cfg = loadConfig();
  var index = parseInt(trigger, 10);           // 1-based position in the list
  var hook = cfg.webhooks[index - 1];
  if (!index || !hook) {
    console.log('Unknown webhook index:', trigger);
    sendStatus(-2);                             // -2 = webhook not found
    return;
  }
  if (!hook.url) {
    console.log('Missing URL for webhook', index);
    sendStatus(0);
    return;
  }

  var method = hook.method || 'POST';
  console.log('Triggering webhook', index, hook.name, method);

  try {
    var xhr = new XMLHttpRequest();
    xhr.open(method, hook.url, true);
    xhr.timeout = 10000;

    // Custom headers
    var hasContentType = false;
    hook.headers.forEach(function (h) {
      try {
        xhr.setRequestHeader(h.name, h.value);
        if (h.name.toLowerCase() === 'content-type') hasContentType = true;
      } catch (err) {
        console.log('Skipping header "' + h.name + '":', err);
      }
    });

    // Cloudflare Access
    if (hook.useAuth) {
      try {
        if (hook.clientId) xhr.setRequestHeader('CF-Access-Client-Id', hook.clientId);
        if (hook.clientSecret) xhr.setRequestHeader('CF-Access-Client-Secret', hook.clientSecret);
      } catch (err) {
        console.log('Error setting Cloudflare headers, sending without:', err);
      }
    }

    // Body (never for GET)
    var body = (method !== 'GET' && hook.body) ? hook.body : null;
    if (body !== null && !hasContentType) {
      var first = body.replace(/^\s+/, '').charAt(0);
      try {
        xhr.setRequestHeader('Content-Type', (first === '{' || first === '[') ? 'application/json' : 'text/plain');
      } catch (err) {
        console.log('Error setting Content-Type:', err);
      }
    }

    xhr.onload = function () {
      console.log('Webhook response status:', xhr.status);
      // Treat every 2xx answer (200, 201, 204, ...) as success.
      var code = (xhr.status >= 200 && xhr.status < 300) ? 200 : xhr.status;
      sendStatus(code);
    };
    xhr.onerror = function () {
      console.log('XHR error:', xhr.status);
      sendStatus(0);
    };
    xhr.ontimeout = function () {
      console.log('XHR timeout');
      sendStatus(-1);
    };

    if (body !== null) xhr.send(body);
    else xhr.send();
  } catch (err) {
    console.log('Error sending XHR:', err);
    sendStatus(0);
  }
});