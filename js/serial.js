/**
 * serial.js — Web Serial API wrapper with device verification
 * Sends a handshake command and validates the response to verify the device.
 */

const Serial = (() => {
  /* ── Public state ── */
  const state = {
    port: null,
    reader: null,
    writer: null,
    reading: false,
    connected: false,
    verified: false,           // true after successful handshake
    deviceInfo: null,          // parsed device info string
    rxBytes: 0,
    txBytes: 0,
  };

  /* ── Config (can be overridden by caller) ── */
  const cfg = {
    baudRate: 115200,
    dataBits: 8,
    stopBits: 1,
    parity: 'none',
    flowControl: 'none',
    /* Handshake protocol */
    handshakeCmd: 'PIVOTECH_ID\r\n',   // command sent to device
    handshakeToken: 'PIVOTECH',         // must appear in response
    handshakeTimeout: 3000,             // ms to wait for response
  };

  /* ── Callbacks registered by the UI ── */
  const cb = {
    onData: null,       // (bytes: Uint8Array) => void
    onConnect: null,    // () => void
    onDisconnect: null, // () => void
    onVerified: null,   // (info: string) => void
    onVerifyFail: null, // (reason: string) => void
    onError: null,      // (err: Error) => void
  };

  /* ── Helpers ── */
  function _emit(name, ...args) {
    if (typeof cb[name] === 'function') cb[name](...args);
  }

  /* ── Connect ── */
  async function connect(options = {}) {
    if (!('serial' in navigator)) {
      throw new Error('Web Serial API 不受此浏览器支持。请使用 Chrome 89+ 或 Edge 89+。');
    }
    Object.assign(cfg, options);

    try {
      state.port = await navigator.serial.requestPort();
      await state.port.open({
        baudRate:    cfg.baudRate,
        dataBits:    cfg.dataBits,
        stopBits:    cfg.stopBits,
        parity:      cfg.parity,
        flowControl: cfg.flowControl,
      });

      state.writer   = state.port.writable.getWriter();
      state.connected = true;
      state.verified  = false;
      state.deviceInfo = null;
      state.rxBytes = 0;
      state.txBytes = 0;

      _emit('onConnect');

      /* Start read loop then perform handshake */
      _startReadLoop();
      _doHandshake();

    } catch (err) {
      state.connected = false;
      if (err.name !== 'NotFoundError') _emit('onError', err);
      throw err;
    }
  }

  /* ── Read loop ── */
  function _startReadLoop() {
    state.reading = true;
    (async () => {
      const decoder = new TextDecoderStream();
      const inputDone = state.port.readable.pipeTo(decoder.writable);
      state.reader = decoder.readable.getReader();

      try {
        while (state.reading) {
          const { value, done } = await state.reader.read();
          if (done) break;
          if (value) {
            const bytes = new TextEncoder().encode(value);
            state.rxBytes += bytes.length;
            _emit('onData', bytes, value);
          }
        }
      } catch (err) {
        if (state.connected) _emit('onError', err);
      } finally {
        state.reader.releaseLock();
        if (state.connected) _handleDisconnect();
      }
    })();
  }

  /* ── Handshake / Device Verification ── */
  function _doHandshake() {
    /* Collect all incoming data during the timeout window */
    let accumulated = '';
    let resolved    = false;

    const originalOnData = cb.onData;

    function tempListener(bytes, text) {
      accumulated += text;
      if (!resolved && accumulated.includes(cfg.handshakeToken)) {
        resolved = true;
        _finaliseVerify(true, accumulated.trim());
      }
      /* Also forward to the normal handler */
      if (typeof originalOnData === 'function') originalOnData(bytes, text);
    }

    /* Temporarily intercept data */
    cb.onData = tempListener;

    /* Send handshake command */
    writeText(cfg.handshakeCmd);

    /* Timeout */
    setTimeout(() => {
      /* Restore the original handler */
      cb.onData = originalOnData;
      if (!resolved) {
        if (accumulated.length > 0) {
          /* Got something but no token — still flag as unverified */
          _finaliseVerify(false, `收到响应但未匹配设备标识: "${accumulated.trim().slice(0, 60)}"`);
        } else {
          _finaliseVerify(false, '握手超时：设备无响应');
        }
      }
    }, cfg.handshakeTimeout);
  }

  function _finaliseVerify(ok, detail) {
    state.verified  = ok;
    state.deviceInfo = ok ? detail : null;
    if (ok) {
      _emit('onVerified', detail);
    } else {
      _emit('onVerifyFail', detail);
    }
  }

  /* ── Write ── */
  async function writeText(text) {
    if (!state.writer) return;
    const bytes = new TextEncoder().encode(text);
    await state.writer.write(bytes);
    state.txBytes += bytes.length;
  }

  async function writeBytes(bytes) {
    if (!state.writer) return;
    await state.writer.write(bytes);
    state.txBytes += bytes.length;
  }

  /* ── Disconnect ── */
  async function disconnect() {
    state.reading = false;
    try {
      if (state.reader)  { await state.reader.cancel(); }
      if (state.writer)  { await state.writer.close();  }
      if (state.port)    { await state.port.close();    }
    } catch (_) { /* ignore */ }
    _handleDisconnect();
  }

  function _handleDisconnect() {
    state.connected  = false;
    state.verified   = false;
    state.deviceInfo = null;
    state.reader     = null;
    state.writer     = null;
    state.port       = null;
    _emit('onDisconnect');
  }

  /* ── Re-verify on demand ── */
  async function reVerify() {
    if (!state.connected) return;
    state.verified   = false;
    state.deviceInfo = null;
    _doHandshake();
  }

  /* ── Public API ── */
  return {
    state, cfg, cb,
    connect, disconnect, reVerify,
    writeText, writeBytes,
  };
})();
