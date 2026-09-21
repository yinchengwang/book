package mmdb

import (
	"fmt"
	"net"
	"strconv"
)

type Driver struct{}

func (d *Driver) Open(name string) (*Conn, error) {
	// Parse DSN and create connection
	// DSN format: host:port
	host := "localhost"
	port := 8080

	if name != "" {
		var err error
		host, port, err = parseDSN(name)
		if err != nil {
			return nil, err
		}
	}

	return &Conn{
		host: host,
		port: port,
	}, nil
}

// parseDSN parses a DSN string in the format host:port
func parseDSN(name string) (string, int, error) {
	host, portStr, err := net.SplitHostPort(name)
	if err != nil {
		return "", 0, fmt.Errorf("invalid DSN %q (expect host:port): %w", name, err)
	}
	port, err := strconv.Atoi(portStr)
	if err != nil {
		return "", 0, fmt.Errorf("invalid port in DSN %q: %w", name, err)
	}
	return host, port, nil
}
