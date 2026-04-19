//go:build !windows

package terminal

import (
	"context"
	"errors"
	"fmt"
	"os"
	"os/exec"
	"path/filepath"
	"strconv"
	"strings"
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

func startPTYProcess(executablePath string, args []string, cols, rows int, env []string) (ptyProcess, error) {
	commandArgs := make([]string, 0, len(args)+1)
	commandArgs = append(commandArgs, executablePath)
	commandArgs = append(commandArgs, args...)

	cmd := exec.Command("sh", commandArgs...)
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

func (p *unixPTYProcess) ForceKill() error {
	if p.cmd.Process == nil {
		return nil
	}

	pid := p.cmd.Process.Pid
	if pid > 0 {
		descendants, err := descendantPIDs(pid)
		if err == nil {
			for _, childPID := range descendants {
				if killErr := syscall.Kill(childPID, syscall.SIGKILL); killErr != nil && !errors.Is(killErr, syscall.ESRCH) {
					return killErr
				}
			}
		}

		if err := syscall.Kill(pid, syscall.SIGKILL); err != nil && !errors.Is(err, syscall.ESRCH) {
			return err
		}
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

func descendantPIDs(rootPID int) ([]int, error) {
	out, err := exec.Command("ps", "-A", "-o", "pid=", "-o", "ppid=").Output()
	if err != nil {
		return nil, err
	}

	childrenByParent := make(map[int][]int)
	lines := strings.Split(string(out), "\n")
	for _, line := range lines {
		line = strings.TrimSpace(line)
		if line == "" {
			continue
		}

		fields := strings.Fields(line)
		if len(fields) < 2 {
			continue
		}

		pid, pidErr := strconv.Atoi(fields[0])
		ppid, ppidErr := strconv.Atoi(fields[1])
		if pidErr != nil || ppidErr != nil {
			continue
		}

		childrenByParent[ppid] = append(childrenByParent[ppid], pid)
	}

	visited := make(map[int]bool)
	ordered := make([]int, 0)

	var walk func(int)
	walk = func(parent int) {
		for _, child := range childrenByParent[parent] {
			if visited[child] {
				continue
			}
			visited[child] = true
			walk(child)
			ordered = append(ordered, child)
		}
	}

	walk(rootPID)
	return ordered, nil
}
