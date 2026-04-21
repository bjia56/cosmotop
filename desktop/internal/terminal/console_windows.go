//go:build windows

package terminal

import (
	"fmt"
	"os"
	"syscall"
)

var (
	kernel32     = syscall.MustLoadDLL("kernel32.dll")
	allocConsole = kernel32.MustFindProc("AllocConsole")
)

func FixupConsole() error {
	r, _, err := allocConsole.Call()
	if r == 0 {
		return fmt.Errorf("AllocConsole failed: %v", err)
	}

	conin, err := os.OpenFile("CONIN$", os.O_RDONLY, 0)
	if err != nil {
		return fmt.Errorf("open CONIN$: %w", err)
	}

	conout, err := os.OpenFile("CONOUT$", os.O_WRONLY, 0)
	if err != nil {
		conin.Close()
		return fmt.Errorf("open CONOUT$: %w", err)
	}

	os.Stdin = conin
	os.Stdout = conout
	os.Stderr = conout

	return nil
}
