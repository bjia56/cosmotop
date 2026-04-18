//go:build windows

package terminal

import (
	"context"
	"fmt"
	"path/filepath"
	"strconv"
	"sync"

	"github.com/UserExistsError/conpty"
)

type windowsPTYProcess struct {
	cpty *conpty.ConPty

	closeOnce sync.Once
	closeErr  error
}

func startPTYProcess(executablePath string, cols, rows int, env []string) (ptyProcess, error) {
	workDir := filepath.Dir(executablePath)
	cpty, err := conpty.Start(
		strconv.Quote(executablePath),
		conpty.ConPtyDimensions(cols, rows),
		conpty.ConPtyWorkDir(workDir),
		conpty.ConPtyEnv(env),
	)
	if err != nil {
		return nil, fmt.Errorf("start process with ConPTY: %w", err)
	}

	return &windowsPTYProcess{cpty: cpty}, nil
}

func (p *windowsPTYProcess) Read(b []byte) (int, error) {
	return p.cpty.Read(b)
}

func (p *windowsPTYProcess) Write(b []byte) (int, error) {
	return p.cpty.Write(b)
}

func (p *windowsPTYProcess) Resize(cols, rows int) error {
	return p.cpty.Resize(cols, rows)
}

func (p *windowsPTYProcess) GracefulStop() error {
	_, _ = p.cpty.Write([]byte("q"))
	_, _ = p.cpty.Write([]byte("\x03"))
	_, _ = p.cpty.Write([]byte("\r"))
	return nil
}

func (p *windowsPTYProcess) ForceKill() error {
	return p.Close()
}

func (p *windowsPTYProcess) Wait(ctx context.Context) (int, error) {
	exitCode, err := p.cpty.Wait(ctx)
	return int(exitCode), err
}

func (p *windowsPTYProcess) Close() error {
	p.closeOnce.Do(func() {
		p.closeErr = p.cpty.Close()
	})
	return p.closeErr
}
