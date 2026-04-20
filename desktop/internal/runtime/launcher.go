package runtime

import (
	"os/exec"
	"strings"
)

func NewCommand(path string, args []string) *exec.Cmd {
	if goos == "windows" {
		commandArgs := WindowsCommandLine(path, args)
		return exec.Command(commandArgs[0], commandArgs[1:]...)
	}

	commandArgs := make([]string, 0, len(args)+1)
	commandArgs = append(commandArgs, path)
	commandArgs = append(commandArgs, args...)
	return exec.Command("sh", commandArgs...)
}

func WindowsCommandLine(path string, args []string) []string {
	quotedPath := quoteWindowsCommandArg(path)
	quotedArgs := quoteWindowsCommandArgs(args)

	if isWindowsBatchScript(path) {
		commandLine := []string{"cmd.exe", "/d", "/c", "call", quotedPath}
		if len(quotedArgs) > 0 {
			commandLine = append(commandLine, quotedArgs...)
		}
		return commandLine
	}

	if len(quotedArgs) == 0 {
		return []string{quotedPath}
	}
	return append([]string{quotedPath}, quotedArgs...)
}

func isWindowsBatchScript(path string) bool {
	lowerPath := strings.ToLower(path)
	return strings.HasSuffix(lowerPath, ".cmd") || strings.HasSuffix(lowerPath, ".bat")
}

func quoteWindowsCommandArgs(args []string) []string {
	if len(args) == 0 {
		return []string{}
	}

	quoted := make([]string, 0, len(args))
	for _, arg := range args {
		quoted = append(quoted, quoteWindowsCommandArg(arg))
	}
	return quoted
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
