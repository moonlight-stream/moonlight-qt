package main

import (
	"bytes"
	"context"
	"crypto/sha256"
	"encoding/hex"
	"encoding/json"
	"errors"
	"fmt"
	"html"
	"io"
	"net/http"
	"net/url"
	"os"
	"strings"
	"sync"
	"time"
)

const authRequestLifetime = 10 * time.Minute
const coordinatorSessionLifetime = 12 * time.Hour

type gilIDProfile struct {
	ID        string `json:"id"`
	Email     string `json:"email"`
	Username  string `json:"username"`
	FirstName string `json:"first_name"`
	LastName  string `json:"last_name"`
}

type pendingAuth struct {
	RequestID   string
	State       string
	DeviceID    string
	DeviceName  string
	ExpiresAt   time.Time
	AccessToken string
	Profile     gilIDProfile
	Error       string
}

type coordinatorSession struct {
	Owner     string
	ExpiresAt time.Time
}

type AuthBroker struct {
	mu             sync.Mutex
	clientID       string
	clientSecret   string
	redirectURI    string
	authorizeURL   string
	tokenURL       string
	profileURL     string
	devAuthEnabled bool
	pending        map[string]*pendingAuth
	stateToRequest map[string]string
	sessions       map[string]coordinatorSession
	httpClient     *http.Client
	now            func() time.Time
}

func NewAuthBrokerFromEnvironment(devAuthEnabled bool) (*AuthBroker, error) {
	broker := &AuthBroker{
		clientID:       strings.TrimSpace(os.Getenv("GILID_CLIENT_ID")),
		clientSecret:   strings.TrimSpace(os.Getenv("GILID_CLIENT_SECRET")),
		redirectURI:    strings.TrimSpace(os.Getenv("GILID_REDIRECT_URI")),
		authorizeURL:   strings.TrimSpace(os.Getenv("GILID_AUTHORIZE_URL")),
		tokenURL:       strings.TrimSpace(os.Getenv("GILID_TOKEN_URL")),
		profileURL:     strings.TrimSpace(os.Getenv("GILID_PROFILE_URL")),
		devAuthEnabled: devAuthEnabled,
		pending:        make(map[string]*pendingAuth),
		stateToRequest: make(map[string]string),
		sessions:       make(map[string]coordinatorSession),
		httpClient:     &http.Client{Timeout: 10 * time.Second},
		now:            time.Now,
	}
	if broker.clientID == "" || broker.clientSecret == "" || broker.redirectURI == "" ||
		broker.authorizeURL == "" || broker.tokenURL == "" || broker.profileURL == "" {
		return nil, errors.New("all GILID_* OAuth environment variables are required")
	}
	for name, value := range map[string]string{
		"GILID_REDIRECT_URI": broker.redirectURI, "GILID_AUTHORIZE_URL": broker.authorizeURL,
		"GILID_TOKEN_URL": broker.tokenURL, "GILID_PROFILE_URL": broker.profileURL,
	} {
		parsed, err := url.Parse(value)
		if err != nil || parsed.Scheme != "https" || parsed.Host == "" {
			return nil, fmt.Errorf("%s must be an absolute HTTPS URL", name)
		}
	}
	return broker, nil
}

func (b *AuthBroker) Start(w http.ResponseWriter, r *http.Request) {
	var request struct {
		DeviceID   string `json:"deviceId"`
		DeviceName string `json:"deviceName"`
	}
	if err := decodeJSON(w, r, &request); err != nil || strings.TrimSpace(request.DeviceID) == "" {
		writeError(w, http.StatusBadRequest, "INVALID_REQUEST", "deviceId is required")
		return
	}
	requestID, err := randomID()
	if err != nil {
		writeError(w, http.StatusInternalServerError, "INTERNAL_ERROR", "could not start login")
		return
	}
	state, err := randomID()
	if err != nil {
		writeError(w, http.StatusInternalServerError, "INTERNAL_ERROR", "could not start login")
		return
	}

	now := b.now().UTC()
	pending := &pendingAuth{
		RequestID: requestID, State: state, DeviceID: request.DeviceID,
		DeviceName: request.DeviceName, ExpiresAt: now.Add(authRequestLifetime),
	}
	b.mu.Lock()
	b.cleanupLocked(now)
	b.pending[requestID] = pending
	b.stateToRequest[state] = requestID
	b.mu.Unlock()

	authorize, _ := url.Parse(b.authorizeURL)
	query := authorize.Query()
	query.Set("client_id", b.clientID)
	query.Set("redirect_uri", b.redirectURI)
	query.Set("response_type", "code")
	query.Set("scope", "profile email")
	query.Set("state", state)
	authorize.RawQuery = query.Encode()
	writeJSON(w, http.StatusCreated, map[string]any{
		"requestId": requestID, "authorizeUrl": authorize.String(), "expiresAt": pending.ExpiresAt,
	})
}

func (b *AuthBroker) Status(w http.ResponseWriter, r *http.Request) {
	now := b.now().UTC()
	b.mu.Lock()
	b.cleanupLocked(now)
	pending := b.pending[r.PathValue("requestId")]
	if pending == nil {
		b.mu.Unlock()
		writeError(w, http.StatusNotFound, "LOGIN_NOT_FOUND", "login request is missing or expired")
		return
	}
	if pending.Error != "" {
		message := pending.Error
		delete(b.pending, pending.RequestID)
		b.mu.Unlock()
		writeError(w, http.StatusBadGateway, "LOGIN_FAILED", message)
		return
	}
	if pending.AccessToken == "" {
		expiresAt := pending.ExpiresAt
		b.mu.Unlock()
		writeJSON(w, http.StatusOK, map[string]any{"state": "pending", "expiresAt": expiresAt})
		return
	}
	token := pending.AccessToken
	profile := pending.Profile
	delete(b.pending, pending.RequestID)
	delete(b.stateToRequest, pending.State)
	b.mu.Unlock()
	writeJSON(w, http.StatusOK, map[string]any{
		"state": "authenticated", "accessToken": token, "profile": profile,
	})
}

func (b *AuthBroker) Callback(w http.ResponseWriter, r *http.Request) {
	state := r.URL.Query().Get("state")
	code := r.URL.Query().Get("code")
	b.mu.Lock()
	requestID := b.stateToRequest[state]
	pending := b.pending[requestID]
	if pending != nil {
		// Consume the OAuth state before making network requests so a callback
		// cannot be replayed or raced against itself.
		delete(b.stateToRequest, state)
	}
	b.mu.Unlock()
	if state == "" || pending == nil || !pending.ExpiresAt.After(b.now()) {
		http.Error(w, "Invalid or expired login request", http.StatusBadRequest)
		return
	}
	if oauthError := r.URL.Query().Get("error"); oauthError != "" {
		b.failPending(requestID, "GILid denied the login request")
		writeCallbackPage(w, "Login cancelled", "You can return to GilStreaming and try again.")
		return
	}
	if code == "" {
		b.failPending(requestID, "GILid returned no authorization code")
		http.Error(w, "Missing authorization code", http.StatusBadRequest)
		return
	}

	profile, err := b.exchangeAndLoadProfile(r.Context(), code)
	if err != nil {
		b.failPending(requestID, "GILid authentication could not be completed")
		http.Error(w, "Authentication failed", http.StatusBadGateway)
		return
	}
	coordinatorToken, err := randomID()
	if err != nil {
		b.failPending(requestID, "Could not create a GilStreaming session")
		http.Error(w, "Authentication failed", http.StatusInternalServerError)
		return
	}
	hash := sha256.Sum256([]byte(coordinatorToken))
	b.mu.Lock()
	if current := b.pending[requestID]; current != nil {
		current.AccessToken = coordinatorToken
		current.Profile = profile
		b.sessions[hex.EncodeToString(hash[:])] = coordinatorSession{
			Owner: profile.ID, ExpiresAt: b.now().UTC().Add(coordinatorSessionLifetime),
		}
	}
	b.mu.Unlock()
	writeCallbackPage(w, "Signed in", "Authentication succeeded. You can return to GilStreaming.")
}

func (b *AuthBroker) DevLogin(w http.ResponseWriter, r *http.Request) {
	if !b.devAuthEnabled {
		writeError(w, http.StatusNotFound, "NOT_FOUND", "development login is disabled")
		return
	}
	var request struct {
		DeviceID string `json:"deviceId"`
	}
	if err := decodeJSON(w, r, &request); err != nil || request.DeviceID == "" {
		writeError(w, http.StatusBadRequest, "INVALID_REQUEST", "deviceId is required")
		return
	}
	token, err := randomID()
	if err != nil {
		writeError(w, http.StatusInternalServerError, "INTERNAL_ERROR", "could not create development session")
		return
	}
	hash := sha256.Sum256([]byte(token))
	owner := "dev:" + request.DeviceID
	b.mu.Lock()
	b.sessions[hex.EncodeToString(hash[:])] = coordinatorSession{
		Owner: owner, ExpiresAt: b.now().UTC().Add(coordinatorSessionLifetime),
	}
	b.mu.Unlock()
	writeJSON(w, http.StatusCreated, map[string]any{
		"state": "authenticated", "accessToken": token,
		"profile": gilIDProfile{ID: owner, Username: "Developer"},
	})
}

func (b *AuthBroker) AuthenticateToken(token string) string {
	now := b.now().UTC()
	hash := sha256.Sum256([]byte(token))
	encoded := hex.EncodeToString(hash[:])
	b.mu.Lock()
	defer b.mu.Unlock()
	b.cleanupLocked(now)
	if session, ok := b.sessions[encoded]; ok && session.ExpiresAt.After(now) {
		return session.Owner
	}
	return ""
}

func (b *AuthBroker) exchangeAndLoadProfile(ctx context.Context, code string) (gilIDProfile, error) {
	payload, _ := json.Marshal(map[string]string{
		"grant_type": "authorization_code", "code": code, "client_id": b.clientID,
		"client_secret": b.clientSecret, "redirect_uri": b.redirectURI,
	})
	tokenRequest, err := http.NewRequestWithContext(ctx, http.MethodPost, b.tokenURL, bytes.NewReader(payload))
	if err != nil {
		return gilIDProfile{}, err
	}
	tokenRequest.Header.Set("Content-Type", "application/json")
	tokenResponse, err := b.httpClient.Do(tokenRequest)
	if err != nil {
		return gilIDProfile{}, err
	}
	defer tokenResponse.Body.Close()
	if tokenResponse.StatusCode != http.StatusOK {
		_, _ = io.Copy(io.Discard, io.LimitReader(tokenResponse.Body, 4096))
		return gilIDProfile{}, fmt.Errorf("token endpoint returned %d", tokenResponse.StatusCode)
	}
	var tokens struct {
		AccessToken string `json:"access_token"`
	}
	if err := json.NewDecoder(io.LimitReader(tokenResponse.Body, 1<<20)).Decode(&tokens); err != nil || tokens.AccessToken == "" {
		return gilIDProfile{}, errors.New("invalid token response")
	}

	profileRequest, err := http.NewRequestWithContext(ctx, http.MethodGet, b.profileURL, nil)
	if err != nil {
		return gilIDProfile{}, err
	}
	profileRequest.Header.Set("Authorization", "Bearer "+tokens.AccessToken)
	profileResponse, err := b.httpClient.Do(profileRequest)
	if err != nil {
		return gilIDProfile{}, err
	}
	defer profileResponse.Body.Close()
	if profileResponse.StatusCode != http.StatusOK {
		return gilIDProfile{}, fmt.Errorf("profile endpoint returned %d", profileResponse.StatusCode)
	}
	var profile gilIDProfile
	if err := json.NewDecoder(io.LimitReader(profileResponse.Body, 1<<20)).Decode(&profile); err != nil || profile.ID == "" {
		return gilIDProfile{}, errors.New("invalid profile response")
	}
	return profile, nil
}

func (b *AuthBroker) failPending(requestID, message string) {
	b.mu.Lock()
	defer b.mu.Unlock()
	if pending := b.pending[requestID]; pending != nil {
		pending.Error = message
	}
}

func (b *AuthBroker) cleanupLocked(now time.Time) {
	for id, pending := range b.pending {
		if !pending.ExpiresAt.After(now) {
			delete(b.pending, id)
			delete(b.stateToRequest, pending.State)
		}
	}
	for hash, session := range b.sessions {
		if !session.ExpiresAt.After(now) {
			delete(b.sessions, hash)
		}
	}
}

func writeCallbackPage(w http.ResponseWriter, title, message string) {
	w.Header().Set("Content-Type", "text/html; charset=utf-8")
	w.Header().Set("Content-Security-Policy", "default-src 'none'; style-src 'unsafe-inline'")
	_, _ = fmt.Fprintf(w, `<!doctype html><meta charset="utf-8"><title>%s</title><style>body{font:18px system-ui;background:#111827;color:#fff;display:grid;place-items:center;min-height:90vh}main{max-width:34rem;text-align:center}h1{color:#a78bfa}</style><main><h1>%s</h1><p>%s</p></main>`, html.EscapeString(title), html.EscapeString(title), html.EscapeString(message))
}
