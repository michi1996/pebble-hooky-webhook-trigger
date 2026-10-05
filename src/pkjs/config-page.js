// Hooky configuration page.
// A small self-contained HTML page that is opened as a data URI (works fully offline, no cloud).
//
// NOTE: The page script below deliberately avoids backticks, "${" and backslashes,
// because it lives inside a JS template literal.

var PAGE = `<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1, viewport-fit=cover">
<title>Hooky Settings</title>
<style>
  :root {
    --bg: #f3f5f9; --card: #ffffff; --text: #14181f; --muted: #667085;
    --line: #dde2ea; --accent: #0055aa; --accent-text: #ffffff; --danger: #c0392b;
    --input: #f7f9fc;
  }
  @media (prefers-color-scheme: dark) {
    :root {
      --bg: #0f1218; --card: #1a1f29; --text: #eef1f6; --muted: #9aa4b5;
      --line: #2b3240; --accent: #4a90e2; --accent-text: #08111f; --danger: #ff7566;
      --input: #131820;
    }
  }
  * { box-sizing: border-box; }
  html, body { margin: 0; background: var(--bg); color: var(--text);
    font: 16px/1.4 -apple-system, "Segoe UI", Roboto, "Helvetica Neue", Arial, sans-serif; }
  body { padding: 16px 14px 96px; max-width: 560px; margin: 0 auto; }
  h1 { font-size: 22px; margin: 4px 0 2px; }
  h2 { font-size: 15px; margin: 22px 2px 8px; color: var(--muted); font-weight: 600; }
  p.hint { margin: 0 2px 14px; color: var(--muted); font-size: 14px; }
  p.note { margin: 6px 4px 0; color: var(--muted); font-size: 13px; }
  .card { background: var(--card); border: 1px solid var(--line); border-radius: 12px; overflow: hidden; }
  .row { display: flex; align-items: center; gap: 10px; padding: 12px 14px; }
  .row + .row { border-top: 1px solid var(--line); }
  .grow { flex: 1; min-width: 0; }
  .title { font-weight: 600; overflow: hidden; text-overflow: ellipsis; white-space: nowrap; }
  .sub { color: var(--muted); font-size: 14px; overflow: hidden; text-overflow: ellipsis; white-space: nowrap; }
  .empty { padding: 22px 14px; text-align: center; color: var(--muted); }
  .dot { display: inline-block; width: 10px; height: 10px; border-radius: 50%; margin-right: 7px; }
  button { font: inherit; border: 0; border-radius: 9px; padding: 9px 14px; cursor: pointer;
    background: var(--input); color: var(--text); border: 1px solid var(--line); }
  button.primary { background: var(--accent); color: var(--accent-text); border-color: var(--accent); font-weight: 600; }
  button.danger { color: var(--danger); }
  button.icon { padding: 9px 0; width: 38px; flex: none; }
  button.icon svg { display: block; margin: 0 auto; }
  .acts { display: flex; gap: 6px; flex: none; }
  .acts button.icon { width: 34px; }
  button:disabled { opacity: .35; cursor: default; }
  button:focus-visible, input:focus-visible, select:focus-visible, textarea:focus-visible {
    outline: 2px solid var(--accent); outline-offset: 2px; }
  .add { width: 100%; margin-top: 12px; padding: 13px; font-weight: 600; }
  .block { padding: 10px 14px 12px; }
  .block + .block, .row + .block, .block + .row, label.field + .block, .block + label.field { border-top: 1px solid var(--line); }
  .label { font-size: 13px; color: var(--muted); margin-bottom: 8px; }
  label.field { display: block; padding: 10px 14px 12px; }
  label.field + label.field, .row + label.field, label.field + .row { border-top: 1px solid var(--line); }
  label.field span { display: block; font-size: 13px; color: var(--muted); margin-bottom: 5px; }
  input[type=text], input[type=url], input[type=password], select, textarea {
    width: 100%; font: inherit; color: var(--text); background: var(--input);
    border: 1px solid var(--line); border-radius: 9px; padding: 10px 11px; }
  textarea { min-height: 96px; resize: vertical; font-family: ui-monospace, Menlo, Consolas, monospace; font-size: 14px; }
  input.bad { border-color: var(--danger); }
  .err { color: var(--danger); font-size: 13px; margin: 6px 4px 0; min-height: 18px; }
  .chips, .swatches { display: flex; flex-wrap: wrap; gap: 10px; }
  .swatch { width: 34px; height: 34px; border-radius: 50%; padding: 0; border: 1px solid var(--line); }
  .swatch.none { width: auto; padding: 0 14px; background: var(--input); font-size: 14px; }
  .swatch[aria-pressed=true] { outline: 3px solid var(--accent); outline-offset: 2px; }
  .hrow { display: flex; gap: 8px; margin-bottom: 8px; }
  .hrow input { flex: 1; min-width: 0; }
  .hrow button { flex: none; }
  button.menu { display: flex; align-items: center; gap: 10px; width: 100%; text-align: left;
    background: transparent; border: 0; border-radius: 0; padding: 14px; color: var(--text); }
  button.menu[aria-expanded=true] { border-bottom: 1px solid var(--line); }
  button.menu:focus-visible { outline-offset: -3px; }
  .mtitle { display: block; font-weight: 600; }
  .msub { display: block; color: var(--muted); font-size: 14px; }
  .chev { flex: none; font-size: 26px; line-height: 1; color: var(--muted); transition: transform .15s; }
  button.menu[aria-expanded=true] .chev { transform: rotate(90deg); }
  .btns { display: flex; gap: 10px; }
  .btns button { flex: 1; }
  .switch { position: relative; width: 48px; height: 28px; flex: none; }
  .switch input { position: absolute; inset: 0; opacity: 0; margin: 0; width: 100%; height: 100%; cursor: pointer; }
  .switch i { position: absolute; inset: 0; background: var(--line); border-radius: 14px; transition: background .15s; pointer-events: none; }
  .switch i::after { content: ""; position: absolute; top: 3px; left: 3px; width: 22px; height: 22px;
    border-radius: 50%; background: #fff; transition: transform .15s; }
  .switch input:checked + i { background: var(--accent); }
  .switch input:checked + i::after { transform: translateX(20px); }
  .switch input:focus-visible + i { outline: 2px solid var(--accent); outline-offset: 2px; }
  .bar { position: fixed; left: 0; right: 0; bottom: 0; padding: 12px 14px calc(12px + env(safe-area-inset-bottom, 0px));
    background: var(--bg); border-top: 1px solid var(--line); }
  .bar > div { max-width: 560px; margin: 0 auto; display: flex; gap: 10px; }
  .bar button { flex: 1; padding: 13px; }
  .hidden { display: none !important; }
  @media (prefers-reduced-motion: reduce) { .switch i, .switch i::after, .chev { transition: none; } }
</style>
</head>
<body>

<!-- LIST VIEW -->
<div id="listView">
  <h1>Hooky</h1>
  <p class="hint">Webhooks shown on your watch, in this order.</p>
  <div id="list" class="card"></div>
  <button id="addBtn" class="add primary" type="button">+ Add webhook</button>

  <h2>Watch App options</h2>
  <div class="card">
    <div class="row">
      <div class="grow"><div class="title">Auto-close after success</div><div class="sub">Closes the app 5 seconds after a successful call</div></div>
      <label class="switch"><input id="autoClose" type="checkbox"><i></i></label>
    </div>
    <div class="row">
      <div class="grow"><div class="title">Sound feedback</div><div class="sub">Plays a sound on success and on error (watches with a speaker only)</div></div>
      <label class="switch"><input id="sound" type="checkbox"><i></i></label>
    </div>
    <div class="row">
      <div class="grow"><div class="title">Touch controls</div><div class="sub">Scroll and run webhooks by touch (touchscreen watches only)</div></div>
      <label class="switch"><input id="touch" type="checkbox"><i></i></label>
    </div>
  </div>

  <div class="card" style="margin-top:12px">
    <button id="appearanceToggle" class="menu" type="button" aria-expanded="false" aria-controls="appearanceBody">
      <span class="grow"><span class="mtitle">Appearance</span><span class="msub">Color themes, header, selection, dark list</span></span>
      <span class="chev" aria-hidden="true">›</span>
    </button>
    <div id="appearanceBody" class="hidden">
    <div class="block">
      <div class="label">Color themes</div>
      <div id="presets" class="chips"></div>
    </div>
    <div class="block">
      <div class="label">Header color</div>
      <div id="hdrSwatches" class="swatches"></div>
    </div>
    <div class="block">
      <div class="label">Selection color</div>
      <div id="hlSwatches" class="swatches"></div>
    </div>
    <div class="row">
      <div class="grow"><div class="title">Dark list</div><div class="sub">Light text on a dark background</div></div>
      <label class="switch"><input id="darkList" type="checkbox"><i></i></label>
    </div>
      <div class="block"><div class="sub" style="white-space:normal">Colors are shown on color watches only. Each webhook can get its own button color in its settings.</div></div>
    </div>
  </div>

  <h2>Backup</h2>
  <div class="card">
    <div class="block">
      <textarea id="backup" spellcheck="false" autocomplete="off" autocapitalize="off" placeholder="Tap Export to see your settings here, or paste an export and tap Import."></textarea>
      <div class="btns" style="margin-top:10px">
        <button id="exportBtn" type="button">Export</button>
        <button id="importBtn" type="button">Import</button>
      </div>
    </div>
  </div>
  <p class="note">The export contains client secrets and header values as plain text. Keep it private.</p>
  <div id="backupMsg" class="err" style="color:var(--muted)"></div>

  <div class="bar"><div>
    <button id="saveBtn" class="primary" type="button">Save settings</button>
  </div></div>
</div>

<!-- EDIT VIEW -->
<div id="editView" class="hidden">
  <h1 id="editHeading">Edit webhook</h1>
  <p class="hint">Changes are kept when you tap Done. Save settings on the main page to send them to your watch.</p>

  <div class="card">
    <label class="field"><span>Name</span><input id="fName" type="text" maxlength="31" placeholder="Living room light" autocomplete="off"></label>
    <label class="field"><span>Webhook URL</span><input id="fUrl" type="url" placeholder="https://example.com/webhook" autocomplete="off" autocapitalize="off" spellcheck="false"></label>
  </div>
  <div id="urlErr" class="err"></div>

  <h2>Description</h2>
  <div class="card">
    <div class="row">
      <div class="grow"><div class="title">Show description</div><div class="sub">Turn off for a slimmer button on the watch</div></div>
      <label class="switch"><input id="fShowDesc" type="checkbox"><i></i></label>
    </div>
    <label class="field" id="descField"><span>Description</span><input id="fDesc" type="text" maxlength="39" placeholder="Turns the light on" autocomplete="off"></label>
  </div>

  <h2>Request</h2>
  <div class="card">
    <label class="field"><span>Method</span><select id="fMethod"></select></label>
    <label class="field" id="bodyField"><span>Body (optional)</span><textarea id="fBody" spellcheck="false" autocomplete="off" autocapitalize="off" placeholder="{&quot;state&quot;: &quot;on&quot;}"></textarea></label>
    <div class="block">
      <div class="label">Custom headers</div>
      <div id="hdrList"></div>
      <button id="addHdr" type="button">+ Add header</button>
    </div>
  </div>
  <p class="note">If a body is set and you add no Content-Type header, it is sent as application/json when it starts with { or [, otherwise as text/plain.</p>

  <h2>Response</h2>
  <div class="card">
    <label class="field"><span>Show on watch</span><select id="fResponse">
      <option value="status">Status only (Success / Error)</option>
      <option value="text">Response text</option>
      <option value="json">Value from a JSON response</option>
    </select></label>
    <label class="field" id="pathField"><span>JSON path</span><input id="fJsonPath" type="text" placeholder="state  or  attributes.temperature" autocomplete="off" autocapitalize="off" spellcheck="false"></label>
    <label class="field" id="formatField"><span>Format (optional)</span><input id="fTemplate" type="text" placeholder="{value} &deg;C" autocomplete="off" autocapitalize="off" spellcheck="false"></label>
  </div>
  <p class="note" id="responseNote">The answer opens in a scrollable window on the watch (up to about 480 characters). In the format, {value} is replaced by the answer. On the watch, SELECT runs the webhook again to refresh.</p>

  <h2>Authentication</h2>
  <div class="card">
    <div class="row">
      <div class="grow"><div class="title">Use Client ID and Secret</div><div class="sub">Sent as Cloudflare Access headers</div></div>
      <label class="switch"><input id="fUseAuth" type="checkbox"><i></i></label>
    </div>
    <div id="authFields">
      <label class="field"><span>Client ID</span><input id="fClientId" type="text" autocomplete="off" autocapitalize="off" spellcheck="false"></label>
      <label class="field"><span>Client Secret</span><input id="fClientSecret" type="password" autocomplete="off" autocapitalize="off" spellcheck="false"></label>
    </div>
  </div>

  <h2>Button</h2>
  <div class="card">
    <div class="block">
      <div class="label">Button color</div>
      <div id="colorSwatches" class="swatches"></div>
    </div>
    <div class="row">
      <div class="grow"><div class="title">Ask before running</div><div class="sub">Shows a confirmation on the watch first</div></div>
      <label class="switch"><input id="fConfirm" type="checkbox"><i></i></label>
    </div>
  </div>

  <div class="bar"><div>
    <button id="delBtn" class="danger" type="button">Delete</button>
    <button id="dupBtn" type="button">Duplicate</button>
    <button id="doneBtn" class="primary" type="button">Done</button>
  </div></div>
</div>

<script>
var INITIAL = __HOOKY_INITIAL__;

var METHODS = ['GET', 'POST', 'PUT', 'PATCH', 'DELETE'];
var PALETTE = ['#0055AA', '#00AAFF', '#00FFFF', '#00AA55', '#55FF55', '#FFFF00', '#FFAA00',
               '#FF5500', '#FF0000', '#FF55AA', '#AA00FF', '#5500AA', '#555555', '#000000'];
var PRESETS = [
  { name: 'Classic', header: '#0055AA', hl: '#00FFFF', dark: false },
  { name: 'Forest',  header: '#00AA55', hl: '#55FF55', dark: false },
  { name: 'Sunset',  header: '#FF5500', hl: '#FFFF00', dark: false },
  { name: 'Grape',   header: '#5500AA', hl: '#FF55AA', dark: false },
  { name: 'Night',   header: '#555555', hl: '#00AAFF', dark: true }
];

var state = INITIAL;
var editing = null;          // webhook object currently open in the edit view
var editingIsNew = false;
var editingInsertAfter = null; // webhook after which a duplicate is inserted
var editingHeaders = [];     // working copy of the custom headers
var editingColor = '';       // working copy of the button color
var deleteArmed = false;
var importArmed = false;

function $(id) { return document.getElementById(id); }
function show(id, on) { $(id).classList.toggle('hidden', !on); }

function newId() {
  return 'w' + Date.now().toString(36) + Math.floor(Math.random() * 1e6).toString(36);
}

function el(tag, cls, text) {
  var e = document.createElement(tag);
  if (cls) e.className = cls;
  if (typeof text === 'string') e.textContent = text;
  return e;
}

function str(v) { return typeof v === 'string' ? v : (v === null || v === undefined ? '' : String(v)); }
function hex(v, fallback) {
  var s = str(v).trim().toUpperCase();
  return /^#[0-9A-F]{6}$/.test(s) ? s : fallback;
}

/* ---------- color swatches ---------- */

function renderSwatches(containerId, current, allowNone, onPick) {
  var box = $(containerId);
  box.innerHTML = '';
  if (allowNone) {
    var none = el('button', 'swatch none', 'None');
    none.type = 'button';
    none.setAttribute('aria-pressed', String(!current));
    none.addEventListener('click', function () { onPick(''); });
    box.appendChild(none);
  }
  PALETTE.forEach(function (c) {
    var b = el('button', 'swatch');
    b.type = 'button';
    b.style.background = c;
    b.setAttribute('aria-label', c);
    b.setAttribute('aria-pressed', String(current === c));
    b.addEventListener('click', function () { onPick(c); });
    box.appendChild(b);
  });
}

function renderAppearance() {
  renderSwatches('hdrSwatches', state.headerColor, false, function (c) {
    state.headerColor = c; renderAppearance();
  });
  renderSwatches('hlSwatches', state.highlightColor, false, function (c) {
    state.highlightColor = c; renderAppearance();
  });
  var presets = $('presets');
  presets.innerHTML = '';
  PRESETS.forEach(function (p) {
    var b = el('button', null, p.name);
    b.type = 'button';
    b.addEventListener('click', function () {
      state.headerColor = p.header;
      state.highlightColor = p.hl;
      state.darkList = p.dark;
      $('darkList').checked = p.dark;
      renderAppearance();
    });
    presets.appendChild(b);
  });
}

/* ---------- list view ---------- */

function copyIcon() {
  var ns = 'http://www.w3.org/2000/svg';
  var svg = document.createElementNS(ns, 'svg');
  svg.setAttribute('viewBox', '0 0 24 24');
  svg.setAttribute('width', '18');
  svg.setAttribute('height', '18');
  svg.setAttribute('fill', 'none');
  svg.setAttribute('stroke', 'currentColor');
  svg.setAttribute('stroke-width', '2');
  svg.setAttribute('stroke-linecap', 'round');
  svg.setAttribute('stroke-linejoin', 'round');
  svg.setAttribute('aria-hidden', 'true');
  var rect = document.createElementNS(ns, 'rect');
  rect.setAttribute('x', '9');
  rect.setAttribute('y', '9');
  rect.setAttribute('width', '13');
  rect.setAttribute('height', '13');
  rect.setAttribute('rx', '2');
  var path = document.createElementNS(ns, 'path');
  path.setAttribute('d', 'M5 15H4a2 2 0 0 1-2-2V4a2 2 0 0 1 2-2h9a2 2 0 0 1 2 2v1');
  svg.appendChild(rect);
  svg.appendChild(path);
  return svg;
}

// Opens a copy of a webhook in the edit view; it is inserted below the original on Done.
function openCopyOf(original) {
  var copy = JSON.parse(JSON.stringify(original));
  copy.id = newId();
  copy.name = (original.name + ' copy').slice(0, 31);
  openEdit(copy, true, original);
  $('fName').focus();
  $('fName').select();
}

function move(i, delta) {
  var j = i + delta;
  if (j < 0 || j >= state.webhooks.length) return;
  var tmp = state.webhooks[i];
  state.webhooks[i] = state.webhooks[j];
  state.webhooks[j] = tmp;
  renderList();
}

function renderList() {
  var list = $('list');
  list.innerHTML = '';
  if (!state.webhooks.length) {
    list.appendChild(el('div', 'empty', 'No webhooks yet. Tap "Add webhook" to create your first one.'));
  }
  state.webhooks.forEach(function (w, i) {
    var row = el('div', 'row');
    var info = el('div', 'grow');
    var title = el('div', 'title');
    if (w.color) {
      var dot = el('span', 'dot');
      dot.style.background = w.color;
      title.appendChild(dot);
    }
    title.appendChild(document.createTextNode(w.name || 'Unnamed webhook'));
    info.appendChild(title);
    if (w.showDesc && w.desc) info.appendChild(el('div', 'sub', w.desc));

    var up = el('button', 'icon', '↑');
    up.type = 'button';
    up.setAttribute('aria-label', 'Move up');
    up.disabled = i === 0;
    up.addEventListener('click', function () { move(i, -1); });

    var down = el('button', 'icon', '↓');
    down.type = 'button';
    down.setAttribute('aria-label', 'Move down');
    down.disabled = i === state.webhooks.length - 1;
    down.addEventListener('click', function () { move(i, 1); });

    var dup = el('button', 'icon');
    dup.type = 'button';
    dup.title = 'Duplicate';
    dup.setAttribute('aria-label', 'Duplicate');
    dup.appendChild(copyIcon());
    dup.addEventListener('click', function () { openCopyOf(w); });

    var edit = el('button', null, 'Edit');
    edit.type = 'button';
    edit.addEventListener('click', function () { openEdit(w, false); });

    var acts = el('div', 'acts');
    acts.appendChild(up);
    acts.appendChild(down);
    acts.appendChild(dup);
    acts.appendChild(edit);

    row.appendChild(info);
    row.appendChild(acts);
    list.appendChild(row);
  });
  $('autoClose').checked = !!state.autoClose;
  $('sound').checked = !!state.soundFeedback;
  $('touch').checked = state.touchEnabled !== false;
  $('darkList').checked = !!state.darkList;
  renderAppearance();
}

/* ---------- edit view ---------- */

function renderHeaders() {
  var box = $('hdrList');
  box.innerHTML = '';
  editingHeaders.forEach(function (h, i) {
    var row = el('div', 'hrow');
    var name = el('input');
    name.type = 'text';
    name.placeholder = 'Header';
    name.value = h.name;
    name.setAttribute('autocomplete', 'off');
    name.setAttribute('autocapitalize', 'off');
    name.setAttribute('spellcheck', 'false');
    name.addEventListener('input', function () { h.name = name.value; });
    var value = el('input');
    value.type = 'text';
    value.placeholder = 'Value';
    value.value = h.value;
    value.setAttribute('autocomplete', 'off');
    value.setAttribute('autocapitalize', 'off');
    value.setAttribute('spellcheck', 'false');
    value.addEventListener('input', function () { h.value = value.value; });
    var del = el('button', 'icon', '×');
    del.type = 'button';
    del.setAttribute('aria-label', 'Remove header');
    del.addEventListener('click', function () { editingHeaders.splice(i, 1); renderHeaders(); });
    row.appendChild(name);
    row.appendChild(value);
    row.appendChild(del);
    box.appendChild(row);
  });
}

function renderColorPicker() {
  renderSwatches('colorSwatches', editingColor, true, function (c) {
    editingColor = c; renderColorPicker();
  });
}

function syncEditVisibility() {
  show('descField', $('fShowDesc').checked);
  show('authFields', $('fUseAuth').checked);
  show('bodyField', $('fMethod').value !== 'GET');
  var mode = $('fResponse').value;
  show('pathField', mode === 'json');
  show('formatField', mode !== 'status');
  show('responseNote', mode !== 'status');
}

function openEdit(w, isNew, insertAfter) {
  editing = w;
  editingIsNew = isNew;
  editingInsertAfter = insertAfter || null;
  deleteArmed = false;
  $('delBtn').textContent = isNew ? 'Discard' : 'Delete';
  $('editHeading').textContent = insertAfter ? 'Duplicate webhook' : (isNew ? 'New webhook' : 'Edit webhook');
  $('fName').value = w.name || '';
  $('fUrl').value = w.url || '';
  $('fShowDesc').checked = !!w.showDesc;
  $('fDesc').value = w.desc || '';
  $('fMethod').value = w.method || 'POST';
  $('fBody').value = w.body || '';
  $('fUseAuth').checked = !!w.useAuth;
  $('fClientId').value = w.clientId || '';
  $('fClientSecret').value = w.clientSecret || '';
  $('fConfirm').checked = !!w.confirm;
  $('fResponse').value = w.response === 'text' || w.response === 'json' ? w.response : 'status';
  $('fJsonPath').value = w.jsonPath || '';
  $('fTemplate').value = w.template || '';
  editingHeaders = (w.headers || []).map(function (h) { return { name: h.name, value: h.value }; });
  editingColor = w.color || '';
  $('urlErr').textContent = '';
  $('fUrl').classList.remove('bad');
  renderHeaders();
  renderColorPicker();
  syncEditVisibility();
  show('listView', false);
  show('editView', true);
  window.scrollTo(0, 0);
  if (isNew) $('fName').focus();
}

function closeEdit() {
  editing = null;
  show('editView', false);
  show('listView', true);
  renderList();
  window.scrollTo(0, 0);
}

function validUrl(u) {
  return u.indexOf('http://') === 0 || u.indexOf('https://') === 0;
}

// Writes the form into the webhook and adds it to the list if it is new.
// Returns false (and shows an error) when the URL is not valid.
function applyForm() {
  var url = $('fUrl').value.trim();
  if (!validUrl(url)) {
    $('urlErr').textContent = 'Enter a URL that starts with http:// or https://';
    $('fUrl').classList.add('bad');
    $('fUrl').focus();
    return false;
  }
  editing.name = $('fName').value.trim() || 'Webhook';
  editing.url = url;
  editing.showDesc = $('fShowDesc').checked;
  editing.desc = $('fDesc').value.trim();
  editing.method = $('fMethod').value;
  editing.body = $('fBody').value;
  editing.headers = editingHeaders
    .map(function (h) { return { name: h.name.trim(), value: h.value }; })
    .filter(function (h) { return h.name; });
  editing.useAuth = $('fUseAuth').checked;
  editing.clientId = $('fClientId').value.trim();
  editing.clientSecret = $('fClientSecret').value.trim();
  editing.color = editingColor;
  editing.confirm = $('fConfirm').checked;
  editing.response = $('fResponse').value;
  editing.jsonPath = $('fJsonPath').value.trim();
  editing.template = $('fTemplate').value;
  if (state.webhooks.indexOf(editing) === -1) {
    var after = editingInsertAfter ? state.webhooks.indexOf(editingInsertAfter) : -1;
    if (after === -1) state.webhooks.push(editing);
    else state.webhooks.splice(after + 1, 0, editing);
  }
  return true;
}

function commitEdit() {
  if (applyForm()) closeEdit();
}

// Saves the current webhook, then opens a copy of it right below the original.
function duplicateEditing() {
  if (!applyForm()) return;
  openCopyOf(editing);
}

function deleteEditing() {
  if (!editingIsNew && !deleteArmed) {
    deleteArmed = true;
    $('delBtn').textContent = 'Confirm';
    return;
  }
  var i = state.webhooks.indexOf(editing);
  if (i !== -1) state.webhooks.splice(i, 1);
  closeEdit();
}

/* ---------- backup ---------- */

function collectGlobal() {
  state.autoClose = $('autoClose').checked;
  state.soundFeedback = $('sound').checked;
  state.touchEnabled = $('touch').checked;
  state.darkList = $('darkList').checked;
}

function setBackupMsg(text) { $('backupMsg').textContent = text; }

function fallbackCopy(ta) {
  var ok = false;
  try { ta.focus(); ta.select(); ok = document.execCommand('copy'); } catch (e) { ok = false; }
  setBackupMsg(ok ? 'Export copied to clipboard.' : 'Select the text above and copy it manually.');
}

function doExport() {
  collectGlobal();
  importArmed = false;
  $('importBtn').textContent = 'Import';
  var ta = $('backup');
  ta.value = JSON.stringify(state, null, 2);
  try {
    if (navigator.clipboard && navigator.clipboard.writeText) {
      navigator.clipboard.writeText(ta.value).then(
        function () { setBackupMsg('Export copied to clipboard.'); },
        function () { fallbackCopy(ta); });
      return;
    }
  } catch (e) { /* fall through */ }
  fallbackCopy(ta);
}

function normalizeImported(o) {
  var list = Array.isArray(o) ? o : (o && Array.isArray(o.webhooks) ? o.webhooks : null);
  if (!list) return null;
  var src = Array.isArray(o) ? {} : o;
  var out = {
    version: 3,
    autoClose: src.autoClose === true,
    soundFeedback: src.soundFeedback === true,
    touchEnabled: src.touchEnabled !== false,
    darkList: src.darkList === true,
    headerColor: hex(src.headerColor, '#0055AA'),
    highlightColor: hex(src.highlightColor, '#00FFFF'),
    webhooks: []
  };
  list.forEach(function (w) {
    if (!w || typeof w !== 'object') return;
    var method = str(w.method).toUpperCase();
    out.webhooks.push({
      id: newId() + out.webhooks.length,
      name: str(w.name),
      desc: str(w.desc),
      showDesc: w.showDesc !== false,
      url: str(w.url).trim(),
      method: METHODS.indexOf(method) === -1 ? 'POST' : method,
      body: str(w.body),
      headers: (Array.isArray(w.headers) ? w.headers : []).map(function (h) {
        return { name: str(h && h.name).trim(), value: str(h && h.value) };
      }).filter(function (h) { return h.name; }),
      useAuth: w.useAuth === true,
      clientId: str(w.clientId).trim(),
      clientSecret: str(w.clientSecret).trim(),
      color: hex(w.color, ''),
      confirm: w.confirm === true,
      response: (w.response === 'text' || w.response === 'json') ? w.response : 'status',
      jsonPath: str(w.jsonPath).trim(),
      template: str(w.template)
    });
  });
  return out;
}

function doImport() {
  var text = $('backup').value.trim();
  var parsed = null;
  try { parsed = JSON.parse(text); } catch (e) { parsed = null; }
  var imported = parsed ? normalizeImported(parsed) : null;
  if (!imported) {
    importArmed = false;
    $('importBtn').textContent = 'Import';
    setBackupMsg('That is not a valid Hooky export.');
    return;
  }
  if (!importArmed) {
    importArmed = true;
    $('importBtn').textContent = 'Tap again to replace';
    setBackupMsg('This replaces all current webhooks and appearance settings (' + imported.webhooks.length + ' webhooks found).');
    return;
  }
  importArmed = false;
  $('importBtn').textContent = 'Import';
  state = imported;
  renderList();
  setBackupMsg('Imported ' + imported.webhooks.length + ' webhooks. Tap Save settings to send them to your watch.');
}

/* ---------- save ---------- */

function getReturnTo() {
  var src = (location.search || '') + '&' + (location.hash || '').replace('#', '');
  var parts = src.replace('?', '').split('&');
  for (var i = 0; i < parts.length; i++) {
    var kv = parts[i].split('=');
    if (kv[0] === 'return_to' && kv[1]) return decodeURIComponent(kv[1]);
  }
  return 'pebblejs://close#';
}

function save() {
  collectGlobal();
  location.href = getReturnTo() + encodeURIComponent(JSON.stringify(state));
}

/* ---------- wiring ---------- */

METHODS.forEach(function (m) {
  var o = el('option', null, m);
  o.value = m;
  $('fMethod').appendChild(o);
});

$('addBtn').addEventListener('click', function () {
  openEdit({ id: newId(), name: '', desc: '', showDesc: true, url: '', method: 'POST', body: '',
             headers: [], useAuth: false, clientId: '', clientSecret: '', color: '', confirm: false,
             response: 'status', jsonPath: '', template: '' }, true);
});
$('addHdr').addEventListener('click', function () {
  editingHeaders.push({ name: '', value: '' });
  renderHeaders();
});
$('doneBtn').addEventListener('click', commitEdit);
$('delBtn').addEventListener('click', deleteEditing);
$('dupBtn').addEventListener('click', duplicateEditing);
$('saveBtn').addEventListener('click', save);
$('exportBtn').addEventListener('click', doExport);
$('importBtn').addEventListener('click', doImport);
$('backup').addEventListener('input', function () {
  importArmed = false;
  $('importBtn').textContent = 'Import';
});
$('appearanceToggle').addEventListener('click', function () {
  var open = $('appearanceBody').classList.contains('hidden');
  show('appearanceBody', open);
  $('appearanceToggle').setAttribute('aria-expanded', String(open));
});
$('darkList').addEventListener('change', function () { state.darkList = $('darkList').checked; });
$('fShowDesc').addEventListener('change', syncEditVisibility);
$('fUseAuth').addEventListener('change', syncEditVisibility);
$('fMethod').addEventListener('change', syncEditVisibility);
$('fResponse').addEventListener('change', syncEditVisibility);
$('fUrl').addEventListener('input', function () {
  $('urlErr').textContent = '';
  $('fUrl').classList.remove('bad');
});

renderList();
</script>
</body>
</html>`;

// Build a data URI that embeds the current configuration.
function buildConfigUrl(config) {
  var json = JSON.stringify(config)
    .replace(/</g, '\\u003c')
    .replace(/\u2028/g, '\\u2028')
    .replace(/\u2029/g, '\\u2029');
  var html = PAGE.replace('__HOOKY_INITIAL__', function () { return json; });
  return 'data:text/html;charset=utf-8,' + encodeURIComponent(html);
}

module.exports = { buildConfigUrl: buildConfigUrl };