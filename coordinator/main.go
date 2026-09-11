package main

import (
	"context"
	"encoding/json"
	"errors"
	"flag"
	"fmt"
	"log"
	"net/http"
	"os"
	"strings"
	"time"
)

type Config struct {
	Listen          string `json:"listen"`
	LeaseTTLSeconds int    `json:"leaseTTLSeconds"`
	StateFile       string `json:"stateFile"`
	DevAuthEnabled  bool   `json:"devAuthEnabled"`
	VMs             []VM   `json:"vms"`
}

type Server struct {
	store      *Store
	authBroker *AuthBroker
	pairer     *SunshinePairer
	discovery  *SunshineDiscovery
}

func main() {
	configPath := flag.String("config", "config.json", "path to coordinator configuration")
	envPath := flag.String("env-file", ".env", "path to optional environment file")
	discoverOnly := flag.Bool("discover-only", false, "discover Sunshine VMs, print their endpoints, and exit")
	flag.Parse()
	if err := loadDotEnv(*envPath); err != nil {
		log.Fatalf("load environment file: %v", err)
	}

	config, err := loadConfig(*configPath)
	if err != nil {
		log.Fatal(err)
	}
	store := NewStore(config.VMs, time.Duration(config.LeaseTTLSeconds)*time.Second, config.StateFile)
	if err := store.Load(); err != nil {
		log.Fatalf("load lease state: %v", err)
	}
	discovery := NewSunshineDiscovery(3 * time.Second)
	if *discoverOnly {
		if err := discovery.Refresh(context.Background(), store); err != nil {
			log.Fatal(err)
		}
		for _, vm := range store.VMs() {
			log.Printf("VM %s endpoint: %s:%d", vm.ID, vm.StreamAddress, vm.StreamPort)
		}
		return
	}
	authBroker, err := NewAuthBrokerFromEnvironment(config.DevAuthEnabled)
	if err != nil {
		log.Fatal(err)
	}
	server := newServer(store, authBroker)
	server.discovery = discovery

	mux := http.NewServeMux()
	mux.HandleFunc("GET /healthz", server.health)
	mux.HandleFunc("POST /v1/auth/start", server.authBroker.Start)
	mux.HandleFunc("GET /v1/auth/status/{requestId}", server.authBroker.Status)
	mux.HandleFunc("GET /auth/callback", server.authBroker.Callback)
	mux.HandleFunc("POST /v1/auth/dev", server.authBroker.DevLogin)
	mux.HandleFunc("POST /v1/leases", server.auth(server.createLease))
	mux.HandleFunc("POST /v1/leases/{leaseId}/pair", server.auth(server.pairLease))
	mux.HandleFunc("POST /v1/leases/{leaseId}/heartbeat", server.auth(server.heartbeat))
	mux.HandleFunc("DELETE /v1/leases/{leaseId}", server.auth(server.release))

	httpServer := &http.Server{
		Addr:              config.Listen,
		Handler:           mux,
		ReadHeaderTimeout: 5 * time.Second,
		ReadTimeout:       10 * time.Second,
		WriteTimeout:      10 * time.Second,
		IdleTimeout:       60 * time.Second,
	}
	log.Printf("GilStreaming coordinator listening on %s", config.Listen)
	log.Fatal(httpServer.ListenAndServe())
}

func loadConfig(path string) (Config, error) {
	data, err := os.ReadFile(path)
	if err != nil {
		return Config{}, err
	}
	var config Config
	if err := json.Unmarshal(data, &config); err != nil {
		return Config{}, err
	}
	if config.Listen == "" || config.StateFile == "" || config.LeaseTTLSeconds < 30 {
		return Config{}, errors.New("listen, stateFile, and leaseTTLSeconds >= 30 are required")
	}
	if len(config.VMs) == 0 {
		return Config{}, errors.New("at least one VM is required")
	}
	for _, vm := range config.VMs {
		if vm.ID == "" || vm.StreamAddress == "" || vm.StreamPort < 1 || vm.StreamPort > 65535 {
			return Config{}, fmt.Errorf("invalid VM configuration for %q", vm.ID)
		}
		if vm.PublicPort < 0 || vm.PublicPort > 65535 || (vm.PublicAddress != "" && vm.PublicPort == 0) {
			return Config{}, fmt.Errorf("invalid public endpoint for VM %q", vm.ID)
		}
	}
	return config, nil
}

func newServer(store *Store, authBroker *AuthBroker) *Server {
	return &Server{store: store, authBroker: authBroker, pairer: NewSunshinePairer()}
}

func (s *Server) auth(next func(http.ResponseWriter, *http.Request, string)) http.HandlerFunc {
	return func(w http.ResponseWriter, r *http.Request) {
		header := r.Header.Get("Authorization")
		if !strings.HasPrefix(header, "Bearer ") {
			writeError(w, http.StatusUnauthorized, "UNAUTHORIZED", "missing bearer token")
			return
		}
		owner := s.authBroker.AuthenticateToken(strings.TrimPrefix(header, "Bearer "))
		if owner == "" {
			writeError(w, http.StatusUnauthorized, "UNAUTHORIZED", "invalid bearer token")
			return
		}
		next(w, r, owner)
	}
}

func (s *Server) health(w http.ResponseWriter, _ *http.Request) {
	writeJSON(w, http.StatusOK, map[string]string{"status": "ok"})
}

func (s *Server) createLease(w http.ResponseWriter, r *http.Request, owner string) {
	var request struct {
		DeviceID   string `json:"deviceId"`
		DeviceName string `json:"deviceName"`
	}
	if err := decodeJSON(w, r, &request); err != nil || request.DeviceID == "" {
		writeError(w, http.StatusBadRequest, "INVALID_REQUEST", "deviceId is required")
		return
	}
	if s.discovery != nil {
		if err := s.discovery.Refresh(r.Context(), s.store); err != nil {
			log.Printf("Sunshine discovery failed; using last known VM addresses: %v", err)
		}
	}
	lease, vm, err := s.store.CreateOrRecover(owner, request.DeviceID, request.DeviceName)
	if errors.Is(err, errPoolExhausted) {
		writeJSON(w, http.StatusConflict, map[string]any{
			"code": "POOL_EXHAUSTED", "message": "all gaming VMs are in use", "retryAfterSeconds": 15,
		})
		return
	}
	if err != nil {
		writeError(w, http.StatusInternalServerError, "INTERNAL_ERROR", "could not create lease")
		return
	}
	clientAddress, clientPort := vm.ClientEndpoint()
	writeJSON(w, http.StatusCreated, map[string]any{
		"leaseId": lease.ID, "state": "reserved", "expiresAt": lease.ExpiresAt,
		"host":            map[string]any{"name": vm.DisplayName, "address": clientAddress, "port": clientPort},
		"pairingRequired": true,
	})
}

func (s *Server) heartbeat(w http.ResponseWriter, r *http.Request, owner string) {
	lease, err := s.store.Heartbeat(r.PathValue("leaseId"), owner)
	if errors.Is(err, errLeaseNotFound) {
		writeError(w, http.StatusGone, "LEASE_EXPIRED", "lease is missing or expired")
		return
	}
	if errors.Is(err, errLeaseForbidden) {
		writeError(w, http.StatusForbidden, "FORBIDDEN", "lease belongs to another user")
		return
	}
	if err != nil {
		writeError(w, http.StatusInternalServerError, "INTERNAL_ERROR", "could not renew lease")
		return
	}
	writeJSON(w, http.StatusOK, map[string]any{"leaseId": lease.ID, "expiresAt": lease.ExpiresAt})
}

func (s *Server) pairLease(w http.ResponseWriter, r *http.Request, owner string) {
	var request struct {
		PIN        string `json:"pin"`
		DeviceName string `json:"deviceName"`
	}
	if err := decodeJSON(w, r, &request); err != nil || len(request.PIN) != 4 || request.DeviceName == "" {
		writeError(w, http.StatusBadRequest, "INVALID_REQUEST", "a four-digit pin and deviceName are required")
		return
	}
	for _, digit := range request.PIN {
		if digit < '0' || digit > '9' {
			writeError(w, http.StatusBadRequest, "INVALID_REQUEST", "pin must contain four digits")
			return
		}
	}
	_, vm, err := s.store.LeaseVM(r.PathValue("leaseId"), owner)
	if errors.Is(err, errLeaseNotFound) {
		writeError(w, http.StatusGone, "LEASE_EXPIRED", "lease is missing or expired")
		return
	}
	if errors.Is(err, errLeaseForbidden) {
		writeError(w, http.StatusForbidden, "FORBIDDEN", "lease belongs to another user")
		return
	}
	if err != nil {
		writeError(w, http.StatusInternalServerError, "INTERNAL_ERROR", "could not load lease")
		return
	}
	if err := s.pairer.Pair(r.Context(), vm, request.PIN, request.DeviceName); err != nil {
		log.Printf("automatic pairing failed for VM %s: %v", vm.ID, err)
		writeError(w, http.StatusBadGateway, "PAIRING_FAILED", err.Error())
		return
	}
	writeJSON(w, http.StatusOK, map[string]bool{"paired": true})
}

func (s *Server) release(w http.ResponseWriter, r *http.Request, owner string) {
	err := s.store.Release(r.PathValue("leaseId"), owner)
	if errors.Is(err, errLeaseForbidden) {
		writeError(w, http.StatusForbidden, "FORBIDDEN", "lease belongs to another user")
		return
	}
	if err != nil {
		writeError(w, http.StatusInternalServerError, "INTERNAL_ERROR", "could not release lease")
		return
	}
	w.WriteHeader(http.StatusNoContent)
}

func decodeJSON(w http.ResponseWriter, r *http.Request, target any) error {
	decoder := json.NewDecoder(http.MaxBytesReader(w, r.Body, 1<<20))
	decoder.DisallowUnknownFields()
	return decoder.Decode(target)
}

func writeJSON(w http.ResponseWriter, status int, value any) {
	w.Header().Set("Content-Type", "application/json")
	w.WriteHeader(status)
	_ = json.NewEncoder(w).Encode(value)
}

func writeError(w http.ResponseWriter, status int, code, message string) {
	writeJSON(w, status, map[string]string{"code": code, "message": message})
}
