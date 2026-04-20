//go:build windows

package terminal

import (
	"context"
	"fmt"
	"os/exec"
	"path/filepath"
	"strconv"
	"strings"
	"sync"

	"github.com/UserExistsError/conpty"
	internalruntime "github.com/bjia56/cosmotop/desktop/internal/runtime"
)

type windowsPTYProcess struct {
	cpty *conpty.ConPty

	closeOnce sync.Once
	closeErr  error
}

func startPTYProcess(executablePath string, args []string, cols, rows int, env []string) (ptyProcess, error) {
	workDir := filepath.Dir(executablePath)
	commandLine := internalruntime.WindowsCommandLine(executablePath, args)

	cpty, err := conpty.Start(
		strings.Join(commandLine, " "),
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

func (p *windowsPTYProcess) ForceKill() error {
	pid := p.cpty.Pid()
	if pid > 0 {
		_ = exec.Command("taskkill", "/PID", strconv.Itoa(pid), "/T", "/F").Run()
	}
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
