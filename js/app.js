/**
 * app.js — UI logic for PIVOTECH debug detector
 */

/* ── DOM helpers ── */
const $ = id => document.getElementById(id);
const $$ = sel => document.querySelectorAll(sel);

/* ── Tabs ── */
function initTabs() {
  $$('.nav-tab').forEach(tab => {
    tab.addEventListener('click', () => {
      const target = tab.dataset.tab;
      $$('.nav-tab').forEach(t => t.classList.toggle('active', t === tab));
      $$('.tab-pane').forEach(p => p.classList.toggle('active', p.id === 'tab-' + target));
    });
  });
}

/* ── Terminal state ── */
const term = {
  showHex:     false,
  showTs:      true,
  autoScroll:  true,
  lineCount:   0,
  maxLines:    2000,
};

/* ── Verify banner ── */
function setVerifyBanner(state, title, detail) {
  const banner = $('verify-banner');
  const icon   = $('verify-icon');
  const msg    = $('verify-msg');
  const det    = $('verify-detail');

  banner.className = 'verify-banner ' + state;
  icon.textContent = { idle: '🔌', checking: '', ok: '✅', fail: '❌', warn: '⚠️' }[state] || '';
  if (state === 'checking') {
    icon.innerHTML = '<span class="spinner"></span>';
  }
  msg.textContent = title;
  det.textContent = detail || '';
}

/* ── Connection badge ── */
function setConnBadge(state, text) {
  const badge = $('conn-badge');
  badge.className  = 'conn-badge ' + state;
  $('conn-status-text').textContent = text;
}

/* ── Terminal append ── */
function appendLine(type, content, raw) {
  const out = $('term-out');
  if (term.lineCount >= term.maxLines) {
    // Remove oldest line
    if (out.firstChild) out.removeChild(out.firstChild);
  }

  const now = new Date();
  const ts  = now.toTimeString().slice(0, 8) + '.' + String(now.getMilliseconds()).padStart(3, '0');

  const line = document.createElement('div');
  line.className = 'tline';

  const dirSymbol = { rx: '←', tx: '→', sys: '·', warn: '!', err: '✕' }[type] || '·';

  const tsEl = document.createElement('span');
  tsEl.className = 'tts';
  tsEl.textContent = term.showTs ? ts : '';

  const dirEl = document.createElement('span');
  dirEl.className = 'tdir ' + type;
  dirEl.textContent = dirSymbol;

  const cntEl = document.createElement('span');
  cntEl.className = 'tcnt ' + type;

  if (term.showHex && raw instanceof Uint8Array) {
    Array.from(raw).forEach(b => {
      const sp = document.createElement('span');
      sp.className = 'hbyte';
      sp.textContent = b.toString(16).toUpperCase().padStart(2, '0') + ' ';
      cntEl.appendChild(sp);
    });
  } else {
    cntEl.textContent = content;
  }

  line.appendChild(tsEl);
  line.appendChild(dirEl);
  line.appendChild(cntEl);
  out.appendChild(line);
  term.lineCount++;

  if (term.autoScroll) {
    out.scrollTop = out.scrollHeight;
  }
}

function sysLog(msg, type) {
  appendLine(type || 'sys', msg);
}

/* ── Stats bar ── */
function updateStats() {
  $('stat-rx').textContent = formatBytes(Serial.state.rxBytes);
  $('stat-tx').textContent = formatBytes(Serial.state.txBytes);
  $('stat-conn').textContent = Serial.state.connected ? '已连接' : '未连接';
}

function formatBytes(n) {
  if (n < 1024) return n + ' B';
  if (n < 1048576) return (n / 1024).toFixed(1) + ' KB';
  return (n / 1048576).toFixed(1) + ' MB';
}

/* ── Parameter value display ── */
const paramValues = {};

function updateParamDisplay(id, val) {
  const el = document.getElementById('pval-' + id);
  if (el) el.textContent = val;
  paramValues[id] = val;
}

/* ── Command log ── */
function addCmdLog(dir, text) {
  const log = $('cmd-log');
  const entry = document.createElement('div');
  entry.className = 'cmd-entry';
  const now = new Date();
  const ts  = now.toTimeString().slice(0, 8);
  entry.innerHTML =
    `<span class="cmd-time">${ts}</span>` +
    `<span class="cmd-dir ${dir}">${dir === 'send' ? '→' : dir === 'recv' ? '←' : dir}</span>` +
    `<span class="cmd-text">${escHtml(text)}</span>`;
  log.appendChild(entry);
  log.scrollTop = log.scrollHeight;
}

function escHtml(s) {
  return s.replace(/&/g,'&amp;').replace(/</g,'&lt;').replace(/>/g,'&gt;');
}

/* ── Send command ── */
async function sendCommand(raw) {
  if (!Serial.state.connected) {
    sysLog('未连接 — 请先连接设备', 'warn');
    return;
  }
  const eolMap = { '\\r\\n': '\r\n', '\\n': '\n', '\\r': '\r', 'none': '' };
  const eolSel = $('send-eol').value;
  const eol    = eolMap[eolSel] ?? '\r\n';
  const text   = raw + eol;
  await Serial.writeText(text);
  appendLine('tx', raw);
  addCmdLog('send', raw);
  updateStats();
}

/* ── Receive incoming data ── */
function onData(bytes, text) {
  /* Split into lines for cleaner display */
  const lines = text.split(/\r?\n/);
  lines.forEach((line, i) => {
    if (line.length > 0 || i < lines.length - 1) {
      appendLine('rx', line, bytes);
    }
  });
  updateStats();
  /* Update param displays if we can parse key=value */
  parseParamResponse(text);
}

function parseParamResponse(text) {
  /* Simple key=value parser: e.g. "FREQ=1000" */
  const re = /([A-Z_][A-Z0-9_]*)=([^\s,;]+)/g;
  let m;
  while ((m = re.exec(text)) !== null) {
    const key = m[1].toLowerCase();
    const val = m[2];
    const el  = document.getElementById('pval-' + key);
    if (el) el.textContent = val;
  }
}

/* ── Connect / Disconnect ── */
async function doConnect() {
  const baudRate = parseInt($('baud-rate').value, 10);
  const dataBits = parseInt($('data-bits').value, 10);
  const stopBits = parseInt($('stop-bits').value, 10);
  const parity   = $('parity').value;
  const handshakeCmd   = $('hs-cmd').value   || 'PIVOTECH_ID\r\n';
  const handshakeToken = $('hs-token').value || 'PIVOTECH';

  $('btn-connect').disabled    = true;
  $('btn-disconnect').disabled = false;
  $('btn-reverify').disabled   = false;

  setVerifyBanner('checking', '正在连接串口…', '');
  setConnBadge('', '连接中…');
  sysLog(`连接中 — 波特率 ${baudRate}`, 'sys');

  try {
    await Serial.connect({ baudRate, dataBits, stopBits, parity, handshakeCmd, handshakeToken });
  } catch (err) {
    if (err.name !== 'NotFoundError') {
      sysLog('连接失败: ' + err.message, 'err');
      setVerifyBanner('fail', '连接失败', err.message);
    }
    $('btn-connect').disabled    = false;
    $('btn-disconnect').disabled = true;
    $('btn-reverify').disabled   = true;
    setConnBadge('', '未连接');
  }
}

function doDisconnect() {
  Serial.disconnect();
}

function doReVerify() {
  if (!Serial.state.connected) return;
  setVerifyBanner('checking', '重新发送握手命令…', '');
  sysLog('重新验证设备…', 'sys');
  Serial.reVerify();
}

/* ── Wire up Serial callbacks ── */
function wireSerial() {
  Serial.cb.onData = onData;

  Serial.cb.onConnect = () => {
    setConnBadge('connected', '已连接');
    setVerifyBanner('checking', '正在验证设备…',
      `发送: ${Serial.cfg.handshakeCmd.replace(/\r/g,'\\r').replace(/\n/g,'\\n')}`);
    sysLog('串口已打开', 'sys');
    updateStats();
  };

  Serial.cb.onDisconnect = () => {
    setConnBadge('', '未连接');
    setVerifyBanner('idle', '未连接', '连接设备后将自动验证');
    sysLog('串口已关闭', 'sys');
    $('btn-connect').disabled    = false;
    $('btn-disconnect').disabled = true;
    $('btn-reverify').disabled   = true;
    updateStats();
  };

  Serial.cb.onVerified = (info) => {
    setConnBadge('verified', '已验证');
    setVerifyBanner('ok', '设备验证成功 ✓',
      `识别到 PIVOTECH 设备：${info.slice(0, 80)}`);
    sysLog('设备验证成功: ' + info.slice(0, 60), 'sys');
    addCmdLog('ok', '设备验证通过');
  };

  Serial.cb.onVerifyFail = (reason) => {
    setConnBadge('unverified', '未验证');
    setVerifyBanner('warn', '设备未验证', reason);
    sysLog('验证失败: ' + reason, 'warn');
    addCmdLog('fail', reason);
  };

  Serial.cb.onError = (err) => {
    sysLog('串口错误: ' + err.message, 'err');
    setVerifyBanner('fail', '串口错误', err.message);
  };
}

/* ── Firmware table search ── */
function initFwSearch() {
  const input = $('fw-search');
  if (!input) return;
  input.addEventListener('input', () => {
    const q = input.value.toLowerCase();
    $$('.fw-table tbody tr').forEach(row => {
      row.style.display = row.textContent.toLowerCase().includes(q) ? '' : 'none';
    });
  });
}

/* ── Param control ── */
function sendParamCmd(paramId, inputId, cmd) {
  const val = $(inputId) ? $(inputId).value : '';
  const full = cmd.replace('{v}', val);
  sendCommand(full);
}

function queryParam(cmd) {
  sendCommand(cmd);
}

/* ── Theme / display toggles ── */
function initToggles() {
  $('tog-hex').addEventListener('click', () => {
    term.showHex = !term.showHex;
    $('tog-hex').classList.toggle('active', term.showHex);
  });
  $('tog-ts').addEventListener('click', () => {
    term.showTs = !term.showTs;
    $('tog-ts').classList.toggle('active', term.showTs);
    $$('.tts').forEach(el => { el.style.display = term.showTs ? '' : 'none'; });
  });
  $('tog-scroll').addEventListener('click', () => {
    term.autoScroll = !term.autoScroll;
    $('tog-scroll').classList.toggle('active', term.autoScroll);
  });
}

/* ── Send bar ── */
function initSendBar() {
  const input  = $('send-input');
  const btnSnd = $('btn-send');

  btnSnd.addEventListener('click', () => {
    const v = input.value.trim();
    if (v) { sendCommand(v); input.value = ''; }
  });

  input.addEventListener('keydown', e => {
    if (e.key === 'Enter') { btnSnd.click(); }
  });
}

/* ── Clear terminal ── */
function clearTerm() {
  $('term-out').innerHTML = '';
  term.lineCount = 0;
  sysLog('终端已清空', 'sys');
}

/* ── Range sliders ── */
function initSliders() {
  $$('input[type=range]').forEach(slider => {
    const displayId = slider.dataset.display;
    if (!displayId) return;
    slider.addEventListener('input', () => {
      const el = document.getElementById(displayId);
      if (el) el.textContent = slider.value;
    });
  });
}

/* ── Web Serial support check ── */
function checkSerialSupport() {
  if (!('serial' in navigator)) {
    setVerifyBanner('fail',
      '此浏览器不支持 Web Serial API',
      '请使用 Chrome 89+、Edge 89+ 或 Opera 75+');
    $('btn-connect').disabled = true;
    sysLog('Web Serial API 不可用 — 请更换支持的浏览器', 'err');
  } else {
    setVerifyBanner('idle', '未连接', '点击"连接串口"按钮开始');
  }
}

/* ── Init ── */
document.addEventListener('DOMContentLoaded', () => {
  initTabs();
  wireSerial();
  initToggles();
  initSendBar();
  initFwSearch();
  initSliders();
  checkSerialSupport();

  $('btn-connect').addEventListener('click', doConnect);
  $('btn-disconnect').addEventListener('click', doDisconnect);
  $('btn-reverify').addEventListener('click', doReVerify);
  $('btn-clear').addEventListener('click', clearTerm);

  /* Initial button state */
  $('btn-disconnect').disabled = true;
  $('btn-reverify').disabled   = true;

  updateStats();
  sysLog('PIVOTECH 调试器就绪', 'sys');
  sysLog('使用 Chrome / Edge 浏览器以获得最佳体验', 'sys');
});
