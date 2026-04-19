package terminal

import (
	"context"
	"errors"
	"io"
	"sync"
	"testing"
	"time"
)

func TestManagerLifecycle(t *testing.T) {
	origStarter := ptyStarter
	defer func() { ptyStarter = origStarter }()

	fake := newFakePTY()
	fake.onForceKill = func() { fake.finish(0, nil) }
	ptyStarter = func(executablePath string, args []string, cols, rows int, env []string) (ptyProcess, error) {
		if executablePath != "/tmp/cosmotop" {
			t.Fatalf("executablePath = %q, want %q", executablePath, "/tmp/cosmotop")
		}
		if len(args) != 2 || args[0] != "-p" || args[1] != "1" {
			t.Fatalf("args = %v, want [-p 1]", args)
		}
		if cols != 100 || rows != 40 {
			t.Fatalf("size = %dx%d, want 100x40", cols, rows)
		}
		assertHasEnv(t, env, "TERM=xterm-256color")
		assertHasEnv(t, env, "COLORTERM=truecolor")
		return fake, nil
	}

	var (
		outputCh = make(chan []byte, 1)
		exitCh   = make(chan struct {
			code int
			err  error
		}, 1)
	)

	mgr := NewManager(Callbacks{
		OnOutput: func(b []byte) {
			outputCh <- b
		},
		OnExit: func(exitCode int, err error) {
			exitCh <- struct {
				code int
				err  error
			}{code: exitCode, err: err}
		},
	})

	if err := mgr.Start("/tmp/cosmotop", []string{"-p", "1"}, 100, 40); err != nil {
		t.Fatalf("Start() error = %v", err)
	}
	if !mgr.IsRunning() {
		t.Fatal("IsRunning() = false, want true")
	}

	fake.pushOutput([]byte("hello"))
	select {
	case out := <-outputCh:
		if string(out) != "hello" {
			t.Fatalf("output = %q, want %q", string(out), "hello")
		}
	case <-time.After(time.Second):
		t.Fatal("timed out waiting for output callback")
	}

	if err := mgr.Write([]byte("input")); err != nil {
		t.Fatalf("Write() error = %v", err)
	}
	if err := mgr.Resize(120, 50); err != nil {
		t.Fatalf("Resize() error = %v", err)
	}

	if err := mgr.Kill(200 * time.Millisecond); err != nil {
		t.Fatalf("Kill() error = %v", err)
	}

	select {
	case got := <-exitCh:
		if got.code != 0 || got.err != nil {
			t.Fatalf("exit = (%d, %v), want (0, nil)", got.code, got.err)
		}
	case <-time.After(time.Second):
		t.Fatal("timed out waiting for exit callback")
	}

	if mgr.IsRunning() {
		t.Fatal("IsRunning() = true, want false")
	}

	fake.mu.Lock()
	defer fake.mu.Unlock()
	if fake.forceKillCalls == 0 {
		t.Fatalf("ForceKill() calls = %d, want >= 1", fake.forceKillCalls)
	}
	if len(fake.writes) == 0 || string(fake.writes[0]) != "input" {
		t.Fatalf("writes = %q, want first write %q", fake.writes, "input")
	}
	if len(fake.resizes) == 0 || fake.resizes[0] != [2]int{120, 50} {
		t.Fatalf("resizes = %v, want first resize [120 50]", fake.resizes)
	}
}

func TestManagerDuplicateStart(t *testing.T) {
	origStarter := ptyStarter
	defer func() { ptyStarter = origStarter }()

	fake := newFakePTY()
	ptyStarter = func(string, []string, int, int, []string) (ptyProcess, error) { return fake, nil }

	mgr := NewManager(Callbacks{})
	if err := mgr.Start("/tmp/cosmotop", nil, 80, 24); err != nil {
		t.Fatalf("first Start() error = %v", err)
	}

	err := mgr.Start("/tmp/cosmotop", nil, 80, 24)
	if !errors.Is(err, ErrAlreadyRunning) {
		t.Fatalf("second Start() error = %v, want ErrAlreadyRunning", err)
	}

	fake.finish(0, nil)
	if err := mgr.Kill(100 * time.Millisecond); err != nil {
		t.Fatalf("Kill() error = %v", err)
	}
}

func TestManagerKillForceKillAfterTimeout(t *testing.T) {
	origStarter := ptyStarter
	defer func() { ptyStarter = origStarter }()

	fake := newFakePTY()
	fake.onForceKill = func() { fake.finish(137, nil) }
	ptyStarter = func(string, []string, int, int, []string) (ptyProcess, error) { return fake, nil }

	mgr := NewManager(Callbacks{})
	if err := mgr.Start("/tmp/cosmotop", nil, 80, 24); err != nil {
		t.Fatalf("Start() error = %v", err)
	}

	if err := mgr.Kill(10 * time.Millisecond); err != nil {
		t.Fatalf("Kill() error = %v", err)
	}

	fake.mu.Lock()
	defer fake.mu.Unlock()
	if fake.forceKillCalls == 0 {
		t.Fatal("ForceKill() calls = 0, want >= 1")
	}
}

func TestManagerStartFailureLeavesNotRunning(t *testing.T) {
	origStarter := ptyStarter
	defer func() { ptyStarter = origStarter }()

	startErr := errors.New("boom")
	ptyStarter = func(string, []string, int, int, []string) (ptyProcess, error) { return nil, startErr }

	mgr := NewManager(Callbacks{})

	err := mgr.Start("/tmp/cosmotop", nil, 80, 24)
	if err == nil {
		t.Fatal("Start() error = nil, want failure")
	}
	if mgr.IsRunning() {
		t.Fatal("IsRunning() = true, want false")
	}
}

func TestManagerKillProcessExitsNaturallyDuringKill(t *testing.T) {
	origStarter := ptyStarter
	defer func() { ptyStarter = origStarter }()

	fake := newFakePTY()
	ptyStarter = func(string, []string, int, int, []string) (ptyProcess, error) { return fake, nil }

	var (
		exitMu sync.Mutex
		exits  int
		exitCh = make(chan struct{}, 1)
	)

	mgr := NewManager(Callbacks{
		OnExit: func(exitCode int, err error) {
			if exitCode != 17 || err != nil {
				t.Errorf("OnExit(%d, %v), want (17, nil)", exitCode, err)
			}
			exitMu.Lock()
			exits++
			exitMu.Unlock()
			exitCh <- struct{}{}
		},
	})

	if err := mgr.Start("/tmp/cosmotop", nil, 80, 24); err != nil {
		t.Fatalf("Start() error = %v", err)
	}

	done := make(chan error, 1)
	go func() {
		done <- mgr.Kill(250 * time.Millisecond)
	}()

	time.Sleep(20 * time.Millisecond)
	fake.finish(17, nil)

	select {
	case err := <-done:
		if err != nil {
			t.Fatalf("Kill() error = %v", err)
		}
	case <-time.After(time.Second):
		t.Fatal("timed out waiting for Kill()")
	}

	select {
	case <-exitCh:
	case <-time.After(time.Second):
		t.Fatal("timed out waiting for OnExit callback")
	}

	time.Sleep(20 * time.Millisecond)

	exitMu.Lock()
	defer exitMu.Unlock()
	if exits != 1 {
		t.Fatalf("OnExit callback count = %d, want 1", exits)
	}

	if mgr.IsRunning() {
		t.Fatal("IsRunning() = true, want false")
	}
}

func TestManagerKillForceKillErrorStillEmitsExitOnce(t *testing.T) {
	origStarter := ptyStarter
	defer func() { ptyStarter = origStarter }()

	fake := newFakePTY()
	fake.forceKillErr = errors.New("force kill failed")
	ptyStarter = func(string, []string, int, int, []string) (ptyProcess, error) { return fake, nil }

	var (
		exitMu sync.Mutex
		exits  int
		exitCh = make(chan struct{}, 1)
	)

	mgr := NewManager(Callbacks{
		OnExit: func(exitCode int, err error) {
			if exitCode != 23 || err != nil {
				t.Errorf("OnExit(%d, %v), want (23, nil)", exitCode, err)
			}
			exitMu.Lock()
			exits++
			exitMu.Unlock()
			exitCh <- struct{}{}
		},
	})

	if err := mgr.Start("/tmp/cosmotop", nil, 80, 24); err != nil {
		t.Fatalf("Start() error = %v", err)
	}

	errCh := make(chan error, 1)
	go func() {
		errCh <- mgr.Kill(10 * time.Millisecond)
	}()

	time.Sleep(25 * time.Millisecond)
	fake.finish(23, nil)

	select {
	case err := <-errCh:
		if err == nil || err.Error() != "force kill terminal session: force kill failed" {
			t.Fatalf("Kill() error = %v, want force kill terminal session error", err)
		}
	case <-time.After(time.Second):
		t.Fatal("timed out waiting for Kill() error")
	}

	select {
	case <-exitCh:
	case <-time.After(time.Second):
		t.Fatal("timed out waiting for OnExit callback")
	}

	time.Sleep(20 * time.Millisecond)

	exitMu.Lock()
	defer exitMu.Unlock()
	if exits != 1 {
		t.Fatalf("OnExit callback count = %d, want 1", exits)
	}

	if mgr.IsRunning() {
		t.Fatal("IsRunning() = true, want false")
	}
}

func TestWithTerminalEnvOverridesTerminalVars(t *testing.T) {
	env := withTerminalEnv([]string{"PATH=/bin", "TERM=vt100", "COLORTERM=24bit"})

	assertHasEnv(t, env, "PATH=/bin")
	assertHasEnv(t, env, "TERM=xterm-256color")
	assertHasEnv(t, env, "COLORTERM=truecolor")
}

func assertHasEnv(t *testing.T, env []string, want string) {
	t.Helper()
	for _, entry := range env {
		if entry == want {
			return
		}
	}
	t.Fatalf("env missing %q in %v", want, env)
}

type fakePTY struct {
	mu sync.Mutex

	outputCh chan []byte
	waitCh   chan fakeWait

	finishOnce sync.Once
	closeOnce  sync.Once

	writes  []string
	resizes [][2]int

	forceKillCalls int

	onForceKill func()

	forceKillErr error
}

type fakeWait struct {
	exitCode int
	err      error
}

func newFakePTY() *fakePTY {
	return &fakePTY{
		outputCh: make(chan []byte, 8),
		waitCh:   make(chan fakeWait, 1),
	}
}

func (f *fakePTY) Read(p []byte) (int, error) {
	b, ok := <-f.outputCh
	if !ok {
		return 0, io.EOF
	}
	n := copy(p, b)
	return n, nil
}

func (f *fakePTY) Write(p []byte) (int, error) {
	f.mu.Lock()
	f.writes = append(f.writes, string(append([]byte(nil), p...)))
	f.mu.Unlock()
	return len(p), nil
}

func (f *fakePTY) Resize(cols, rows int) error {
	f.mu.Lock()
	f.resizes = append(f.resizes, [2]int{cols, rows})
	f.mu.Unlock()
	return nil
}

func (f *fakePTY) ForceKill() error {
	f.mu.Lock()
	f.forceKillCalls++
	hook := f.onForceKill
	err := f.forceKillErr
	f.mu.Unlock()
	if hook != nil {
		hook()
	}
	return err
}

func (f *fakePTY) Wait(ctx context.Context) (int, error) {
	select {
	case <-ctx.Done():
		return -1, ctx.Err()
	case result := <-f.waitCh:
		return result.exitCode, result.err
	}
}

func (f *fakePTY) Close() error {
	f.closeOnce.Do(func() {
		close(f.outputCh)
	})
	return nil
}

func (f *fakePTY) pushOutput(b []byte) {
	f.outputCh <- append([]byte(nil), b...)
}

func (f *fakePTY) finish(exitCode int, err error) {
	f.finishOnce.Do(func() {
		f.waitCh <- fakeWait{exitCode: exitCode, err: err}
		close(f.waitCh)
	})
}
