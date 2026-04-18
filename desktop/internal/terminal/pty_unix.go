//go:build !windows

package terminal

import (
	"context"
	"fmt"
	"os"
	"os/exec"
	"path/filepath"
	"sync"
	"syscall"

	"github.com/creack/pty"
)

type unixPTYProcess struct {
	cmd  *exec.Cmd
	file *os.File

	closeOnce sync.Once
	waitCh    chan waitResult
}

type waitResult struct {
	exitCode int
	err      error
}

func startPTYProcess(executablePath string, cols, rows int, env []string) (ptyProcess, error) {
	cmd := exec.Command("sh", "-c", "exec \"$1\"", "sh", executablePath)
	cmd.Env = env
	cmd.Dir = filepath.Dir(executablePath)

	ws := &pty.Winsize{Cols: uint16(cols), Rows: uint16(rows)}
	f, err := pty.StartWithSize(cmd, ws)
	if err != nil {
		return nil, fmt.Errorf("start process with PTY: %w", err)
	}

	p := &unixPTYProcess{
		cmd:    cmd,
		file:   f,
		waitCh: make(chan waitResult, 1),
	}

	go func() {
		err := cmd.Wait()
		exitCode := 0
		if cmd.ProcessState != nil {
			exitCode = cmd.ProcessState.ExitCode()
		}
		p.waitCh <- waitResult{exitCode: exitCode, err: err}
		close(p.waitCh)
	}()

	return p, nil
}

func (p *unixPTYProcess) Read(b []byte) (int, error) {
	return p.file.Read(b)
}

func (p *unixPTYProcess) Write(b []byte) (int, error) {
	return p.file.Write(b)
}

func (p *unixPTYProcess) Resize(cols, rows int) error {
	return pty.Setsize(p.file, &pty.Winsize{Cols: uint16(cols), Rows: uint16(rows)})
}

func (p *unixPTYProcess) GracefulStop() error {
	_, _ = p.file.Write([]byte("q"))
	if p.cmd.Process != nil {
		if err := p.cmd.Process.Signal(syscall.SIGINT); err != nil && err != os.ErrProcessDone {
			return err
		}
	}
	return nil
}

func (p *unixPTYProcess) ForceKill() error {
	if p.cmd.Process == nil {
		return nil
	}
	if err := p.cmd.Process.Kill(); err != nil && err != os.ErrProcessDone {
		return err
	}
	return nil
}

func (p *unixPTYProcess) Wait(ctx context.Context) (int, error) {
	select {
	case <-ctx.Done():
		return -1, ctx.Err()
	case result := <-p.waitCh:
		return result.exitCode, result.err
	}
}

func (p *unixPTYProcess) Close() error {
	var err error
	p.closeOnce.Do(func() {
		err = p.file.Close()
	})
	return err
}
