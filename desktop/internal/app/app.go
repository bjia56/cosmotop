package app

import (
	"context"
	"encoding/base64"
	"errors"
	"fmt"
	"log"
	"strings"
	"sync"
	"time"

	internalruntime "github.com/bjia56/cosmotop/desktop/internal/runtime"
	"github.com/bjia56/cosmotop/desktop/internal/terminal"
	wailsruntime "github.com/wailsapp/wails/v2/pkg/runtime"
)

const (
	terminalDataEventName   = "terminal:data"
	terminalStatusEventName = "terminal:status"
	terminalExitEventName   = "terminal:exit"

	defaultStopTimeout = 3 * time.Second
	maxInputBytes      = 64 * 1024
)

type terminalStatusEvent struct {
	State   string `json:"state"`
	Message string `json:"message,omitempty"`
}

type terminalExitEvent struct {
	Code  int    `json:"code"`
	Error string `json:"error,omitempty"`
}

type App struct {
	ctx context.Context

	mu sync.Mutex

	runtimeInfo internalruntime.ExtractedBinaryInfo
	runtimePath string

	terminal *terminal.Manager
}

func New() *App {
	a := &App{}
	a.terminal = terminal.NewManager(terminal.Callbacks{
		OnOutput: a.onTerminalOutput,
		OnStatus: a.onTerminalStatus,
		OnExit:   a.onTerminalExit,
	})
	return a
}

func (a *App) Startup(ctx context.Context) {
	a.ctx = ctx

	info, err := internalruntime.EnsureExtracted(ctx)
	if err != nil {
		log.Printf("runtime extraction failed: %v", err)
		return
	}

	a.mu.Lock()
	a.runtimeInfo = info
	a.runtimePath = info.Path
	a.mu.Unlock()

	log.Printf("runtime extracted path=%q digest=%s", info.Path, info.Digest)
}

func (a *App) DomReady(ctx context.Context) {
	a.ctx = ctx
}

func (a *App) BeforeClose(ctx context.Context) bool {
	a.ctx = ctx
	if err := a.terminal.Stop(defaultStopTimeout); err != nil {
		log.Printf("failed to stop terminal before close: %v", err)
	}
	return false
}

func (a *App) Shutdown(ctx context.Context) {
	a.ctx = ctx
	if err := a.terminal.Stop(defaultStopTimeout); err != nil {
		log.Printf("failed to stop terminal on shutdown: %v", err)
	}
}

func (a *App) Status() string {
	return "Cosmotop terminal runtime is not wired yet."
}

func (a *App) StartCosmotop(cols int, rows int) error {
	if a.terminal.IsRunning() {
		err := errors.New("cosmotop session is already running")
		a.emitStatus("error", err.Error())
		return err
	}

	runtimePath, err := a.ensureRuntimePath()
	if err != nil {
		a.emitStatus("error", err.Error())
		return err
	}

	if err := a.terminal.Start(runtimePath, cols, rows); err != nil {
		if errors.Is(err, terminal.ErrAlreadyRunning) {
			err = errors.New("cosmotop session is already running")
		}
		a.emitStatus("error", err.Error())
		return err
	}

	return nil
}

func (a *App) WriteInputBase64(data string) error {
	trimmed := strings.TrimSpace(data)
	if trimmed == "" {
		return nil
	}

	decoded, err := base64.StdEncoding.DecodeString(trimmed)
	if err != nil {
		return fmt.Errorf("invalid base64 input: %w", err)
	}
	if len(decoded) > maxInputBytes {
		return fmt.Errorf("input too large: decoded payload %d bytes exceeds %d bytes", len(decoded), maxInputBytes)
	}

	if err := a.terminal.Write(decoded); err != nil {
		return err
	}
	return nil
}

func (a *App) Resize(cols int, rows int) error {
	if err := a.terminal.Resize(cols, rows); err != nil {
		return err
	}
	return nil
}

func (a *App) StopCosmotop() error {
	if err := a.terminal.Stop(defaultStopTimeout); err != nil {
		a.emitStatus("error", err.Error())
		return err
	}
	return nil
}

func (a *App) IsRunning() bool {
	return a.terminal.IsRunning()
}

func (a *App) ensureRuntimePath() (string, error) {
	a.mu.Lock()
	if a.runtimePath != "" {
		path := a.runtimePath
		a.mu.Unlock()
		return path, nil
	}
	a.mu.Unlock()

	ctx := a.ctx
	if ctx == nil {
		ctx = context.Background()
	}

	info, err := internalruntime.EnsureExtracted(ctx)
	if err != nil {
		return "", fmt.Errorf("extract cosmotop runtime: %w", err)
	}

	a.mu.Lock()
	a.runtimeInfo = info
	a.runtimePath = info.Path
	a.mu.Unlock()

	return info.Path, nil
}

func (a *App) onTerminalOutput(output []byte) {
	if len(output) == 0 {
		return
	}
	a.emitEvent(terminalDataEventName, base64.StdEncoding.EncodeToString(output))
}

func (a *App) onTerminalStatus(status terminal.Status) {
	switch status {
	case terminal.StatusStarting:
		a.emitStatus("starting", "")
	case terminal.StatusRunning:
		a.emitStatus("running", "")
	case terminal.StatusStopped:
	case terminal.StatusStopping:
	}
}

func (a *App) onTerminalExit(exitCode int, err error) {
	message := ""
	if err != nil {
		message = err.Error()
		a.emitStatus("error", message)
	} else if exitCode != 0 {
		message = fmt.Sprintf("cosmotop exited with code %d", exitCode)
		a.emitStatus("error", message)
	} else {
		a.emitStatus("stopped", "")
	}

	a.emitEvent(terminalExitEventName, terminalExitEvent{Code: exitCode, Error: message})
}

func (a *App) emitStatus(state string, message string) {
	a.emitEvent(terminalStatusEventName, terminalStatusEvent{State: state, Message: message})
}

func (a *App) emitEvent(name string, data any) {
	if name == "" {
		return
	}
	if a.ctx == nil {
		return
	}
	wailsruntime.EventsEmit(a.ctx, name, data)
}
