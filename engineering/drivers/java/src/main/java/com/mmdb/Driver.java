package com.mmdb;

import java.sql.Connection;
import java.sql.Driver;
import java.sql.DriverPropertyInfo;
import java.sql.SQLException;
import java.util.Properties;

public class MMDBDriver implements Driver {

    @Override
    public Connection connect(String url, Properties info) throws SQLException {
        if (!acceptsURL(url)) {
            return null;
        }
        // Parse URL: mmdb://host:port
        String host = "localhost";
        int port = 8080;

        if (url != null && url.startsWith("jdbc:mmdb://")) {
            String urlPart = url.substring("jdbc:mmdb://".length());
            String[] parts = urlPart.split(":");
            if (parts.length >= 1 && !parts[0].isEmpty()) {
                host = parts[0];
            }
            if (parts.length >= 2) {
                try {
                    port = Integer.parseInt(parts[1]);
                } catch (NumberFormatException e) {
                    // Use default port
                }
            }
        }

        return new MMDBConnection(host, port);
    }

    @Override
    public boolean acceptsURL(String url) {
        return url != null && url.startsWith("jdbc:mmdb://");
    }

    @Override
    public DriverPropertyInfo[] getPropertyInfo(String url, Properties info) {
        return new DriverPropertyInfo[0];
    }

    @Override
    public int getMajorVersion() {
        return 1;
    }

    @Override
    public int getMinorVersion() {
        return 0;
    }

    @Override
    public boolean jdbcCompliant() {
        return false;
    }
}
