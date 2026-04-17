package runtime

import (
	"context"
	"crypto/sha256"
	"encoding/hex"
	"errors"
	"fmt"
	"os"
	"path/filepath"
	"strings"
	"testing"
)

func TestEnsureExtractedFreshExtraction(t *testing.T) {
	hooks := setTestHooks(t, []byte("fresh-runtime-bytes"), "linux")
	defer hooks()

	cacheRoot := t.TempDir()
	userCacheDirFunc = func() (string, error) { return cacheRoot, nil }

	info, err := EnsureExtracted(context.Background())
	if err != nil {
		t.Fatalf("EnsureExtracted() error = %v", err)
	}

	if got, want := info.VersionDir, filepath.Join(cacheRoot, runtimeCacheSubdir, info.Digest); got != want {
		t.Fatalf("VersionDir = %q, want %q", got, want)
	}

	if got, want := info.Path, filepath.Join(info.VersionDir, "cosmotop"); got != want {
		t.Fatalf("Path = %q, want %q", got, want)
	}

	b, err := os.ReadFile(info.Path)
	if err != nil {
		t.Fatalf("ReadFile(%q) error = %v", info.Path, err)
	}
	if got, want := string(b), "fresh-runtime-bytes"; got != want {
		t.Fatalf("extracted bytes = %q, want %q", got, want)
	}

	st, err := os.Stat(info.Path)
	if err != nil {
		t.Fatalf("Stat(%q) error = %v", info.Path, err)
	}
	if got, want := st.Mode().Perm(), os.FileMode(0o700); got != want {
		t.Fatalf("mode = %o, want %o", got, want)
	}
}

func TestEnsureExtractedNoOpExistingGoodFile(t *testing.T) {
	hooks := setTestHooks(t, []byte("existing-good-runtime"), "linux")
	defer hooks()

	cacheRoot := t.TempDir()
	userCacheDirFunc = func() (string, error) { return cacheRoot, nil }

	first, err := EnsureExtracted(context.Background())
	if err != nil {
		t.Fatalf("first EnsureExtracted() error = %v", err)
	}

	stBefore, err := os.Stat(first.Path)
	if err != nil {
		t.Fatalf("Stat before second extraction error = %v", err)
	}

	second, err := EnsureExtracted(context.Background())
	if err != nil {
		t.Fatalf("second EnsureExtracted() error = %v", err)
	}
	if first != second {
		t.Fatalf("second info = %+v, want %+v", second, first)
	}

	stAfter, err := os.Stat(second.Path)
	if err != nil {
		t.Fatalf("Stat after second extraction error = %v", err)
	}

	if !stAfter.ModTime().Equal(stBefore.ModTime()) {
		t.Fatalf("expected no rewrite, modtime changed from %v to %v", stBefore.ModTime(), stAfter.ModTime())
	}
}

func TestEnsureExtractedRewriteCorruptedFile(t *testing.T) {
	hooks := setTestHooks(t, []byte("correct-runtime"), "linux")
	defer hooks()

	cacheRoot := t.TempDir()
	userCacheDirFunc = func() (string, error) { return cacheRoot, nil }

	expectedDigest := bundleDigestFunc()
	versionDir := filepath.Join(cacheRoot, runtimeCacheSubdir, expectedDigest)
	binaryPath := filepath.Join(versionDir, "cosmotop")

	if err := os.MkdirAll(versionDir, 0o700); err != nil {
		t.Fatalf("MkdirAll(%q) error = %v", versionDir, err)
	}
	if err := os.WriteFile(binaryPath, []byte("corrupted"), 0o600); err != nil {
		t.Fatalf("WriteFile(%q) error = %v", binaryPath, err)
	}

	info, err := EnsureExtracted(context.Background())
	if err != nil {
		t.Fatalf("EnsureExtracted() error = %v", err)
	}

	b, err := os.ReadFile(info.Path)
	if err != nil {
		t.Fatalf("ReadFile(%q) error = %v", info.Path, err)
	}
	if got, want := string(b), "correct-runtime"; got != want {
		t.Fatalf("rewritten bytes = %q, want %q", got, want)
	}
}

func TestEnsureExtractedDigestMismatchError(t *testing.T) {
	hooks := setTestHooks(t, []byte("actual-runtime-bytes"), "linux")
	defer hooks()

	bundleDigestFunc = func() string { return "0000000000000000000000000000000000000000000000000000000000000000" }

	cacheRoot := t.TempDir()
	userCacheDirFunc = func() (string, error) { return cacheRoot, nil }

	_, err := EnsureExtracted(context.Background())
	if err == nil {
		t.Fatal("EnsureExtracted() error = nil, want digest mismatch")
	}
	if !errors.Is(err, ErrDigestMismatch) {
		t.Fatalf("EnsureExtracted() error = %v, want ErrDigestMismatch", err)
	}

	if !strings.Contains(err.Error(), filepath.Join(cacheRoot, runtimeCacheSubdir, bundleDigestFunc(), "cosmotop")) {
		t.Fatalf("EnsureExtracted() error = %q, want extracted path included", err)
	}
}

func setTestHooks(t *testing.T, bundleBytes []byte, osName string) func() {
	t.Helper()

	origUserCacheDir := userCacheDirFunc
	origBundleBytes := bundleBytesFunc
	origBundleDigest := bundleDigestFunc
	origBundleName := bundleNameFunc
	origGOOS := goos

	userCacheDirFunc = os.UserCacheDir
	bundleBytesFunc = func() []byte { return append([]byte(nil), bundleBytes...) }
	bundleDigestFunc = func() string {
		sum := sha256.Sum256(bundleBytes)
		return hex.EncodeToString(sum[:])
	}
	bundleNameFunc = func() string { return "cosmotop" }
	goos = osName

	return func() {
		userCacheDirFunc = origUserCacheDir
		bundleBytesFunc = origBundleBytes
		bundleDigestFunc = origBundleDigest
		bundleNameFunc = origBundleName
		goos = origGOOS
	}
}

func TestEnsureExtractedUserCacheError(t *testing.T) {
	hooks := setTestHooks(t, []byte("runtime"), "linux")
	defer hooks()

	userCacheDirFunc = func() (string, error) { return "", fmt.Errorf("no cache") }

	_, err := EnsureExtracted(context.Background())
	if err == nil {
		t.Fatal("EnsureExtracted() error = nil, want user cache dir error")
	}
	if !strings.Contains(err.Error(), "resolve user cache dir") {
		t.Fatalf("EnsureExtracted() error = %q, want cache dir context", err)
	}
}
