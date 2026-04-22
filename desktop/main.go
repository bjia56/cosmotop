package main

import (
	"context"
	"embed"
	"errors"
	"fmt"
	"log"
	"os"
	"os/exec"
	"runtime"
	"slices"

	"github.com/andybalholm/crlf"
	"github.com/bjia56/cosmotop/desktop/internal/app"
	internalruntime "github.com/bjia56/cosmotop/desktop/internal/runtime"
	"github.com/bjia56/cosmotop/desktop/internal/terminal"
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

	err = terminal.FixupConsole()
	if err != nil {
		fmt.Fprintf(os.Stderr, "failed to fix up console: %v\n", err)
		return 1
	}

	cmd := internalruntime.NewCommand(info.Path, args)
	if runtime.GOOS == "windows" {
		cmd.Stdout = crlf.NewWriter(os.Stdout)
	} else {
		cmd.Stdout = os.Stdout
	}
	cmd.Stdin = os.Stdin

	err = cmd.Run()
	if err == nil && slices.Contains(args, "--version") {
		fmt.Printf("Desktop application compiled with: %s\n", runtime.Version())
	}

	if runtime.GOOS == "windows" {
		fmt.Println("Press Enter to exit...")
		os.Stdin.Read(make([]byte, 1))
	}

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
