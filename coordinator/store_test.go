package main

import (
	"errors"
	"path/filepath"
	"strconv"
	"sync"
	"testing"
	"time"
)

func testStore(t *testing.T) *Store {
	t.Helper()
	store := NewStore([]VM{
		{ID: "vm-1", StreamAddress: "vm1", StreamPort: 47989, Enabled: true},
		{ID: "vm-2", StreamAddress: "vm2", StreamPort: 47989, Enabled: true},
	}, time.Minute, filepath.Join(t.TempDir(), "state.json"))
	now := time.Date(2026, 9, 7, 20, 0, 0, 0, time.UTC)
	store.now = func() time.Time { return now }
	return store
}

func TestPoolCapacityAndRelease(t *testing.T) {
	store := testStore(t)
	one, vmOne, err := store.CreateOrRecover("user-1", "device-1", "one")
	if err != nil {
		t.Fatal(err)
	}
	two, vmTwo, err := store.CreateOrRecover("user-2", "device-2", "two")
	if err != nil {
		t.Fatal(err)
	}
	if vmOne.ID == vmTwo.ID {
		t.Fatal("two leases received the same VM")
	}
	if _, _, err := store.CreateOrRecover("user-3", "device-3", "three"); !errors.Is(err, errPoolExhausted) {
		t.Fatalf("expected pool exhaustion, got %v", err)
	}
	if err := store.Release(one.ID, "user-1"); err != nil {
		t.Fatal(err)
	}
	three, vmThree, err := store.CreateOrRecover("user-3", "device-3", "three")
	if err != nil || three == nil || vmThree.ID != vmOne.ID {
		t.Fatalf("released VM was not reassigned: lease=%v vm=%v err=%v", three, vmThree, err)
	}
	_ = two
}

func TestCreateIsIdempotentPerOwnerAndDevice(t *testing.T) {
	store := testStore(t)
	one, _, err := store.CreateOrRecover("user-1", "device-1", "one")
	if err != nil {
		t.Fatal(err)
	}
	again, _, err := store.CreateOrRecover("user-1", "device-1", "renamed")
	if err != nil {
		t.Fatal(err)
	}
	if one.ID != again.ID {
		t.Fatalf("expected recovered lease %s, got %s", one.ID, again.ID)
	}
}

func TestExpiredLeaseIsReclaimed(t *testing.T) {
	store := testStore(t)
	current := store.now()
	store.ttl = time.Second
	one, vmOne, err := store.CreateOrRecover("user-1", "device-1", "one")
	if err != nil {
		t.Fatal(err)
	}
	store.now = func() time.Time { return current.Add(2 * time.Second) }
	if _, err := store.Heartbeat(one.ID, "user-1"); !errors.Is(err, errLeaseNotFound) {
		t.Fatalf("expected expired lease, got %v", err)
	}
	_, vmTwo, err := store.CreateOrRecover("user-2", "device-2", "two")
	if err != nil || vmTwo.ID != vmOne.ID {
		t.Fatalf("expired VM was not reclaimed: vm=%v err=%v", vmTwo, err)
	}
}

func TestLeaseOwnership(t *testing.T) {
	store := testStore(t)
	lease, _, err := store.CreateOrRecover("user-1", "device-1", "one")
	if err != nil {
		t.Fatal(err)
	}
	if err := store.Release(lease.ID, "user-2"); !errors.Is(err, errLeaseForbidden) {
		t.Fatalf("expected forbidden, got %v", err)
	}
}

func TestConcurrentRequestsCannotDoubleAssign(t *testing.T) {
	store := testStore(t)
	const requestCount = 20

	type result struct {
		vm  VM
		err error
	}
	results := make(chan result, requestCount)
	var wait sync.WaitGroup
	for i := 0; i < requestCount; i++ {
		wait.Add(1)
		go func(index int) {
			defer wait.Done()
			_, vm, err := store.CreateOrRecover(
				"user-"+strconv.Itoa(index),
				"device-"+strconv.Itoa(index),
				"device",
			)
			results <- result{vm: vm, err: err}
		}(i)
	}
	wait.Wait()
	close(results)

	assigned := make(map[string]bool)
	for result := range results {
		if errors.Is(result.err, errPoolExhausted) {
			continue
		}
		if result.err != nil {
			t.Fatal(result.err)
		}
		if assigned[result.vm.ID] {
			t.Fatalf("VM %s was assigned twice", result.vm.ID)
		}
		assigned[result.vm.ID] = true
	}
	if len(assigned) != 2 {
		t.Fatalf("expected exactly two assigned VMs, got %d", len(assigned))
	}
}
