# Embedded bundle artifact

This directory contains the single embedded artifact file used by
`desktop/internal/bundle`.

Expected filenames (platform-specific embed):

- Unix builds (`linux`/`darwin`): `cosmotop`
- Windows builds: `cosmotop.cmd`

CI copy convention:

- CI should copy the built runtime artifact into this directory before desktop
  build.
- For Unix desktop builds, the runtime file must be at
  `desktop/internal/bundle/data/cosmotop`.
- For Windows desktop builds, the runtime file must be at
  `desktop/internal/bundle/data/cosmotop.cmd`.
