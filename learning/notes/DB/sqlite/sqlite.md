# SQL语法

``` sql

# 格式化输出
sqlite>.header on
sqlite>.mode column
sqlite>.width 10, 20, 10

# 建表
sqlite> CREATE TABLE IF NOT EXISTS COMPANY(
   ID INT PRIMARY KEY     NOT NULL,
   NAME           TEXT    NOT NULL,
   AGE            INT     NOT NULL,
   ADDRESS        CHAR(50),
   SALARY         REAL
);

sqlite> CREATE TABLE IF NOT EXISTS DEPARTMENT(
   ID INT PRIMARY KEY      NOT NULL,
   DEPT           CHAR(50) NOT NULL,
   EMP_ID         INT      NOT NULL
);

# 查看当前有哪些表
sqlite> .tables
COMPANY     DEPARTMENT

sqlite> SELECT tbl_name FROM sqlite_master WHERE type = 'table';
tbl_name
--------
COMPANY

# 查看表模型
sqlite> .schema COMPANY
CREATE TABLE COMPANY(
   ID INT PRIMARY KEY     NOT NULL,
   NAME           TEXT    NOT NULL,
   AGE            INT     NOT NULL,
   ADDRESS        CHAR(50),
   SALARY         REAL
);

sqlite> .schema DEPARTMENT
CREATE TABLE DEPARTMENT(
   ID INT PRIMARY KEY      NOT NULL,
   DEPT           CHAR(50) NOT NULL,
   EMP_ID         INT      NOT NULL
);

# 删表
sqlite> DROP TABLE IF EXISTS DEPARTMENT;
sqlite> DROP TABLE IF EXISTS COMPANY;

# 插入
INSERT INTO COMPANY (ID,NAME,AGE,ADDRESS,SALARY)
VALUES (1, 'Paul', 32, 'California', 20000.00 );

INSERT INTO COMPANY (ID,NAME,AGE,ADDRESS,SALARY)
VALUES (2, 'Allen', 25, 'Texas', 15000.00 );

INSERT INTO COMPANY (ID,NAME,AGE,ADDRESS,SALARY)
VALUES (3, 'Teddy', 23, 'Norway', 20000.00 );

INSERT INTO COMPANY (ID,NAME,AGE,ADDRESS,SALARY)
VALUES (4, 'Mark', 25, 'Rich-Mond ', 65000.00 );

INSERT INTO COMPANY (ID,NAME,AGE,ADDRESS,SALARY)
VALUES (5, 'David', 27, 'Texas', 85000.00 );

INSERT INTO COMPANY (ID,NAME,AGE,ADDRESS,SALARY)
VALUES (6, 'Kim', 22, 'South-Hall', 45000.00 );

INSERT INTO COMPANY VALUES (7, 'James', 24, 'Houston', 10000.00);

# 查询
sqlite> select * from COMPANY;
ID  NAME   AGE  ADDRESS     SALARY
--  -----  ---  ----------  -------
1   Paul   32   California  20000.0
2   Allen  25   Texas       15000.0
3   Teddy  23   Norway      20000.0
4   Mark   25   Rich-Mond   65000.0
5   David  27   Texas       85000.0
6   Kim    22   South-Hall  45000.0
7   James  24   Houston     10000.0

# 使用一个表来填充另一个表
sqlite> CREATE TABLE IF NOT EXISTS COMPANY1(
   ID INT PRIMARY KEY     NOT NULL,
   NAME           TEXT    NOT NULL,
   AGE            INT     NOT NULL,
   ADDRESS        CHAR(50),
   SALARY         REAL
);

sqlite> INSERT INTO COMPANY1 SELECT ID, NAME, AGE, ADDRESS, SALARY FROM COMPANY where id < 5;

sqlite> select * from COMPANY1;
ID  NAME   AGE  ADDRESS     SALARY
--  -----  ---  ----------  -------
1   Paul   32   California  20000.0
2   Allen  25   Texas       15000.0
3   Teddy  23   Norway      20000.0
4   Mark   25   Rich-Mond   65000.0

# 算术运算符
sqlite> .mode line

sqlite> select 10 + 20;
10 + 20 = 30

sqlite> select 10 - 20;
10 - 20 = -10


sqlite> select 10 * 20;
10 * 20 = 200


sqlite> select 10 / 5;
10 / 5 = 2
sqlite> select 10 / 3;
10 / 3 = 3

sqlite> select 12 %  5;
12 %  5 = 2

# 比较运算符
sqlite> .mode column

sqlite> SELECT * FROM COMPANY WHERE SALARY > 50000;
ID  NAME   AGE  ADDRESS     SALARY
--  -----  ---  ----------  -------
4   Mark   25   Rich-Mond   65000.0
5   David  27   Texas       85000.0


sqlite> SELECT * FROM COMPANY WHERE SALARY = 20000;
ID  NAME   AGE  ADDRESS     SALARY
--  -----  ---  ----------  -------
1   Paul   32   California  20000.0
3   Teddy  23   Norway      20000.0


sqlite> SELECT * FROM COMPANY WHERE SALARY != 20000;
ID  NAME   AGE  ADDRESS     SALARY
--  -----  ---  ----------  -------
2   Allen  25   Texas       15000.0
4   Mark   25   Rich-Mond   65000.0
5   David  27   Texas       85000.0
6   Kim    22   South-Hall  45000.0
7   James  24   Houston     10000.0


sqlite> SELECT * FROM COMPANY WHERE SALARY <> 20000;
ID  NAME   AGE  ADDRESS     SALARY
--  -----  ---  ----------  -------
2   Allen  25   Texas       15000.0
4   Mark   25   Rich-Mond   65000.0
5   David  27   Texas       85000.0
6   Kim    22   South-Hall  45000.0
7   James  24   Houston     10000.0


sqlite> SELECT * FROM COMPANY WHERE SALARY >= 65000;
ID  NAME   AGE  ADDRESS     SALARY
--  -----  ---  ----------  -------
4   Mark   25   Rich-Mond   65000.0
5   David  27   Texas       85000.0


# 逻辑运算符
sqlite> SELECT * FROM COMPANY WHERE AGE >= 25 AND SALARY >= 65000;
ID          NAME        AGE         ADDRESS     SALARY
----------  ----------  ----------  ----------  ----------
4           Mark        25          Rich-Mond   65000.0
5           David       27          Texas       85000.0


sqlite> SELECT * FROM COMPANY WHERE AGE >= 25 OR SALARY >= 65000;
ID          NAME        AGE         ADDRESS     SALARY
----------  ----------  ----------  ----------  ----------
1           Paul        32          California  20000.0
2           Allen       25          Texas       15000.0
4           Mark        25          Rich-Mond   65000.0
5           David       27          Texas       85000.0


sqlite>  SELECT * FROM COMPANY WHERE AGE IS NOT NULL;
ID          NAME        AGE         ADDRESS     SALARY
----------  ----------  ----------  ----------  ----------
1           Paul        32          California  20000.0
2           Allen       25          Texas       15000.0
3           Teddy       23          Norway      20000.0
4           Mark        25          Rich-Mond   65000.0
5           David       27          Texas       85000.0
6           Kim         22          South-Hall  45000.0
7           James       24          Houston     10000.0


sqlite> SELECT * FROM COMPANY WHERE NAME LIKE 'Ki%';
ID          NAME        AGE         ADDRESS     SALARY
----------  ----------  ----------  ----------  ----------
6           Kim         22          South-Hall  45000.0


sqlite> SELECT * FROM COMPANY WHERE NAME GLOB 'Ki*';
ID          NAME        AGE         ADDRESS     SALARY
----------  ----------  ----------  ----------  ----------
6           Kim         22          South-Hall  45000.0


sqlite> SELECT * FROM COMPANY WHERE AGE IN ( 25, 27 );
ID          NAME        AGE         ADDRESS     SALARY
----------  ----------  ----------  ----------  ----------
2           Allen       25          Texas       15000.0
4           Mark        25          Rich-Mond   65000.0
5           David       27          Texas       85000.0


sqlite> SELECT * FROM COMPANY WHERE AGE NOT IN ( 25, 27 );
ID          NAME        AGE         ADDRESS     SALARY
----------  ----------  ----------  ----------  ----------
1           Paul        32          California  20000.0
3           Teddy       23          Norway      20000.0
6           Kim         22          South-Hall  45000.0
7           James       24          Houston     10000.0


sqlite> SELECT * FROM COMPANY WHERE AGE BETWEEN 25 AND 27;
ID          NAME        AGE         ADDRESS     SALARY
----------  ----------  ----------  ----------  ----------
2           Allen       25          Texas       15000.0
4           Mark        25          Rich-Mond   65000.0
5           David       27          Texas       85000.0


sqlite> SELECT AGE FROM COMPANY WHERE EXISTS (SELECT AGE FROM COMPANY WHERE SALARY > 65000);
AGE
----------
32
25
23
25
27
22
24


sqlite> SELECT * FROM COMPANY WHERE AGE > (SELECT AGE FROM COMPANY WHERE SALARY > 65000);
ID          NAME        AGE         ADDRESS     SALARY
----------  ----------  ----------  ----------  ----------
1           Paul        32          California  20000.0


# 位运算符
sqlite> .mode line

sqlite> select 60 | 13;
# 0011 1100     60
# 0000 1101     13
# 0011 1101     61
60 | 13 = 61

sqlite> select 60 & 13;
# 0011 1100     60
# 0000 1101     13
# 0000 1100     12
60 & 13 = 12

sqlite>  select  (~60);
# ~x = -x - 1
(~60) = -61

sqlite>  select  (60 << 2);
(60 << 2) = 240

sqlite>  select  (60 >> 2);
(60 >> 2) = 15

```