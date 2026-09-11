package main

import (
	"bufio"
	"errors"
	"fmt"
	"os"
	"strings"
)

// loadDotEnv loads development configuration without overriding variables that
// are already present in the process environment. A missing file is allowed so
// production deployments can provide secrets through their service manager.
func loadDotEnv(path string) error {
	file, err := os.Open(path)
	if errors.Is(err, os.ErrNotExist) {
		return nil
	}
	if err != nil {
		return err
	}
	defer file.Close()

	scanner := bufio.NewScanner(file)
	for lineNumber := 1; scanner.Scan(); lineNumber++ {
		name, value, ok, err := parseDotEnvLine(scanner.Text())
		if err != nil {
			return fmt.Errorf("%s:%d: %w", path, lineNumber, err)
		}
		if !ok {
			continue
		}
		if _, exists := os.LookupEnv(name); exists {
			continue
		}
		if err := os.Setenv(name, value); err != nil {
			return fmt.Errorf("set %s: %w", name, err)
		}
	}
	return scanner.Err()
}

func parseDotEnvLine(line string) (name, value string, ok bool, err error) {
	line = strings.TrimSpace(line)
	if line == "" || strings.HasPrefix(line, "#") {
		return "", "", false, nil
	}
	line = strings.TrimSpace(strings.TrimPrefix(line, "export "))
	separator := strings.IndexByte(line, '=')
	if separator <= 0 {
		return "", "", false, errors.New("expected NAME=VALUE")
	}
	name = strings.TrimSpace(line[:separator])
	value = strings.TrimSpace(line[separator+1:])
	if !validEnvironmentName(name) {
		return "", "", false, fmt.Errorf("invalid variable name %q", name)
	}

	if len(value) >= 2 && ((value[0] == '"' && value[len(value)-1] == '"') ||
		(value[0] == '\'' && value[len(value)-1] == '\'')) {
		value = value[1 : len(value)-1]
	} else if strings.HasPrefix(value, "\"") || strings.HasPrefix(value, "'") {
		return "", "", false, errors.New("unterminated quoted value")
	}
	return name, value, true, nil
}

func validEnvironmentName(name string) bool {
	if name == "" {
		return false
	}
	for index, character := range name {
		if (character >= 'A' && character <= 'Z') ||
			(character >= 'a' && character <= 'z') || character == '_' ||
			(index > 0 && character >= '0' && character <= '9') {
			continue
		}
		return false
	}
	return true
}
