# Cosmotop Desktop

This directory contains the Wails desktop wrapper for `cosmotop`.

## Architecture overview

The desktop app is a two-part system:

- **Backend (Go + Wails):** `desktop/main.go` boots Wails and binds `internal/app.App` into the frontend runtime.
- **Frontend (Vite + xterm.js):** `desktop/frontend/src/main.js` renders the window, terminal, toolbar, and event wiring.

### Runtime bundle and extraction

The desktop binary embeds a prebuilt `cosmotop` runtime artifact:

- Embedded file (unix): `desktop/internal/bundle/data/cosmotop`
- Embedded file (windows): `desktop/internal/bundle/data/cosmotop.cmd`
- Embed code:
  - `desktop/internal/bundle/embed_unix.go` via `//go:embed data/cosmotop`
  - `desktop/internal/bundle/embed_windows.go` via `//go:embed data/cosmotop.cmd`
- Digest source: SHA-256 computed from embedded bytes at app startup/runtime use

At startup (and lazily on demand), backend extraction logic in `desktop/internal/runtime/extractor.go`:

1. Computes the expected digest from embedded bytes.
2. Resolves cache root with `os.UserCacheDir()`.
3. Extracts to a digest-versioned path:
   - Unix: `<user-cache>/cosmotop-desktop/runtime/<sha256>/cosmotop`
   - Windows: `<user-cache>/cosmotop-desktop/runtime/<sha256>/cosmotop.cmd`
4. Verifies digest of an existing file and reuses it if valid.
5. Otherwise writes a temp file, marks executable on non-Windows, atomically renames, and re-verifies digest.

This gives deterministic runtime versioning, avoids in-place mutation of a shared filename across versions, and allows safe reuse across app launches.

### PTY bridge

`internal/terminal.Manager` owns a single live cosmotop session:

- Unix PTY backend: `desktop/internal/terminal/pty_unix.go` (`github.com/creack/pty`)
- Windows PTY backend: `desktop/internal/terminal/pty_windows.go` (`github.com/UserExistsError/conpty`)

Process behavior:

- Start command is the extracted `cosmotop` path (working directory set to its directory).
- Input from frontend is base64-decoded in backend and written to PTY.
- Output from PTY is base64-encoded in backend and emitted to frontend.
- Resize events from frontend are forwarded to PTY size updates.
- Stop uses graceful shutdown first, then force-kill after timeout.

### Event protocol

Backend emits Wails runtime events; frontend subscribes with `runtime.EventsOn(...)`.

- Event `terminal:data`
  - Payload type: `string`
  - Meaning: base64-encoded PTY output chunk
- Event `terminal:status`
  - Payload type: object
  - Fields:
    - `state: string`
    - `message?: string`
  - Common states from backend: `starting`, `running`, `stopping`, `stopped`, `error`
- Event `terminal:exit`
  - Payload type: object
  - Fields:
    - `code: number`
    - `error?: string`

## Backend API contract (exact binding names)

Frontend calls Go-bound methods from `window.go.app.App` (fallback `window.go.main.App`):

- `StartCosmotop(cols, rows)`
  - Starts a session using extracted runtime path
  - Error if already running or runtime extraction/start fails
- `WriteInputBase64(data)`
  - Decodes base64 input and writes to PTY
  - Max decoded payload is 64 KiB per call
- `Resize(cols, rows)`
  - Resizes running PTY
- `StopCosmotop()`
  - Stops session gracefully with timeout fallback
- `IsRunning()`
  - Returns current session state
- `Status()`
  - Legacy placeholder status string

## Local development setup

### Prerequisites

- Go `1.24` (matches `desktop/go.mod`)
- Node.js `22` (matches CI)
- Wails CLI `v2.10.2`
- Platform prerequisites for Wails WebView runtime
  - Linux build hosts also need GTK/WebKit dev packages, e.g.:
    - `libgtk-3-dev`
    - `libwebkit2gtk-4.1-dev`

Install Wails CLI:

```bash
go install github.com/wailsapp/wails/v2/cmd/wails@v2.10.2
```

### Place prebuilt `cosmotop` runtime artifact

The embedded runtime file must exist before backend build/test:

- Unix builds: `desktop/internal/bundle/data/cosmotop`
- Windows builds: `desktop/internal/bundle/data/cosmotop.cmd`

Example:

```bash
cp /path/to/prebuilt/cosmotop desktop/internal/bundle/data/cosmotop
chmod +x desktop/internal/bundle/data/cosmotop

# Windows build hosts should place runtime at:
# desktop/internal/bundle/data/cosmotop.cmd
```

### Install frontend dependencies

```bash
npm install --prefix desktop/frontend
```

### Run desktop app in development mode

```bash
wails dev
```

Run from `desktop/`.

### Build desktop app locally

```bash
wails build -clean -platform linux/amd64
```

Run from `desktop/` and replace platform as needed (`darwin/universal`, `windows/amd64`, etc).

### Run backend tests

```bash
go test ./...
```

Run from `desktop/`.

## CI artifact flow

Current flow in `.github/workflows/build.yml`:

1. `bundle` job builds and uploads artifact named `cosmotop`.
2. `desktop_build` job declares `needs: bundle`.
3. `desktop_build` downloads artifact `cosmotop` into `./desktop/internal/bundle/data`.
4. Workflow verifies `./desktop/internal/bundle/data/cosmotop` exists.
5. Workflow copies the artifact to platform-specific embed filename:
   - unix target: `cosmotop`
   - windows target: `cosmotop.cmd`
6. Wails build runs in `./desktop`.
7. Built desktop app binaries are uploaded as:
   - `desktop-linux-amd64`
   - `desktop-macos-universal`
   - `desktop-windows-amd64`

Key invariant: desktop build must provide the platform-specific embed filename (`cosmotop` on unix, `cosmotop.cmd` on windows).

## Troubleshooting

- **Build fails with embed/missing file errors**
  - Ensure platform-specific runtime exists before `go test` or `wails build`:
    - unix: `desktop/internal/bundle/data/cosmotop`
    - windows: `desktop/internal/bundle/data/cosmotop.cmd`
- **Desktop starts but terminal shows error immediately**
  - Check execution bit/permissions on extracted runtime and source bundle (`chmod +x .../cosmotop` on Unix).
- **Frontend errors like missing Wails bindings/runtime**
  - Launch via `wails dev` or built desktop binary, not by directly opening `frontend/dist/index.html`.
- **Linux build failures for WebKit/GTK packages**
  - Install `libgtk-3-dev` and `libwebkit2gtk-4.1-dev`.
- **`cosmotop session is already running`**
  - Wait for `terminal:exit`/`stopped` state before retrying start, or call `StopCosmotop()` first.

## Security and operational notes

- Runtime extraction location is user cache scoped (`os.UserCacheDir`), under digest-versioned subdirectories.
- Extracted directory is created with `0700`; non-Windows extracted binary is chmodded `0700`.
- Digest verification occurs before reuse and after write, with rewrite on mismatch/corruption.
- No auto-update channel exists in desktop runtime management.
  - The desktop app runs exactly the embedded runtime bytes (`cosmotop` on unix, `cosmotop.cmd` on windows).
  - Updating runtime means rebuilding desktop app with a new embedded artifact.
