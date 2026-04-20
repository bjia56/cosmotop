//go:build !windows

package main

import (
	"os"
	"os/exec"
)

func configurePassthroughCommandIO(cmd *exec.Cmd) func() {
	cmd.Stdin = os.Stdin
	cmd.Stdout = os.Stdout
	cmd.Stderr = os.Stderr
	return func() {}
}
