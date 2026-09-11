package main

import (
	"context"
	"path/filepath"
	"testing"
	"time"
)

func TestSunshineDiscoveryUpdatesMatchingVM(t *testing.T) {
	store := NewStore([]VM{{
		ID: "vm-1", DiscoveryName: "v1", StreamAddress: "192.168.1.21",
		StreamPort: 47989, SunshineAPIURL: "https://192.168.1.21:47990", Enabled: true,
	}}, time.Minute, filepath.Join(t.TempDir(), "state.json"))
	discovery := NewSunshineDiscovery(time.Second)
	discovery.browse = func(context.Context) ([]discoveredSunshine, error) {
		return []discoveredSunshine{{
			instance: "v1", hostname: "v1.local.", address: "192.168.1.84", port: 47989,
		}}, nil
	}

	if err := discovery.Refresh(context.Background(), store); err != nil {
		t.Fatal(err)
	}
	vm := store.VMs()[0]
	if vm.StreamAddress != "192.168.1.84" || vm.StreamPort != 47989 {
		t.Fatalf("unexpected discovered stream endpoint: %#v", vm)
	}
	if vm.SunshineAPIURL != "https://192.168.1.84:47990" {
		t.Fatalf("unexpected discovered Sunshine URL: %s", vm.SunshineAPIURL)
	}
}

func TestSunshineDiscoveryDoesNotUpdateDifferentVM(t *testing.T) {
	store := NewStore([]VM{{
		ID: "vm-1", DiscoveryName: "v1", StreamAddress: "192.168.1.21",
		StreamPort: 47989, Enabled: true,
	}}, time.Minute, filepath.Join(t.TempDir(), "state.json"))
	discovery := NewSunshineDiscovery(time.Second)
	discovery.browse = func(context.Context) ([]discoveredSunshine, error) {
		return []discoveredSunshine{{instance: "v2", address: "192.168.1.84", port: 47989}}, nil
	}

	if err := discovery.Refresh(context.Background(), store); err != nil {
		t.Fatal(err)
	}
	if got := store.VMs()[0].StreamAddress; got != "192.168.1.21" {
		t.Fatalf("unmatched VM address changed to %s", got)
	}
}
