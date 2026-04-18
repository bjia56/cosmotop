package terminal

import (
	"context"
	"io"
)

type ptyProcess interface {
	io.Reader
	io.Writer

	Resize(cols, rows int) error
	GracefulStop() error
	ForceKill() error
	Wait(ctx context.Context) (int, error)
	Close() error
}
