package main

import (
	"encoding/json"
	"net/http"
	"net/http/httptest"
	"net/url"
	"strings"
	"testing"
	"time"
)

func TestBrokeredGilIDLogin(t *testing.T) {
	identity := httptest.NewServer(http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		switch r.URL.Path {
		case "/oauth/token":
			var body map[string]string
			if err := json.NewDecoder(r.Body).Decode(&body); err != nil {
				t.Fatal(err)
			}
			if body["client_secret"] != "server-only-secret" || body["code"] != "valid-code" {
				t.Fatalf("unexpected token request: %#v", body)
			}
			writeJSON(w, http.StatusOK, map[string]string{"access_token": "gilid-access-token"})
		case "/auth/me":
			if r.Header.Get("Authorization") != "Bearer gilid-access-token" {
				t.Fatal("profile request did not use the GILid access token")
			}
			writeJSON(w, http.StatusOK, gilIDProfile{ID: "user-123", Username: "gil"})
		default:
			http.NotFound(w, r)
		}
	}))
	defer identity.Close()

	broker := &AuthBroker{
		clientID: "client-id", clientSecret: "server-only-secret",
		redirectURI:  "https://stream.example/auth/callback",
		authorizeURL: "https://auth.example/oauth/authorize",
		tokenURL:     identity.URL + "/oauth/token", profileURL: identity.URL + "/auth/me",
		pending: make(map[string]*pendingAuth), stateToRequest: make(map[string]string),
		sessions: make(map[string]coordinatorSession), httpClient: identity.Client(), now: time.Now,
	}

	startRequest := httptest.NewRequest(http.MethodPost, "/v1/auth/start", strings.NewReader(`{"deviceId":"device-1","deviceName":"Test PC"}`))
	startResponse := httptest.NewRecorder()
	broker.Start(startResponse, startRequest)
	if startResponse.Code != http.StatusCreated {
		t.Fatalf("start returned %d: %s", startResponse.Code, startResponse.Body.String())
	}
	var started struct {
		RequestID    string `json:"requestId"`
		AuthorizeURL string `json:"authorizeUrl"`
	}
	if err := json.NewDecoder(startResponse.Body).Decode(&started); err != nil {
		t.Fatal(err)
	}
	authorizeURL, err := url.Parse(started.AuthorizeURL)
	if err != nil {
		t.Fatal(err)
	}
	state := authorizeURL.Query().Get("state")
	if state == "" || authorizeURL.Query().Get("client_id") != "client-id" {
		t.Fatalf("invalid authorize URL: %s", started.AuthorizeURL)
	}

	callbackRequest := httptest.NewRequest(http.MethodGet, "/auth/callback?code=valid-code&state="+url.QueryEscape(state), nil)
	callbackResponse := httptest.NewRecorder()
	broker.Callback(callbackResponse, callbackRequest)
	if callbackResponse.Code != http.StatusOK {
		t.Fatalf("callback returned %d: %s", callbackResponse.Code, callbackResponse.Body.String())
	}

	statusRequest := httptest.NewRequest(http.MethodGet, "/v1/auth/status/"+started.RequestID, nil)
	statusRequest.SetPathValue("requestId", started.RequestID)
	statusResponse := httptest.NewRecorder()
	broker.Status(statusResponse, statusRequest)
	var completed struct {
		State       string `json:"state"`
		AccessToken string `json:"accessToken"`
	}
	if err := json.NewDecoder(statusResponse.Body).Decode(&completed); err != nil {
		t.Fatal(err)
	}
	if completed.State != "authenticated" || completed.AccessToken == "" {
		t.Fatalf("unexpected status response: %s", statusResponse.Body.String())
	}
	if owner := broker.AuthenticateToken(completed.AccessToken); owner != "user-123" {
		t.Fatalf("expected authenticated owner user-123, got %q", owner)
	}

	secondStatus := httptest.NewRecorder()
	broker.Status(secondStatus, statusRequest)
	if secondStatus.Code != http.StatusNotFound {
		t.Fatalf("one-time status response was reusable: %d", secondStatus.Code)
	}
}

func TestDevelopmentLoginMustBeEnabled(t *testing.T) {
	broker := &AuthBroker{
		pending: make(map[string]*pendingAuth), stateToRequest: make(map[string]string),
		sessions: make(map[string]coordinatorSession), now: time.Now,
	}
	request := httptest.NewRequest(http.MethodPost, "/v1/auth/dev", strings.NewReader(`{"deviceId":"device-1"}`))
	response := httptest.NewRecorder()
	broker.DevLogin(response, request)
	if response.Code != http.StatusNotFound {
		t.Fatalf("disabled development login returned %d", response.Code)
	}
}
