package main

import (
	"context"
	"fmt"
	"log"
	"net"
	"net/url"
	"strings"
	"time"

	"github.com/grandcat/zeroconf"
)

const sunshineMDNSService = "_nvstream._tcp"

type discoveredSunshine struct {
	instance string
	hostname string
	address  string
	port     int
}

type SunshineDiscovery struct {
	timeout time.Duration
	browse  func(context.Context) ([]discoveredSunshine, error)
}

func NewSunshineDiscovery(timeout time.Duration) *SunshineDiscovery {
	discovery := &SunshineDiscovery{timeout: timeout}
	discovery.browse = discovery.browseMDNS
	return discovery
}

// Refresh discovers configured Sunshine hosts by their stable mDNS name and
// updates the coordinator's in-memory pool endpoints. Configured IP addresses
// remain as a fallback when mDNS is temporarily unavailable.
func (d *SunshineDiscovery) Refresh(ctx context.Context, store *Store) error {
	ctx, cancel := context.WithTimeout(ctx, d.timeout)
	defer cancel()

	entries, err := d.browse(ctx)
	if err != nil {
		return err
	}

	updates := make(map[string]VM)
	for _, vm := range store.VMs() {
		if vm.DiscoveryName == "" {
			continue
		}
		for _, entry := range entries {
			if !discoveryNameMatches(vm.DiscoveryName, entry) || net.ParseIP(entry.address) == nil {
				continue
			}
			updated := vm
			updated.StreamAddress = entry.address
			if entry.port > 0 {
				updated.StreamPort = entry.port
			}
			updated.SunshineAPIURL = sunshineURLAtAddress(vm.SunshineAPIURL, entry.address)
			updates[vm.ID] = updated
			break
		}
	}

	store.UpdateVMs(updates)
	for id, vm := range updates {
		log.Printf("discovered Sunshine VM %s at %s:%d", id, vm.StreamAddress, vm.StreamPort)
	}
	return nil
}

func (d *SunshineDiscovery) browseMDNS(ctx context.Context) ([]discoveredSunshine, error) {
	resolver, err := zeroconf.NewResolver(nil)
	if err != nil {
		return nil, fmt.Errorf("create mDNS resolver: %w", err)
	}

	results := make(chan *zeroconf.ServiceEntry)
	if err := resolver.Browse(ctx, sunshineMDNSService, "local.", results); err != nil {
		return nil, fmt.Errorf("browse Sunshine mDNS service: %w", err)
	}

	var discovered []discoveredSunshine
	for {
		select {
		case <-ctx.Done():
			return discovered, nil
		case entry, ok := <-results:
			if !ok {
				return discovered, nil
			}
			for _, address := range entry.AddrIPv4 {
				discovered = append(discovered, discoveredSunshine{
					instance: entry.Instance,
					hostname: entry.HostName,
					address:  address.String(),
					port:     entry.Port,
				})
			}
		}
	}
}

func discoveryNameMatches(expected string, entry discoveredSunshine) bool {
	expected = normalizeDiscoveryName(expected)
	return expected != "" && (expected == normalizeDiscoveryName(entry.instance) ||
		expected == normalizeDiscoveryName(entry.hostname))
}

func normalizeDiscoveryName(value string) string {
	value = strings.TrimSuffix(strings.TrimSpace(strings.ToLower(value)), ".")
	value = strings.TrimSuffix(value, ".local")
	if dot := strings.IndexByte(value, '.'); dot >= 0 {
		value = value[:dot]
	}
	return value
}

func sunshineURLAtAddress(existing, address string) string {
	port := "47990"
	scheme := "https"
	if parsed, err := url.Parse(existing); err == nil {
		if parsed.Scheme != "" {
			scheme = parsed.Scheme
		}
		if parsed.Port() != "" {
			port = parsed.Port()
		}
	}
	return scheme + "://" + net.JoinHostPort(address, port)
}
