# Embedded bundle artifact

This directory contains the single embedded artifact file used by
`desktop/internal/bundle`.

Expected filename:

- `cosmotop`

CI copy convention:

- CI should copy the built desktop artifact into this directory using the exact
  path `desktop/internal/bundle/data/cosmotop` before running `go test` or
  packaging steps.
- The filename must remain `cosmotop` so `//go:embed data/cosmotop` keeps
  working without code changes.
