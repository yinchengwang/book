-- sql_fundamentals — 数据库 Schema 定义
-- 演示 DDL（CREATE TABLE / INDEX）和 DML（INSERT）

CREATE TABLE IF NOT EXISTS departments (
    id   INTEGER PRIMARY KEY,
    name TEXT NOT NULL
);

CREATE TABLE IF NOT EXISTS employees (
    id         INTEGER PRIMARY KEY,
    name       TEXT NOT NULL,
    dept_id    INTEGER REFERENCES departments(id),
    salary     REAL NOT NULL,
    hire_date  TEXT NOT NULL
);

CREATE TABLE IF NOT EXISTS projects (
    id          INTEGER PRIMARY KEY,
    title       TEXT NOT NULL,
    emp_id      INTEGER REFERENCES employees(id),
    budget      REAL,
    deadline    TEXT
);

CREATE INDEX IF NOT EXISTS idx_emp_dept ON employees(dept_id);
CREATE INDEX IF NOT EXISTS idx_proj_emp ON projects(emp_id);
