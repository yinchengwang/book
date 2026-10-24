package com.mmdb;

import java.sql.Array;
import java.sql.Blob;
import java.sql.CallableStatement;
import java.sql.Clob;
import java.sql.Connection;
import java.sql.DatabaseMetaData;
import java.sql.NClob;
import java.sql.PreparedStatement;
import java.sql.ResultSet;
import java.sql.SQLFeatureNotSupportedException;
import java.sql.SQLWarning;
import java.sql.SQLXML;
import java.sql.Savepoint;
import java.sql.SQLException;
import java.sql.Statement;
import java.sql.Struct;
import java.util.Map;
import java.util.Properties;
import java.util.concurrent.Executor;

/**
 * MMDB JDBC Connection —— 可编译骨架。
 *
 * 当前仅本地状态（host/port/closed）真实生效；语句执行与高级 JDBC 能力
 * （预编译语句、元数据、LOB、Savepoint、网络超时等）尚未接线到服务器协议
 * （rest_server / grpc_server），调用时抛出 SQLFeatureNotSupportedException。
 * 待 gap10 后续迭代按协议补齐。
 */
public class MMDBConnection implements Connection {

    private final String host;
    private final int port;
    private boolean closed = false;

    public MMDBConnection(String host, int port) {
        this.host = host;
        this.port = port;
    }

    public String getHost() {
        return host;
    }

    public int getPort() {
        return port;
    }

    private void checkClosed() throws SQLException {
        if (closed) {
            throw new SQLException("Connection is closed");
        }
    }

    private static SQLFeatureNotSupportedException unsupported(String feature) {
        return new SQLFeatureNotSupportedException("MMDB driver does not support: " + feature);
    }

    // ---- 真实生效的最小实现 ----

    @Override
    public Statement createStatement() throws SQLException {
        checkClosed();
        return new MMDBStatement(this);
    }

    @Override
    public void close() throws SQLException {
        closed = true;
    }

    @Override
    public boolean isClosed() throws SQLException {
        return closed;
    }

    @Override
    public boolean isReadOnly() throws SQLException {
        return false;
    }

    @Override
    public int getTransactionIsolation() throws SQLException {
        return Connection.TRANSACTION_NONE;
    }

    @Override
    public int getHoldability() throws SQLException {
        return ResultSet.HOLD_CURSORS_OVER_COMMIT;
    }

    @Override
    public SQLWarning getWarnings() throws SQLException {
        return null;
    }

    @Override
    public void clearWarnings() throws SQLException {
        // 无警告队列，空实现
    }

    @Override
    public boolean isValid(int timeout) throws SQLException {
        return !closed;
    }

    @Override
    public Properties getClientInfo() throws SQLException {
        return new Properties();
    }

    @Override
    public String getClientInfo(String name) throws SQLException {
        return null;
    }

    // ---- 骨架桩：能力未接线，显式抛 SQLFeatureNotSupportedException ----

    @Override
    public PreparedStatement prepareStatement(String sql) throws SQLException {
        throw unsupported("prepareStatement");
    }

    @Override
    public CallableStatement prepareCall(String sql) throws SQLException {
        throw unsupported("prepareCall");
    }

    @Override
    public String nativeSQL(String sql) throws SQLException {
        checkClosed();
        return sql; // 方言透明，原样返回
    }

    @Override
    public void setAutoCommit(boolean autoCommit) throws SQLException {
        checkClosed(); // 单语句自动提交，仅接受默认状态
    }

    @Override
    public boolean getAutoCommit() throws SQLException {
        return true;
    }

    @Override
    public void commit() throws SQLException {
        checkClosed(); // auto-commit 模式下为空操作
    }

    @Override
    public void rollback() throws SQLException {
        checkClosed();
    }

    @Override
    public DatabaseMetaData getMetaData() throws SQLException {
        throw unsupported("getMetaData");
    }

    @Override
    public void setReadOnly(boolean readOnly) throws SQLException {
        checkClosed();
    }

    @Override
    public void setCatalog(String catalog) throws SQLException {
        checkClosed();
    }

    @Override
    public String getCatalog() throws SQLException {
        return null;
    }

    @Override
    public void setTransactionIsolation(int level) throws SQLException {
        throw unsupported("setTransactionIsolation");
    }

    @Override
    public Statement createStatement(int resultSetType, int resultSetConcurrency) throws SQLException {
        throw unsupported("createStatement(type, concurrency)");
    }

    @Override
    public PreparedStatement prepareStatement(String sql, int resultSetType, int resultSetConcurrency) throws SQLException {
        throw unsupported("prepareStatement(type, concurrency)");
    }

    @Override
    public CallableStatement prepareCall(String sql, int resultSetType, int resultSetConcurrency) throws SQLException {
        throw unsupported("prepareCall(type, concurrency)");
    }

    @Override
    public Map<String, Class<?>> getTypeMap() throws SQLException {
        throw unsupported("getTypeMap");
    }

    @Override
    public void setTypeMap(Map<String, Class<?>> map) throws SQLException {
        throw unsupported("setTypeMap");
    }

    @Override
    public void setHoldability(int holdability) throws SQLException {
        throw unsupported("setHoldability");
    }

    @Override
    public Savepoint setSavepoint() throws SQLException {
        throw unsupported("setSavepoint");
    }

    @Override
    public Savepoint setSavepoint(String name) throws SQLException {
        throw unsupported("setSavepoint(name)");
    }

    @Override
    public void rollback(Savepoint savepoint) throws SQLException {
        throw unsupported("rollback(savepoint)");
    }

    @Override
    public void releaseSavepoint(Savepoint savepoint) throws SQLException {
        throw unsupported("releaseSavepoint");
    }

    @Override
    public Statement createStatement(int resultSetType, int resultSetConcurrency, int resultSetHoldability) throws SQLException {
        throw unsupported("createStatement(type, concurrency, holdability)");
    }

    @Override
    public PreparedStatement prepareStatement(String sql, int resultSetType, int resultSetConcurrency, int resultSetHoldability) throws SQLException {
        throw unsupported("prepareStatement(type, concurrency, holdability)");
    }

    @Override
    public CallableStatement prepareCall(String sql, int resultSetType, int resultSetConcurrency, int resultSetHoldability) throws SQLException {
        throw unsupported("prepareCall(type, concurrency, holdability)");
    }

    @Override
    public PreparedStatement prepareStatement(String sql, int autoGeneratedKeys) throws SQLException {
        throw unsupported("prepareStatement(autoGeneratedKeys)");
    }

    @Override
    public PreparedStatement prepareStatement(String sql, int[] columnIndexes) throws SQLException {
        throw unsupported("prepareStatement(columnIndexes)");
    }

    @Override
    public PreparedStatement prepareStatement(String sql, String[] columnNames) throws SQLException {
        throw unsupported("prepareStatement(columnNames)");
    }

    @Override
    public Clob createClob() throws SQLException {
        throw unsupported("createClob");
    }

    @Override
    public Blob createBlob() throws SQLException {
        throw unsupported("createBlob");
    }

    @Override
    public NClob createNClob() throws SQLException {
        throw unsupported("createNClob");
    }

    @Override
    public SQLXML createSQLXML() throws SQLException {
        throw unsupported("createSQLXML");
    }

    @Override
    public void setClientInfo(String name, String value) {
        // 骨架：客户端信息静默忽略（接口 throws SQLClientInfoException，子集可不抛）
    }

    @Override
    public void setClientInfo(Properties properties) {
        // 骨架：客户端信息静默忽略
    }

    @Override
    public Array createArrayOf(String typeName, Object[] elements) throws SQLException {
        throw unsupported("createArrayOf");
    }

    @Override
    public Struct createStruct(String typeName, Object[] attributes) throws SQLException {
        throw unsupported("createStruct");
    }

    @Override
    public void setSchema(String schema) throws SQLException {
        checkClosed();
    }

    @Override
    public String getSchema() throws SQLException {
        return null;
    }

    @Override
    public void abort(Executor executor) throws SQLException {
        close();
    }

    @Override
    public void setNetworkTimeout(Executor executor, int milliseconds) throws SQLException {
        throw unsupported("setNetworkTimeout");
    }

    @Override
    public int getNetworkTimeout() throws SQLException {
        throw unsupported("getNetworkTimeout");
    }

    // ---- java.sql.Wrapper ----

    @Override
    public <T> T unwrap(Class<T> iface) throws SQLException {
        throw unsupported("unwrap");
    }

    @Override
    public boolean isWrapperFor(Class<?> iface) throws SQLException {
        return iface != null && iface.isInstance(this);
    }
}
