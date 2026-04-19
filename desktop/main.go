package main

import (
	"context"
	"embed"
	"errors"
	"fmt"
	"log"
	"os"
	"os/exec"
	goruntime "runtime"
	"strings"

	"github.com/bjia56/cosmotop/desktop/internal/app"
	internalruntime "github.com/bjia56/cosmotop/desktop/internal/runtime"
	"github.com/wailsapp/wails/v2"
	"github.com/wailsapp/wails/v2/pkg/options"
	"github.com/wailsapp/wails/v2/pkg/options/assetserver"
)

//go:embed all:frontend/dist
var assets embed.FS

type launchMode int

const (
	launchModeDesktop launchMode = iota
	launchModePassthrough
)

func main() {
	runtimeArgs := append([]string(nil), os.Args[1:]...)
	mode := classifyLaunchMode(runtimeArgs)
	if mode == launchModePassthrough {
		os.Exit(runRuntimePassthrough(runtimeArgs))
	}

	application := app.New(append([]string{"+t"}, runtimeArgs...))

	err := wails.Run(&options.App{
		Title:             "Cosmotop Desktop",
		Width:             1280,
		Height:            840,
		MinWidth:          1024,
		MinHeight:         768,
		DisableResize:     false,
		Frameless:         false,
		StartHidden:       false,
		HideWindowOnClose: false,
		AssetServer: &assetserver.Options{
			Assets: assets,
		},
		OnStartup:     application.Startup,
		OnDomReady:    application.DomReady,
		OnBeforeClose: application.BeforeClose,
		OnShutdown:    application.Shutdown,
		Bind: []interface{}{
			application,
		},
	})

	if err != nil {
		log.Fatal(err)
	}
}

func classifyLaunchMode(args []string) launchMode {
	if len(args) == 0 {
		return launchModeDesktop
	}

	for i := 0; i < len(args); i++ {
		switch args[i] {
		case "-h", "--help", "-v", "--version", "--licenses", "--show-defaults", "--show-themes", "--mcp":
			return launchModePassthrough
		case "-lc", "--low-color", "-t", "--tty_on", "+t", "--tty_off", "--debug":
			continue
		case "-p", "--preset", "-u", "--update", "-o", "--option":
			i++
			if i >= len(args) {
				return launchModePassthrough
			}
		default:
			return launchModePassthrough
		}
	}

	return launchModeDesktop
}

func runRuntimePassthrough(args []string) int {
	info, err := internalruntime.EnsureExtracted(context.Background())
	if err != nil {
		fmt.Fprintf(os.Stderr, "failed to prepare embedded runtime: %v\n", err)
		return 1
	}

	cmd := newRuntimeCommand(info.Path, args)
	cmd.Stdin = os.Stdin
	cmd.Stdout = os.Stdout
	cmd.Stderr = os.Stderr

	err = cmd.Run()
	if err == nil {
		return 0
	}

	var exitErr *exec.ExitError
	if errors.As(err, &exitErr) {
		return exitErr.ExitCode()
	}

	fmt.Fprintf(os.Stderr, "failed to start embedded runtime %q: %v\n", info.Path, err)
	return 1
}

func newRuntimeCommand(path string, args []string) *exec.Cmd {
	if goruntime.GOOS == "windows" {
		lowerPath := strings.ToLower(path)
		if strings.HasSuffix(lowerPath, ".cmd") || strings.HasSuffix(lowerPath, ".bat") {
			cmdArgs := []string{"/d", "/c", "call", path}
			cmdArgs = append(cmdArgs, args...)
			return exec.Command("cmd.exe", cmdArgs...)
		}
	}

	return exec.Command(path, args...)
}
