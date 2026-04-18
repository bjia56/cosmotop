import "@xterm/xterm/css/xterm.css";
import "./style.css";

import { Terminal } from "@xterm/xterm";
import { FitAddon } from "@xterm/addon-fit";
import {
  IsRunning,
  Resize,
  StartCosmotop,
  WriteInputBase64,
} from "../wailsjs/go/app/App";
import { EventsOff, EventsOn } from "../wailsjs/runtime/runtime";

const RESIZE_DEBOUNCE_MS = 120;
const APP_CLEANUP_KEY = "__cosmotopDesktopCleanup";

function toErrorMessage(err) {
  if (err instanceof Error && err.message) {
    return err.message;
  }
  return String(err);
}

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

function extractChunkFromEventArgs(args) {
  if (args.length === 1) {
    const only = args[0];
    if (typeof only === "string") {
      return only;
    }
    if (Array.isArray(only) && only.length > 0 && typeof only[0] === "string") {
      return only[0];
    }
    if (only && typeof only === "object" && typeof only.data === "string") {
      return only.data;
    }
    return "";
  }
  if (args.length > 1 && typeof args[0] === "string") {
    return args[0];
  }
  return "";
}

function createApp() {
  const root = document.getElementById("app");
  if (!root) {
    throw new Error("Missing #app root element.");
  }

  root.innerHTML = '<div id="terminal" class="terminal-host" aria-label="Cosmotop terminal"></div>';

  if (!window.runtime) {
    throw new Error("Wails runtime event APIs are unavailable.");
  }

  const terminalHost = root.querySelector("#terminal");
  if (!terminalHost) {
    throw new Error("Terminal UI failed to initialize.");
  }

  const terminal = new Terminal({
    allowProposedApi: false,
    fontFamily: "JetBrains Mono, Cascadia Code, Menlo, Consolas, monospace",
    fontSize: 13,
    scrollback: 0,
  });
  const fitAddon = new FitAddon();

  terminal.loadAddon(fitAddon);
  terminal.open(terminalHost);

  const cleanups = [];
  let resizeTimer = null;

  function writeNotice(message) {
    terminal.write(`\r\n[cosmotop-desktop] ${message}\r\n`);
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
      syncResizeToBackend().catch((err) => {
        const message = toErrorMessage(err);
        console.error(message);
        writeNotice(message);
      });
    }, RESIZE_DEBOUNCE_MS);
  }

  EventsOn("terminal:data", (...args) => {
    const chunk = extractChunkFromEventArgs(args);
    if (chunk.length === 0) {
      return;
    }
    try {
      terminal.write(fromBase64(chunk));
    } catch (err) {
      const message = `failed to decode terminal chunk: ${toErrorMessage(err)}`;
      console.error(message);
      writeNotice(message);
    }
  });
  cleanups.push(() => EventsOff("terminal:data"));

  const inputDisposable = terminal.onData((data) => {
    if (!data) {
      return;
    }
    WriteInputBase64(toBase64(data)).catch((err) => {
      const message = `input write failed: ${toErrorMessage(err)}`;
      console.error(message);
      writeNotice(message);
    });
  });
  cleanups.push(() => inputDisposable.dispose());

  const resizeObserver = new ResizeObserver(() => {
    scheduleResizeSync();
  });
  resizeObserver.observe(terminalHost);
  cleanups.push(() => resizeObserver.disconnect());

  const windowResizeHandler = () => scheduleResizeSync();
  window.addEventListener("resize", windowResizeHandler);
  cleanups.push(() => window.removeEventListener("resize", windowResizeHandler));

  terminal.focus();

  (async () => {
    try {
      const isRunning = await IsRunning();
      if (isRunning) {
        await syncResizeToBackend();
      } else {
        const { cols, rows } = fitAndGetSize();
        await StartCosmotop(cols, rows);
      }
    } catch (err) {
      const message = `failed to start cosmotop: ${toErrorMessage(err)}`;
      console.error(message);
      writeNotice(message);
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
