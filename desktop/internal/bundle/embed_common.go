package bundle

import (
	"crypto/sha256"
	"encoding/hex"
)

var artifactSHA256 = sha256Hex(artifactBytes)

func Bytes() []byte {
	return append([]byte(nil), artifactBytes...)
}

func Name() string {
	return artifactName
}

func SHA256Hex() string {
	return artifactSHA256
}

func sha256Hex(b []byte) string {
	sum := sha256.Sum256(b)
	return hex.EncodeToString(sum[:])
}
