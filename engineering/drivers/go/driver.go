package mmdb

import "fmt"

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
	var host string
	var port int

	// Simple parsing: assume host:port format
	fmt.Sscanf(name, "%s:%d", &host, &port)

	if host == "" {
		host = "localhost"
	}
	if port == 0 {
		port = 8080
	}

	return host, port, nil
}