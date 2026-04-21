//go:build !windows

package terminal

func FixupConsole() error {
	// No special handling needed on Unix-like platforms.
	return nil
}
