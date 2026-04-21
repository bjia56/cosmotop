//go:build !windows

package runtime

import (
	"os/exec"
)

func NewCommand(path string, args []string) *exec.Cmd {
	commandArgs := make([]string, 0, len(args)+1)
	commandArgs = append(commandArgs, path)
	commandArgs = append(commandArgs, args...)
	return exec.Command("sh", commandArgs...)
}
