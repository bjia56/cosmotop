package terminal

import (
	"context"
	"errors"
	"fmt"
	"os"
	"strings"
	"sync"
	"time"
)

const (
	defaultCols        = 80
	defaultRows        = 24
	defaultStopTimeout = 3 * time.Second
)

var ErrAlreadyRunning = errors.New("terminal session already running")

type Callbacks struct {
	OnOutput func([]byte)
	OnExit   func(exitCode int, err error)
}

type Manager struct {
	mu sync.Mutex

	callbacks Callbacks
	current   *session
}

type session struct {
	pty ptyProcess

	ctx    context.Context
	cancel context.CancelFunc
	done   chan struct{}
}

var ptyStarter = startPTYProcess

func NewManager(callbacks Callbacks) *Manager {
	return &Manager{callbacks: callbacks}
}

func (m *Manager) IsRunning() bool {
	m.mu.Lock()
	defer m.mu.Unlock()
	return m.current != nil
}

func (m *Manager) Start(executablePath string, args []string, cols, rows int) error {
	if strings.TrimSpace(executablePath) == "" {
		return errors.New("executable path is required")
	}

	if cols <= 0 {
		cols = defaultCols
	}
	if rows <= 0 {
		rows = defaultRows
	}

	m.mu.Lock()
	if m.current != nil {
		m.mu.Unlock()
		return ErrAlreadyRunning
	}
	m.mu.Unlock()

	env := withTerminalEnv(os.Environ())
	proc, err := ptyStarter(executablePath, args, cols, rows, env)
	if err != nil {
		return fmt.Errorf("start PTY session: %w", err)
	}

	ctx, cancel := context.WithCancel(context.Background())
	s := &session{
		pty:    proc,
		ctx:    ctx,
		cancel: cancel,
		done:   make(chan struct{}),
	}

	m.mu.Lock()
	if m.current != nil {
		m.mu.Unlock()
		cancel()
		_ = proc.Close()
		return ErrAlreadyRunning
	}
	m.current = s
	m.mu.Unlock()

	go m.readLoop(s)
	go m.waitLoop(s)

	return nil
}

func (m *Manager) Write(input []byte) error {
	m.mu.Lock()
	s := m.current
	m.mu.Unlock()

	if s == nil {
		return errors.New("terminal session is not running")
	}

	if len(input) == 0 {
		return nil
	}

	_, err := s.pty.Write(input)
	if err != nil {
		return fmt.Errorf("write PTY input: %w", err)
	}
	return nil
}

func (m *Manager) Resize(cols, rows int) error {
	if cols <= 0 || rows <= 0 {
		return nil
	}

	m.mu.Lock()
	s := m.current
	m.mu.Unlock()
	if s == nil {
		return nil
	}

	if err := s.pty.Resize(cols, rows); err != nil {
		return fmt.Errorf("resize PTY: %w", err)
	}
	return nil
}

func (m *Manager) Kill(timeout time.Duration) error {
	m.mu.Lock()
	s := m.current
	m.mu.Unlock()

	if s == nil {
		return nil
	}

	if timeout <= 0 {
		timeout = defaultStopTimeout
	}

	if err := s.pty.ForceKill(); err != nil {
		return fmt.Errorf("force kill terminal session: %w", err)
	}

	select {
	case <-s.done:
		return nil
	case <-time.After(timeout):
		return fmt.Errorf("terminal session did not stop within %s", timeout)
	}
}

func (m *Manager) readLoop(s *session) {
	buf := make([]byte, 8192)
	for {
		if err := s.ctx.Err(); err != nil {
			return
		}

		n, err := s.pty.Read(buf)
		if n > 0 {
			chunk := make([]byte, n)
			copy(chunk, buf[:n])
			m.emitOutput(chunk)
		}

		if err != nil {
			return
		}
	}
}

func (m *Manager) waitLoop(s *session) {
	defer close(s.done)

	exitCode, waitErr := s.pty.Wait(context.Background())
	s.cancel()
	_ = s.pty.Close()

	m.mu.Lock()
	if m.current == s {
		m.current = nil
	}
	m.mu.Unlock()

	m.emitExit(exitCode, waitErr)
}

func (m *Manager) emitOutput(output []byte) {
	if len(output) == 0 {
		return
	}
	if m.callbacks.OnOutput != nil {
		m.callbacks.OnOutput(output)
	}
}

func (m *Manager) emitExit(exitCode int, err error) {
	if m.callbacks.OnExit != nil {
		m.callbacks.OnExit(exitCode, err)
	}
}

func withTerminalEnv(base []string) []string {
	env := make([]string, 0, len(base)+2)
	for _, entry := range base {
		if hasEnvKey(entry, "TERM") || hasEnvKey(entry, "COLORTERM") {
			continue
		}
		env = append(env, entry)
	}

	env = append(env, "TERM=xterm-256color")
	env = append(env, "COLORTERM=truecolor")
	return env
}

func hasEnvKey(entry, key string) bool {
	i := strings.IndexByte(entry, '=')
	if i <= 0 {
		return false
	}
	return strings.EqualFold(entry[:i], key)
}
