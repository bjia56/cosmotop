//go:build windows

package bundle

import _ "embed"

const artifactName = "cosmotop.cmd"

//go:embed data/cosmotop.cmd
var artifactBytes []byte
