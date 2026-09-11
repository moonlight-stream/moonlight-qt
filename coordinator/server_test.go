package main

import (
	"encoding/json"
	"net/http"
	"net/http/httptest"
	"path/filepath"
	"strings"
	"testing"
	"time"
)

func TestDevelopmentLoginToSingleVMLease(t *testing.T) {
	store := NewStore([]VM{{
		ID: "vm-1", DisplayName: "Gaming VM 1", StreamAddress: "192.168.1.21",
		StreamPort: 47989, Enabled: true,
	}}, time.Minute, filepath.Join(t.TempDir(), "state.json"))
	broker := &AuthBroker{
		devAuthEnabled: true, pending: make(map[string]*pendingAuth),
		stateToRequest: make(map[string]string), sessions: make(map[string]coordinatorSession),
		now: time.Now,
	}
	server := newServer(store, broker)

	mux := http.NewServeMux()
	mux.HandleFunc("POST /v1/auth/dev", broker.DevLogin)
	mux.HandleFunc("POST /v1/leases", server.auth(server.createLease))

	firstToken := developmentToken(t, mux, "device-1")
	firstLease := httptest.NewRequest(http.MethodPost, "/v1/leases",
		strings.NewReader(`{"deviceId":"device-1","deviceName":"First"}`))
	firstLease.Header.Set("Authorization", "Bearer "+firstToken)
	firstResponse := httptest.NewRecorder()
	mux.ServeHTTP(firstResponse, firstLease)
	if firstResponse.Code != http.StatusCreated {
		t.Fatalf("first lease returned %d: %s", firstResponse.Code, firstResponse.Body.String())
	}
	var assigned struct {
		Host struct {
			Address string `json:"address"`
		} `json:"host"`
	}
	if err := json.NewDecoder(firstResponse.Body).Decode(&assigned); err != nil {
		t.Fatal(err)
	}
	if assigned.Host.Address != "192.168.1.21" {
		t.Fatalf("expected configured VM, got %q", assigned.Host.Address)
	}

	secondToken := developmentToken(t, mux, "device-2")
	secondLease := httptest.NewRequest(http.MethodPost, "/v1/leases",
		strings.NewReader(`{"deviceId":"device-2","deviceName":"Second"}`))
	secondLease.Header.Set("Authorization", "Bearer "+secondToken)
	secondResponse := httptest.NewRecorder()
	mux.ServeHTTP(secondResponse, secondLease)
	if secondResponse.Code != http.StatusConflict {
		t.Fatalf("second lease returned %d: %s", secondResponse.Code, secondResponse.Body.String())
	}
}

func TestLeaseReturnsPublicStreamingEndpoint(t *testing.T) {
	store := NewStore([]VM{{
		ID: "vm-1", DisplayName: "Gaming VM 1", StreamAddress: "192.168.1.23",
		StreamPort: 47989, PublicAddress: "stream.gilservers.com", PublicPort: 47989, Enabled: true,
	}}, time.Minute, filepath.Join(t.TempDir(), "state.json"))
	broker := &AuthBroker{
		devAuthEnabled: true, pending: make(map[string]*pendingAuth),
		stateToRequest: make(map[string]string), sessions: make(map[string]coordinatorSession),
		now: time.Now,
	}
	server := newServer(store, broker)
	mux := http.NewServeMux()
	mux.HandleFunc("POST /v1/auth/dev", broker.DevLogin)
	mux.HandleFunc("POST /v1/leases", server.auth(server.createLease))

	token := developmentToken(t, mux, "public-device")
	request := httptest.NewRequest(http.MethodPost, "/v1/leases",
		strings.NewReader(`{"deviceId":"public-device","deviceName":"Remote"}`))
	request.Header.Set("Authorization", "Bearer "+token)
	response := httptest.NewRecorder()
	mux.ServeHTTP(response, request)

	var assigned struct {
		Host struct {
			Address string `json:"address"`
			Port    int    `json:"port"`
		} `json:"host"`
	}
	if err := json.NewDecoder(response.Body).Decode(&assigned); err != nil {
		t.Fatal(err)
	}
	if assigned.Host.Address != "stream.gilservers.com" || assigned.Host.Port != 47989 {
		t.Fatalf("unexpected public endpoint: %#v", assigned.Host)
	}
}

func developmentToken(t *testing.T, handler http.Handler, deviceID string) string {
	t.Helper()
	request := httptest.NewRequest(http.MethodPost, "/v1/auth/dev",
		strings.NewReader(`{"deviceId":"`+deviceID+`"}`))
	response := httptest.NewRecorder()
	handler.ServeHTTP(response, request)
	if response.Code != http.StatusCreated {
		t.Fatalf("development login returned %d: %s", response.Code, response.Body.String())
	}
	var login struct {
		AccessToken string `json:"accessToken"`
	}
	if err := json.NewDecoder(response.Body).Decode(&login); err != nil {
		t.Fatal(err)
	}
	return login.AccessToken
}
