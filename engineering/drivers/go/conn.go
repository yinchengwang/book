package mmdb

import (
	"fmt"
	"net"
)

type Conn struct {
	host string
	port int
	conn net.Conn
}

func (c *Conn) Query(query string) (*Rows, error) {
	// Execute query
	// Connect to the server and send the query
	if c.conn == nil {
		conn, err := net.Dial("tcp", fmt.Sprintf("%s:%d", c.host, c.port))
		if err != nil {
			return nil, err
		}
		c.conn = conn
	}

	// Send query to server
	_, err := c.conn.Write([]byte(query))
	if err != nil {
		return nil, err
	}

	return &Rows{}, nil
}

func (c *Conn) Close() error {
	// Close connection
	if c.conn != nil {
		return c.conn.Close()
	}
	return nil
}

type Rows struct{}
