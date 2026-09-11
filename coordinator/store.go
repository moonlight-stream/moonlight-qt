package main

import (
	"crypto/rand"
	"encoding/hex"
	"encoding/json"
	"errors"
	"os"
	"path/filepath"
	"sync"
	"time"
)

var (
	errPoolExhausted  = errors.New("VM pool exhausted")
	errLeaseNotFound  = errors.New("lease not found")
	errLeaseForbidden = errors.New("lease belongs to another user")
)

type VM struct {
	ID             string `json:"id"`
	DisplayName    string `json:"displayName"`
	DiscoveryName  string `json:"discoveryName,omitempty"`
	StreamAddress  string `json:"streamAddress"`
	StreamPort     int    `json:"streamPort"`
	PublicAddress  string `json:"publicAddress,omitempty"`
	PublicPort     int    `json:"publicStreamPort,omitempty"`
	SunshineAPIURL string `json:"sunshineApiUrl"`
	Enabled        bool   `json:"enabled"`
}

func (vm VM) ClientEndpoint() (string, int) {
	if vm.PublicAddress == "" {
		return vm.StreamAddress, vm.StreamPort
	}
	port := vm.PublicPort
	if port == 0 {
		port = vm.StreamPort
	}
	return vm.PublicAddress, port
}

func (s *Store) VMs() []VM {
	s.mu.Lock()
	defer s.mu.Unlock()
	return append([]VM(nil), s.vms...)
}

func (s *Store) UpdateVMs(updates map[string]VM) {
	s.mu.Lock()
	defer s.mu.Unlock()
	for index, vm := range s.vms {
		if updated, ok := updates[vm.ID]; ok {
			s.vms[index] = updated
		}
	}
}

type Lease struct {
	ID         string    `json:"leaseId"`
	VMID       string    `json:"vmId"`
	Owner      string    `json:"owner"`
	DeviceID   string    `json:"deviceId"`
	DeviceName string    `json:"deviceName"`
	CreatedAt  time.Time `json:"createdAt"`
	ExpiresAt  time.Time `json:"expiresAt"`
}

type persistedState struct {
	Leases []*Lease `json:"leases"`
}

type Store struct {
	mu        sync.Mutex
	vms       []VM
	leases    map[string]*Lease
	ttl       time.Duration
	stateFile string
	now       func() time.Time
}

func NewStore(vms []VM, ttl time.Duration, stateFile string) *Store {
	return &Store{
		vms:       append([]VM(nil), vms...),
		leases:    make(map[string]*Lease),
		ttl:       ttl,
		stateFile: stateFile,
		now:       time.Now,
	}
}

func (s *Store) Load() error {
	s.mu.Lock()
	defer s.mu.Unlock()

	data, err := os.ReadFile(s.stateFile)
	if errors.Is(err, os.ErrNotExist) {
		return nil
	}
	if err != nil {
		return err
	}

	var state persistedState
	if err := json.Unmarshal(data, &state); err != nil {
		return err
	}
	for _, lease := range state.Leases {
		if lease.ExpiresAt.After(s.now()) {
			s.leases[lease.ID] = lease
		}
	}
	return nil
}

func (s *Store) CreateOrRecover(owner, deviceID, deviceName string) (*Lease, VM, error) {
	s.mu.Lock()
	defer s.mu.Unlock()

	now := s.now().UTC()
	s.reapLocked(now)
	for _, lease := range s.leases {
		if lease.Owner == owner && lease.DeviceID == deviceID {
			lease.ExpiresAt = now.Add(s.ttl)
			vm, ok := s.vmByIDLocked(lease.VMID)
			if !ok || !vm.Enabled {
				delete(s.leases, lease.ID)
				break
			}
			if err := s.persistLocked(); err != nil {
				return nil, VM{}, err
			}
			return cloneLease(lease), vm, nil
		}
	}

	occupied := make(map[string]bool)
	for _, lease := range s.leases {
		occupied[lease.VMID] = true
	}
	for _, vm := range s.vms {
		if !vm.Enabled || occupied[vm.ID] {
			continue
		}
		id, err := randomID()
		if err != nil {
			return nil, VM{}, err
		}
		lease := &Lease{
			ID: id, VMID: vm.ID, Owner: owner, DeviceID: deviceID,
			DeviceName: deviceName, CreatedAt: now, ExpiresAt: now.Add(s.ttl),
		}
		s.leases[id] = lease
		if err := s.persistLocked(); err != nil {
			delete(s.leases, id)
			return nil, VM{}, err
		}
		return cloneLease(lease), vm, nil
	}
	return nil, VM{}, errPoolExhausted
}

func (s *Store) Heartbeat(id, owner string) (*Lease, error) {
	s.mu.Lock()
	defer s.mu.Unlock()

	now := s.now().UTC()
	s.reapLocked(now)
	lease, ok := s.leases[id]
	if !ok {
		return nil, errLeaseNotFound
	}
	if lease.Owner != owner {
		return nil, errLeaseForbidden
	}
	lease.ExpiresAt = now.Add(s.ttl)
	if err := s.persistLocked(); err != nil {
		return nil, err
	}
	return cloneLease(lease), nil
}

func (s *Store) Release(id, owner string) error {
	s.mu.Lock()
	defer s.mu.Unlock()

	lease, ok := s.leases[id]
	if !ok {
		return nil
	}
	if lease.Owner != owner {
		return errLeaseForbidden
	}
	delete(s.leases, id)
	return s.persistLocked()
}

func (s *Store) LeaseVM(id, owner string) (*Lease, VM, error) {
	s.mu.Lock()
	defer s.mu.Unlock()

	now := s.now().UTC()
	s.reapLocked(now)
	lease, ok := s.leases[id]
	if !ok {
		return nil, VM{}, errLeaseNotFound
	}
	if lease.Owner != owner {
		return nil, VM{}, errLeaseForbidden
	}
	vm, ok := s.vmByIDLocked(lease.VMID)
	if !ok || !vm.Enabled {
		return nil, VM{}, errLeaseNotFound
	}
	return cloneLease(lease), vm, nil
}

func (s *Store) reapLocked(now time.Time) {
	for id, lease := range s.leases {
		if !lease.ExpiresAt.After(now) {
			delete(s.leases, id)
		}
	}
}

func (s *Store) vmByIDLocked(id string) (VM, bool) {
	for _, vm := range s.vms {
		if vm.ID == id {
			return vm, true
		}
	}
	return VM{}, false
}

func (s *Store) persistLocked() error {
	state := persistedState{Leases: make([]*Lease, 0, len(s.leases))}
	for _, lease := range s.leases {
		state.Leases = append(state.Leases, lease)
	}
	data, err := json.MarshalIndent(state, "", "  ")
	if err != nil {
		return err
	}
	if err := os.MkdirAll(filepath.Dir(s.stateFile), 0o700); err != nil && filepath.Dir(s.stateFile) != "." {
		return err
	}
	tmp := s.stateFile + ".tmp"
	if err := os.WriteFile(tmp, data, 0o600); err != nil {
		return err
	}
	return os.Rename(tmp, s.stateFile)
}

func cloneLease(lease *Lease) *Lease {
	copy := *lease
	return &copy
}

func randomID() (string, error) {
	value := make([]byte, 24)
	if _, err := rand.Read(value); err != nil {
		return "", err
	}
	return hex.EncodeToString(value), nil
}
