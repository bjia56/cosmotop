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
)

type windowsPTYProcess struct {
	cpty *conpty.ConPty

	closeOnce sync.Once
	closeErr  error
}

func startPTYProcess(executablePath string, args []string, cols, rows int, env []string) (ptyProcess, error) {
	workDir := filepath.Dir(executablePath)
	commandLine := buildWindowsCommandLine(executablePath, args)

	cpty, err := conpty.Start(
		commandLine,
		conpty.ConPtyDimensions(cols, rows),
		conpty.ConPtyWorkDir(workDir),
		conpty.ConPtyEnv(env),
	)
	if err != nil {
		return nil, fmt.Errorf("start process with ConPTY: %w", err)
	}

	return &windowsPTYProcess{cpty: cpty}, nil
}

func buildWindowsCommandLine(executablePath string, args []string) string {
	lowerPath := strings.ToLower(executablePath)
	quotedExecutablePath := quoteWindowsCommandArg(executablePath)
	quotedArgs := quoteWindowsCommandArgs(args)

	if strings.HasSuffix(lowerPath, ".cmd") || strings.HasSuffix(lowerPath, ".bat") {
		commandLine := "cmd.exe /d /c call " + quotedExecutablePath
		if quotedArgs != "" {
			commandLine += " " + quotedArgs
		}
		return commandLine
	}

	if quotedArgs == "" {
		return quotedExecutablePath
	}
	return quotedExecutablePath + " " + quotedArgs
}

func quoteWindowsCommandArgs(args []string) string {
	if len(args) == 0 {
		return ""
	}

	quoted := make([]string, 0, len(args))
	for _, arg := range args {
		quoted = append(quoted, quoteWindowsCommandArg(arg))
	}
	return strings.Join(quoted, " ")
}

func quoteWindowsCommandArg(s string) string {
	if s == "" {
		return `""`
	}

	var b strings.Builder
	b.Grow(len(s) + 2)
	b.WriteByte('"')

	backslashes := 0
	for i := 0; i < len(s); i++ {
		switch s[i] {
		case '\\':
			backslashes++
		case '"':
			for j := 0; j < backslashes*2+1; j++ {
				b.WriteByte('\\')
			}
			b.WriteByte('"')
			backslashes = 0
		default:
			for j := 0; j < backslashes; j++ {
				b.WriteByte('\\')
			}
			backslashes = 0
			b.WriteByte(s[i])
		}
	}

	for j := 0; j < backslashes*2; j++ {
		b.WriteByte('\\')
	}
	b.WriteByte('"')

	return b.String()
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
