package runtime

import (
	"context"
	"crypto/sha256"
	"encoding/hex"
	"errors"
	"fmt"
	"io"
	"os"
	"path/filepath"
	goruntime "runtime"

	"github.com/bjia56/cosmotop/desktop/internal/bundle"
)

const runtimeCacheSubdir = "cosmotop-desktop/runtime"

var ErrDigestMismatch = errors.New("extracted runtime digest mismatch")

var (
	userCacheDirFunc = os.UserCacheDir
	bundleBytesFunc  = bundle.Bytes
	bundleDigestFunc = bundle.SHA256Hex
	targetFilename   = bundle.Name
	goos             = goruntime.GOOS
)

type ExtractedBinaryInfo struct {
	Path       string
	Digest     string
	VersionDir string
}

func EnsureExtracted(ctx context.Context) (ExtractedBinaryInfo, error) {
	if err := ctx.Err(); err != nil {
		return ExtractedBinaryInfo{}, err
	}

	expectedDigest := bundleDigestFunc()
	cacheRoot, err := userCacheDirFunc()
	if err != nil {
		return ExtractedBinaryInfo{}, fmt.Errorf("resolve user cache dir: %w", err)
	}

	versionDir := filepath.Join(cacheRoot, runtimeCacheSubdir, expectedDigest)
	binaryPath := filepath.Join(versionDir, targetFilename())
	info := ExtractedBinaryInfo{Path: binaryPath, Digest: expectedDigest, VersionDir: versionDir}

	if err := os.MkdirAll(versionDir, 0o700); err != nil {
		return ExtractedBinaryInfo{}, fmt.Errorf("create runtime cache dir %q: %w", versionDir, err)
	}

	if ok, err := fileMatchesDigest(binaryPath, expectedDigest); err == nil && ok {
		return info, nil
	} else if err != nil && !errors.Is(err, os.ErrNotExist) {
		return ExtractedBinaryInfo{}, fmt.Errorf("check existing runtime binary %q: %w", binaryPath, err)
	}

	if err := ctx.Err(); err != nil {
		return ExtractedBinaryInfo{}, err
	}

	tmpPath, err := writeTempBinary(versionDir)
	if err != nil {
		return ExtractedBinaryInfo{}, err
	}
	defer os.Remove(tmpPath)

	if err := moveIntoPlace(tmpPath, binaryPath, expectedDigest); err != nil {
		return ExtractedBinaryInfo{}, err
	}

	if ok, err := fileMatchesDigest(binaryPath, expectedDigest); err != nil {
		return ExtractedBinaryInfo{}, fmt.Errorf("verify runtime binary digest %q: %w", binaryPath, err)
	} else if !ok {
		return ExtractedBinaryInfo{}, fmt.Errorf("%w at %q", ErrDigestMismatch, binaryPath)
	}

	return info, nil
}

func writeTempBinary(versionDir string) (string, error) {
	tmpFile, err := os.CreateTemp(versionDir, ".tmp-cosmotop-*")
	if err != nil {
		return "", fmt.Errorf("create temp runtime file: %w", err)
	}

	tmpPath := tmpFile.Name()
	if _, err := tmpFile.Write(bundleBytesFunc()); err != nil {
		tmpFile.Close()
		return "", fmt.Errorf("write temp runtime file %q: %w", tmpPath, err)
	}

	if goos != "windows" {
		if err := tmpFile.Chmod(0o700); err != nil {
			tmpFile.Close()
			return "", fmt.Errorf("set executable bit on temp runtime file %q: %w", tmpPath, err)
		}
	}

	if err := tmpFile.Sync(); err != nil {
		tmpFile.Close()
		return "", fmt.Errorf("sync temp runtime file %q: %w", tmpPath, err)
	}

	if err := tmpFile.Close(); err != nil {
		return "", fmt.Errorf("close temp runtime file %q: %w", tmpPath, err)
	}

	return tmpPath, nil
}

func moveIntoPlace(tmpPath, binaryPath, expectedDigest string) error {
	if err := os.Rename(tmpPath, binaryPath); err == nil {
		return nil
	}

	if ok, err := fileMatchesDigest(binaryPath, expectedDigest); err == nil && ok {
		return nil
	}

	if err := os.Remove(binaryPath); err != nil && !errors.Is(err, os.ErrNotExist) {
		return fmt.Errorf("remove stale runtime binary %q: %w", binaryPath, err)
	}

	if err := os.Rename(tmpPath, binaryPath); err != nil {
		if ok, digestErr := fileMatchesDigest(binaryPath, expectedDigest); digestErr == nil && ok {
			return nil
		}
		return fmt.Errorf("move runtime binary into place %q: %w", binaryPath, err)
	}

	return nil
}

func fileMatchesDigest(path, expectedDigest string) (bool, error) {
	gotDigest, err := fileSHA256Hex(path)
	if err != nil {
		return false, err
	}
	return gotDigest == expectedDigest, nil
}

func fileSHA256Hex(path string) (string, error) {
	f, err := os.Open(path)
	if err != nil {
		return "", err
	}
	defer f.Close()

	h := sha256.New()
	if _, err := io.Copy(h, f); err != nil {
		return "", fmt.Errorf("hash %q: %w", path, err)
	}

	return hex.EncodeToString(h.Sum(nil)), nil
}
