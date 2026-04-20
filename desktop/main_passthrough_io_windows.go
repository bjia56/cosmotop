//go:build windows

package main

import (
	"errors"
	"os"
	"os/exec"

	"golang.org/x/sys/windows"
)

func configurePassthroughCommandIO(cmd *exec.Cmd) func() {
	cmd.Stdin = os.Stdin
	cmd.Stdout = os.Stdout
	cmd.Stderr = os.Stderr

	attached, detachParentConsole := attachParentConsole()
	if !attached {
		return func() {}
	}

	stdinHandle, err := os.OpenFile("CONIN$", os.O_RDONLY, 0)
	if err != nil {
		if detachParentConsole {
			_ = windows.FreeConsole()
		}
		return func() {}
	}

	stdoutHandle, err := os.OpenFile("CONOUT$", os.O_WRONLY, 0)
	if err != nil {
		_ = stdinHandle.Close()
		if detachParentConsole {
			_ = windows.FreeConsole()
		}
		return func() {}
	}

	cmd.Stdin = stdinHandle
	cmd.Stdout = stdoutHandle
	cmd.Stderr = stdoutHandle

	return func() {
		_ = stdinHandle.Close()
		_ = stdoutHandle.Close()
		if detachParentConsole {
			_ = windows.FreeConsole()
		}
	}
}

func attachParentConsole() (attached bool, shouldDetach bool) {
	err := windows.AttachConsole(windows.ATTACH_PARENT_PROCESS)
	if err == nil {
		return true, true
	}

	if errors.Is(err, windows.ERROR_ACCESS_DENIED) {
		return true, false
	}

	return false, false
}
