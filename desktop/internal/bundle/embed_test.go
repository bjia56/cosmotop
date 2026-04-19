package bundle

import (
	"crypto/sha256"
	"encoding/hex"
	"runtime"
	"testing"
)

func TestName(t *testing.T) {
	want := "cosmotop"
	if runtime.GOOS == "windows" {
		want = "cosmotop.cmd"
	}
	if got := Name(); got != want {
		t.Fatalf("Name() = %q, want %q", got, want)
	}
}

func TestBytesReturnsCopy(t *testing.T) {
	b := Bytes()
	if len(b) == 0 {
		t.Fatal("Bytes() returned empty artifact")
	}

	original := b[0]
	b[0] ^= 0xff

	b2 := Bytes()
	if b2[0] != original {
		t.Fatal("Bytes() did not return a defensive copy")
	}
}

func TestSHA256Hex(t *testing.T) {
	sum := sha256.Sum256(Bytes())
	want := hex.EncodeToString(sum[:])

	if got := SHA256Hex(); got != want {
		t.Fatalf("SHA256Hex() = %q, want %q", got, want)
	}
}
