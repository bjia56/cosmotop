import "xterm/css/xterm.css";
import "./style.css";

import { Terminal } from "xterm";
import { FitAddon } from "xterm-addon-fit";
import {
  IsRunning,
  Resize,
  StartCosmotop,
  StopCosmotop,
  WriteInputBase64,
} from "../wailsjs/go/app/App";
import { EventsOff, EventsOn } from "../wailsjs/runtime/runtime";

const RESIZE_DEBOUNCE_MS = 120;
const APP_CLEANUP_KEY = "__cosmotopDesktopCleanup";

function toBase64(value) {
  const bytes = new TextEncoder().encode(value);
  let binary = "";
  for (let i = 0; i < bytes.length; i += 1) {
    binary += String.fromCharCode(bytes[i]);
  }
  return btoa(binary);
}

function fromBase64(value) {
  const binary = atob(value);
  const bytes = new Uint8Array(binary.length);
  for (let i = 0; i < binary.length; i += 1) {
    bytes[i] = binary.charCodeAt(i);
  }
  return bytes;
}

function createApp() {
  const root = document.getElementById("app");
  if (!root) {
    throw new Error("Missing #app root element.");
  }

  root.innerHTML = `
    <main class="desktop-shell">
      <header class="toolbar">
        <div class="title-group">
          <h1>Cosmotop Desktop</h1>
          <span id="status-indicator" class="status-pill is-idle">idle</span>
        </div>
        <div class="controls">
          <button id="restart-btn" type="button">Restart</button>
          <button id="stop-btn" type="button">Stop</button>
        </div>
      </header>
      <p id="error-banner" class="error-banner" hidden></p>
      <section id="terminal-panel" class="terminal-panel">
        <div id="terminal" class="terminal-host" aria-label="Cosmotop terminal"></div>
      </section>
    </main>
  `;

  if (!window.runtime) {
    throw new Error("Wails runtime event APIs are unavailable.");
  }
  const statusIndicator = root.querySelector("#status-indicator");
  const errorBanner = root.querySelector("#error-banner");
  const terminalPanel = root.querySelector("#terminal-panel");
  const terminalHost = root.querySelector("#terminal");
  const restartButton = root.querySelector("#restart-btn");
  const stopButton = root.querySelector("#stop-btn");

  if (
    !statusIndicator ||
    !errorBanner ||
    !terminalPanel ||
    !terminalHost ||
    !restartButton ||
    !stopButton
  ) {
    throw new Error("Terminal UI failed to initialize.");
  }

  const terminal = new Terminal({
    allowProposedApi: false,
    cursorBlink: true,
    convertEol: true,
    fontFamily: "JetBrains Mono, Cascadia Code, Menlo, Consolas, monospace",
    fontSize: 13,
    scrollback: 5000,
    theme: {
      background: "#11111a",
      foreground: "#f4f2ee",
      cursor: "#ffd386",
      selectionBackground: "rgba(106, 127, 255, 0.38)",
      black: "#191924",
      red: "#ff7f7f",
      green: "#95d483",
      yellow: "#ffdf80",
      blue: "#81a3ff",
      magenta: "#d9a6ff",
      cyan: "#86d8f7",
      white: "#e7e5e1",
      brightBlack: "#4f5068",
      brightRed: "#ffb2b2",
      brightGreen: "#b9edac",
      brightYellow: "#ffe8ad",
      brightBlue: "#abc1ff",
      brightMagenta: "#eac9ff",
      brightCyan: "#b6ecff",
      brightWhite: "#ffffff",
    },
  });
  const fitAddon = new FitAddon();

  terminal.loadAddon(fitAddon);
  terminal.open(terminalHost);

  const cleanups = [];
  let resizeTimer = null;

  function setStatus(state, message = "") {
    const status = state || "idle";
    statusIndicator.textContent = status;
    statusIndicator.className = `status-pill is-${status}`;

    if (message) {
      errorBanner.hidden = false;
      errorBanner.textContent = message;
    } else if (status !== "error") {
      errorBanner.hidden = true;
      errorBanner.textContent = "";
    }
  }

  function showError(value) {
    const message = value instanceof Error ? value.message : String(value);
    setStatus("error", message);
  }

  function fitAndGetSize() {
    fitAddon.fit();
    const { cols, rows } = terminal;
    return { cols, rows };
  }

  async function syncResizeToBackend() {
    const { cols, rows } = fitAndGetSize();
    if (cols > 0 && rows > 0) {
      await Resize(cols, rows);
    }
    return { cols, rows };
  }

  function scheduleResizeSync() {
    if (resizeTimer) {
      window.clearTimeout(resizeTimer);
    }
    resizeTimer = window.setTimeout(() => {
      syncResizeToBackend().catch(showError);
    }, RESIZE_DEBOUNCE_MS);
  }

  function subscribe(eventName, callback) {
    EventsOn(eventName, callback);
    return () => {
      EventsOff(eventName);
    };
  }

  cleanups.push(
    subscribe("terminal:data", (chunk) => {
      if (typeof chunk !== "string" || chunk.length === 0) {
        return;
      }
      try {
        terminal.write(fromBase64(chunk));
      } catch (error) {
        showError(error);
      }
    }),
  );

  cleanups.push(
    subscribe("terminal:status", (payload = {}) => {
      const state = typeof payload.state === "string" ? payload.state : "idle";
      const message = typeof payload.message === "string" ? payload.message : "";
      setStatus(state, message);
    }),
  );

  cleanups.push(
    subscribe("terminal:exit", (payload = {}) => {
      const code = Number.isInteger(payload.code) ? payload.code : 0;
      const message = typeof payload.error === "string" ? payload.error : "";
      if (message) {
        showError(message);
        return;
      }
      if (code === 0) {
        setStatus("stopped");
      } else {
        showError(`cosmotop exited with code ${code}`);
      }
    }),
  );

  const inputDisposable = terminal.onData((data) => {
    if (!data) {
      return;
    }
    WriteInputBase64(toBase64(data)).catch(showError);
  });
  cleanups.push(() => inputDisposable.dispose());

  const resizeObserver = new ResizeObserver(() => {
    scheduleResizeSync();
  });
  resizeObserver.observe(terminalPanel);
  cleanups.push(() => resizeObserver.disconnect());

  const windowResizeHandler = () => scheduleResizeSync();
  window.addEventListener("resize", windowResizeHandler);
  cleanups.push(() => window.removeEventListener("resize", windowResizeHandler));

  const stopHandler = () => {
    StopCosmotop().catch(showError);
  };
  stopButton.addEventListener("click", stopHandler);
  cleanups.push(() => stopButton.removeEventListener("click", stopHandler));

  const startSession = async () => {
    setStatus("starting");
    const { cols, rows } = fitAndGetSize();
    await StartCosmotop(cols, rows);
  };

  const restartHandler = async () => {
    setStatus("starting");
    await StopCosmotop();
    await startSession();
  };
  const restartClickHandler = () => {
    restartHandler().catch(showError);
  };
  restartButton.addEventListener("click", restartClickHandler);
  cleanups.push(() => restartButton.removeEventListener("click", restartClickHandler));

  terminal.focus();

  (async () => {
    try {
      const isRunning = await IsRunning();
      if (isRunning) {
        setStatus("running");
        await syncResizeToBackend();
      } else {
        await startSession();
      }
    } catch (error) {
      showError(error);
    }
  })();

  return () => {
    if (resizeTimer) {
      window.clearTimeout(resizeTimer);
      resizeTimer = null;
    }
    while (cleanups.length > 0) {
      const cleanup = cleanups.pop();
      cleanup();
    }
    terminal.dispose();
  };
}

if (typeof window[APP_CLEANUP_KEY] === "function") {
  window[APP_CLEANUP_KEY]();
}

const cleanup = createApp();
window[APP_CLEANUP_KEY] = cleanup;

if (import.meta.hot) {
  import.meta.hot.dispose(() => {
    if (typeof window[APP_CLEANUP_KEY] === "function") {
      window[APP_CLEANUP_KEY]();
    }
    window[APP_CLEANUP_KEY] = null;
  });
}
