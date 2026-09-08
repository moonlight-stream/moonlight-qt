package main

import (
	"crypto/sha256"
	"crypto/subtle"
	"encoding/hex"
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
	Listen          string            `json:"listen"`
	LeaseTTLSeconds int               `json:"leaseTTLSeconds"`
	StateFile       string            `json:"stateFile"`
	TokenHashes     map[string]string `json:"tokenHashes"`
	VMs             []VM              `json:"vms"`
}

type Server struct {
	store       *Store
	tokenHashes map[string][]byte
}

func main() {
	configPath := flag.String("config", "config.json", "path to coordinator configuration")
	hashToken := flag.String("hash-token", "", "print the SHA-256 hash for a new access token")
	flag.Parse()
	if *hashToken != "" {
		sum := sha256.Sum256([]byte(*hashToken))
		fmt.Println(hex.EncodeToString(sum[:]))
		return
	}

	config, err := loadConfig(*configPath)
	if err != nil {
		log.Fatal(err)
	}
	store := NewStore(config.VMs, time.Duration(config.LeaseTTLSeconds)*time.Second, config.StateFile)
	if err := store.Load(); err != nil {
		log.Fatalf("load lease state: %v", err)
	}
	server, err := newServer(store, config.TokenHashes)
	if err != nil {
		log.Fatal(err)
	}

	mux := http.NewServeMux()
	mux.HandleFunc("GET /healthz", server.health)
	mux.HandleFunc("POST /v1/leases", server.auth(server.createLease))
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
	if len(config.TokenHashes) == 0 || len(config.VMs) == 0 {
		return Config{}, errors.New("at least one token hash and VM are required")
	}
	for _, vm := range config.VMs {
		if vm.ID == "" || vm.StreamAddress == "" || vm.StreamPort < 1 || vm.StreamPort > 65535 {
			return Config{}, fmt.Errorf("invalid VM configuration for %q", vm.ID)
		}
	}
	return config, nil
}

func newServer(store *Store, hashes map[string]string) (*Server, error) {
	decoded := make(map[string][]byte, len(hashes))
	for owner, value := range hashes {
		bytes, err := hex.DecodeString(value)
		if err != nil || len(bytes) != sha256.Size {
			return nil, fmt.Errorf("token hash for %q must be a SHA-256 hex string", owner)
		}
		decoded[owner] = bytes
	}
	return &Server{store: store, tokenHashes: decoded}, nil
}

func (s *Server) auth(next func(http.ResponseWriter, *http.Request, string)) http.HandlerFunc {
	return func(w http.ResponseWriter, r *http.Request) {
		header := r.Header.Get("Authorization")
		if !strings.HasPrefix(header, "Bearer ") {
			writeError(w, http.StatusUnauthorized, "UNAUTHORIZED", "missing bearer token")
			return
		}
		sum := sha256.Sum256([]byte(strings.TrimPrefix(header, "Bearer ")))
		owner := ""
		for candidate, expected := range s.tokenHashes {
			if subtle.ConstantTimeCompare(sum[:], expected) == 1 {
				owner = candidate
			}
		}
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
	writeJSON(w, http.StatusCreated, map[string]any{
		"leaseId": lease.ID, "state": "reserved", "expiresAt": lease.ExpiresAt,
		"host":            map[string]any{"name": vm.DisplayName, "address": vm.StreamAddress, "port": vm.StreamPort},
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
