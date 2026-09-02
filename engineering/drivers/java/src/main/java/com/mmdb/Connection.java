package com.mmdb;

import java.sql.Connection;
import java.sql.SQLException;
import java.sql.Statement;

public class MMDBConnection implements Connection {

    private String host;
    private int port;
    private boolean closed = false;

    public MMDBConnection(String host, int port) {
        this.host = host;
        this.port = port;
    }

    @Override
    public Statement createStatement() throws SQLException {
        checkClosed();
        return new MMDBStatement();
    }

    @Override
    public void close() throws SQLException {
        closed = true;
    }

    @Override
    public boolean isClosed() throws SQLException {
        return closed;
    }

    // Placeholder implementations for other Connection methods
    @Override
    public java.sql.PreparedStatement prepareStatement(String sql) throws SQLException {
        throw new SQLException("Not implemented");
    }

    @Override
    public java.sql.CallableStatement prepareCall(String sql) throws SQLException {
        throw new SQLException("Not implemented");
    }

    @Override
    public String nativeSQL(String sql) throws SQLException {
        throw new SQLException("Not implemented");
    }

    @Override
    public void setAutoCommit(boolean autoCommit) throws SQLException {
        throw new SQLException("Not implemented");
    }

    @Override
    public boolean getAutoCommit() throws SQLException {
        return true;
    }

    @Override
    public void commit() throws SQLException {
        throw new SQLException("Not implemented");
    }

    @Override
    public void rollback() throws SQLException {
        throw new SQLException("Not implemented");
    }

    // Additional Connection methods - using default implementations
    public String getHost() {
        return host;
    }

    public int getPort() {
        return port;
    }
}
