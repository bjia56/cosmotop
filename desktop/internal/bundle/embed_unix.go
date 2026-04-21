//go:build linux || darwin

package bundle

import _ "embed"

const artifactName = "cosmotop"

//go:embed data/cosmotop
var artifactBytes []byte
