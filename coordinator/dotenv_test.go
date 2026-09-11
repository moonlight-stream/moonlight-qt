package main

import (
	"os"
	"path/filepath"
	"testing"
)

func TestParseDotEnvLine(t *testing.T) {
	tests := []struct {
		line      string
		wantName  string
		wantValue string
		wantOK    bool
	}{
		{"GILID_CLIENT_ID=client-id", "GILID_CLIENT_ID", "client-id", true},
		{`GILID_CLIENT_SECRET="secret=value"`, "GILID_CLIENT_SECRET", "secret=value", true},
		{"export GILID_REDIRECT_URI='https://example.test/callback'", "GILID_REDIRECT_URI", "https://example.test/callback", true},
		{"# comment", "", "", false},
		{"   ", "", "", false},
	}
	for _, test := range tests {
		name, value, ok, err := parseDotEnvLine(test.line)
		if err != nil {
			t.Fatalf("parse %q: %v", test.line, err)
		}
		if name != test.wantName || value != test.wantValue || ok != test.wantOK {
			t.Fatalf("parse %q = (%q, %q, %v), want (%q, %q, %v)",
				test.line, name, value, ok, test.wantName, test.wantValue, test.wantOK)
		}
	}
}

func TestLoadDotEnvPreservesProcessEnvironment(t *testing.T) {
	const existingName = "GILSTREAMING_TEST_EXISTING"
	const loadedName = "GILSTREAMING_TEST_LOADED"
	t.Setenv(existingName, "from-process")
	_ = os.Unsetenv(loadedName)
	t.Cleanup(func() { _ = os.Unsetenv(loadedName) })

	path := filepath.Join(t.TempDir(), ".env")
	contents := existingName + "=from-file\n" + loadedName + "=from-file\n"
	if err := os.WriteFile(path, []byte(contents), 0o600); err != nil {
		t.Fatal(err)
	}
	if err := loadDotEnv(path); err != nil {
		t.Fatal(err)
	}
	if value := os.Getenv(existingName); value != "from-process" {
		t.Fatalf("existing variable was overwritten: %q", value)
	}
	if value := os.Getenv(loadedName); value != "from-file" {
		t.Fatalf("file variable was not loaded: %q", value)
	}
}

func TestLoadDotEnvAllowsMissingFile(t *testing.T) {
	if err := loadDotEnv(filepath.Join(t.TempDir(), "missing.env")); err != nil {
		t.Fatal(err)
	}
}
