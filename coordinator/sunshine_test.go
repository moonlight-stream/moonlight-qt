package main

import (
	"context"
	"encoding/json"
	"net/http"
	"net/http/httptest"
	"testing"
)

func TestSunshinePairerUsesCurrentPairingIDAPI(t *testing.T) {
	t.Setenv("SUNSHINE_USERNAME", "admin")
	t.Setenv("SUNSHINE_PASSWORD", "secret")
	const pairingID = "0123456789abcdef0123456789abcdef"

	sunshine := httptest.NewTLSServer(http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		username, password, ok := r.BasicAuth()
		if !ok || username != "admin" || password != "secret" {
			t.Fatal("Sunshine request did not contain the configured credentials")
		}
		switch r.Method {
		case http.MethodGet:
			writeJSON(w, http.StatusOK, map[string]any{"pairings": []map[string]string{{"id": pairingID}}})
		case http.MethodPost:
			var body map[string]string
			if err := json.NewDecoder(r.Body).Decode(&body); err != nil {
				t.Fatal(err)
			}
			if body["pairing_id"] != pairingID || body["pin"] != "1234" || body["name"] != "Test PC" {
				t.Fatalf("unexpected pairing request: %#v", body)
			}
			writeJSON(w, http.StatusOK, map[string]bool{"status": true})
		default:
			http.Error(w, "unsupported", http.StatusMethodNotAllowed)
		}
	}))
	defer sunshine.Close()

	pairer := NewSunshinePairer()
	vm := VM{SunshineAPIURL: sunshine.URL}
	if err := pairer.Pair(context.Background(), vm, "1234", "Test PC"); err != nil {
		t.Fatal(err)
	}
}

func TestSunshinePairerSupportsLegacyAPI(t *testing.T) {
	t.Setenv("SUNSHINE_USERNAME", "admin")
	t.Setenv("SUNSHINE_PASSWORD", "secret")

	sunshine := httptest.NewTLSServer(http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		if r.Method == http.MethodGet {
			http.Error(w, "unsupported", http.StatusMethodNotAllowed)
			return
		}
		var body map[string]string
		if err := json.NewDecoder(r.Body).Decode(&body); err != nil {
			t.Fatal(err)
		}
		if _, present := body["pairing_id"]; present {
			t.Fatal("legacy pairing request included pairing_id")
		}
		writeJSON(w, http.StatusOK, map[string]string{"status": "true"})
	}))
	defer sunshine.Close()

	pairer := NewSunshinePairer()
	vm := VM{SunshineAPIURL: sunshine.URL}
	if err := pairer.Pair(context.Background(), vm, "9876", "Legacy PC"); err != nil {
		t.Fatal(err)
	}
}
