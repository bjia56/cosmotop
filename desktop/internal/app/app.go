package app

import (
	"context"
	"log"

	"github.com/bjia56/cosmotop/desktop/internal/runtime"
)

type App struct {
	ctx context.Context
}

func New() *App {
	return &App{}
}

func (a *App) Startup(ctx context.Context) {
	a.ctx = ctx

	info, err := runtime.EnsureExtracted(ctx)
	if err != nil {
		log.Printf("runtime extraction failed: %v", err)
		return
	}

	log.Printf("runtime extracted path=%q digest=%s", info.Path, info.Digest)
}

func (a *App) DomReady(ctx context.Context) {
	a.ctx = ctx
}

func (a *App) BeforeClose(ctx context.Context) bool {
	a.ctx = ctx
	return false
}

func (a *App) Shutdown(ctx context.Context) {
	a.ctx = ctx
}

func (a *App) Status() string {
	return "Cosmotop terminal runtime is not wired yet."
}
