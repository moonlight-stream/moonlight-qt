package main

import (
	"bytes"
	"context"
	"crypto/tls"
	"encoding/json"
	"errors"
	"fmt"
	"io"
	"net/http"
	"net/url"
	"os"
	"strings"
	"time"
)

var (
	errSunshineCredentials = errors.New("Sunshine credentials are not configured")
	errNoPendingPairing    = errors.New("Sunshine has no pending pairing request")
)

type SunshinePairer struct {
	client *http.Client
	now    func() time.Time
}

func NewSunshinePairer() *SunshinePairer {
	transport := http.DefaultTransport.(*http.Transport).Clone()
	transport.TLSClientConfig = &tls.Config{MinVersion: tls.VersionTLS12, InsecureSkipVerify: true} // Sunshine uses a self-signed Web UI certificate.
	return &SunshinePairer{
		client: &http.Client{Transport: transport, Timeout: 3 * time.Second},
		now:    time.Now,
	}
}

func (p *SunshinePairer) Pair(ctx context.Context, vm VM, pin, deviceName string) error {
	const usernameEnv = "SUNSHINE_USERNAME"
	const passwordEnv = "SUNSHINE_PASSWORD"
	username, password := os.Getenv(usernameEnv), os.Getenv(passwordEnv)
	if username == "" || password == "" {
		return fmt.Errorf("%w (%s and %s)", errSunshineCredentials, usernameEnv, passwordEnv)
	}

	baseURL := vm.SunshineAPIURL
	if baseURL == "" {
		baseURL = fmt.Sprintf("https://%s:47990", vm.StreamAddress)
	}
	parsed, err := url.Parse(baseURL)
	if err != nil || parsed.Scheme != "https" || parsed.Host == "" {
		return fmt.Errorf("invalid Sunshine API URL %q", baseURL)
	}

	deadline := p.now().Add(8 * time.Second)
	for {
		err = p.tryPair(ctx, parsed, username, password, pin, deviceName)
		if !errors.Is(err, errNoPendingPairing) || !p.now().Before(deadline) {
			return err
		}
		select {
		case <-ctx.Done():
			return ctx.Err()
		case <-time.After(250 * time.Millisecond):
		}
	}
}

func (p *SunshinePairer) tryPair(ctx context.Context, baseURL *url.URL, username, password, pin, deviceName string) error {
	pairingID, modernAPI, err := p.pendingPairingID(ctx, baseURL, username, password)
	if err != nil {
		return err
	}
	if modernAPI && pairingID == "" {
		return errNoPendingPairing
	}

	payload := map[string]string{"pin": pin, "name": deviceName}
	if pairingID != "" {
		payload["pairing_id"] = pairingID
	}
	body, _ := json.Marshal(payload)
	request, err := http.NewRequestWithContext(ctx, http.MethodPost, sunshineEndpoint(baseURL, "/api/pin"), bytes.NewReader(body))
	if err != nil {
		return err
	}
	request.SetBasicAuth(username, password)
	request.Header.Set("Content-Type", "application/json")
	request.Header.Set("Accept", "application/json")
	response, err := p.client.Do(request)
	if err != nil {
		return fmt.Errorf("contact Sunshine: %w", err)
	}
	defer response.Body.Close()
	responseBody, _ := io.ReadAll(io.LimitReader(response.Body, 1<<20))
	if response.StatusCode == http.StatusUnauthorized {
		return errors.New("Sunshine rejected its Web UI credentials")
	}
	if response.StatusCode < 200 || response.StatusCode >= 300 {
		if response.StatusCode == http.StatusNotFound || response.StatusCode == http.StatusConflict {
			return errNoPendingPairing
		}
		return fmt.Errorf("Sunshine pairing returned HTTP %d: %s", response.StatusCode, strings.TrimSpace(string(responseBody)))
	}
	var result struct {
		Status any `json:"status"`
	}
	if err := json.Unmarshal(responseBody, &result); err != nil {
		return fmt.Errorf("decode Sunshine pairing response: %w", err)
	}
	if result.Status != true && result.Status != "true" {
		return errNoPendingPairing
	}
	return nil
}

func (p *SunshinePairer) pendingPairingID(ctx context.Context, baseURL *url.URL, username, password string) (string, bool, error) {
	request, err := http.NewRequestWithContext(ctx, http.MethodGet, sunshineEndpoint(baseURL, "/api/pin"), nil)
	if err != nil {
		return "", false, err
	}
	request.SetBasicAuth(username, password)
	request.Header.Set("Accept", "application/json")
	response, err := p.client.Do(request)
	if err != nil {
		return "", false, fmt.Errorf("contact Sunshine: %w", err)
	}
	defer response.Body.Close()
	body, _ := io.ReadAll(io.LimitReader(response.Body, 1<<20))
	if response.StatusCode == http.StatusUnauthorized {
		return "", false, errors.New("Sunshine rejected its Web UI credentials")
	}
	if response.StatusCode == http.StatusNotFound || response.StatusCode == http.StatusMethodNotAllowed {
		return "", false, nil
	}
	if response.StatusCode < 200 || response.StatusCode >= 300 {
		return "", false, fmt.Errorf("list Sunshine pairings returned HTTP %d", response.StatusCode)
	}
	var result struct {
		Pairings []struct {
			ID string `json:"id"`
		} `json:"pairings"`
	}
	if err := json.Unmarshal(body, &result); err != nil {
		// Older Sunshine versions may route GET /api/pin to HTML. Use their
		// legacy POST body, which does not include a pairing ID.
		return "", false, nil
	}
	if len(result.Pairings) == 0 {
		return "", true, nil
	}
	if len(result.Pairings) > 1 {
		return "", true, errors.New("Sunshine reported multiple pending pairing requests")
	}
	return result.Pairings[0].ID, true, nil
}

func sunshineEndpoint(baseURL *url.URL, path string) string {
	endpoint := *baseURL
	endpoint.Path = path
	endpoint.RawQuery = ""
	endpoint.Fragment = ""
	return endpoint.String()
}
