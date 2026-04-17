package app

import "context"

type App struct {
	ctx context.Context
}

func New() *App {
	return &App{}
}

func (a *App) Startup(ctx context.Context) {
	a.ctx = ctx
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
