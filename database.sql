
IF DB_ID(N'HoldingAnalytics') IS NULL
    CREATE DATABASE HoldingAnalytics;
GO
USE HoldingAnalytics;
GO

IF OBJECT_ID(N'dbo.IndicatorValues', N'U') IS NOT NULL DROP TABLE dbo.IndicatorValues;
IF OBJECT_ID(N'dbo.ExchangeRates',   N'U') IS NOT NULL DROP TABLE dbo.ExchangeRates;
IF OBJECT_ID(N'dbo.Indicators',      N'U') IS NOT NULL DROP TABLE dbo.Indicators;
IF OBJECT_ID(N'dbo.Periods',         N'U') IS NOT NULL DROP TABLE dbo.Periods;
IF OBJECT_ID(N'dbo.Enterprises',     N'U') IS NOT NULL DROP TABLE dbo.Enterprises;
IF OBJECT_ID(N'dbo.Users',           N'U') IS NOT NULL DROP TABLE dbo.Users;
IF OBJECT_ID(N'dbo.Currencies',      N'U') IS NOT NULL DROP TABLE dbo.Currencies;
GO

CREATE TABLE Currencies (
    id    INT IDENTITY(1,1) PRIMARY KEY,
    code  CHAR(3)       NOT NULL UNIQUE,
    name  NVARCHAR(100) NOT NULL
);

CREATE TABLE Enterprises (
    id             INT IDENTITY(1,1) PRIMARY KEY,
    name           NVARCHAR(200) NOT NULL UNIQUE,
    requisites     NVARCHAR(300) NOT NULL,
    phone          NVARCHAR(30)  NOT NULL,
    contact_person NVARCHAR(150) NOT NULL
);

CREATE TABLE Indicators (
    id          INT IDENTITY(1,1) PRIMARY KEY,
    name        NVARCHAR(200) NOT NULL UNIQUE,
    weight      FLOAT         NOT NULL CHECK (weight > 0),
    unit        NVARCHAR(50)  NOT NULL,
    currency_id INT NULL REFERENCES Currencies(id)      -- NULL: безразмерный показатель (%, коэффициент)
);

CREATE TABLE Periods (
    id         INT IDENTITY(1,1) PRIMARY KEY,
    name       NVARCHAR(100) NOT NULL UNIQUE,
    start_date DATE NOT NULL,
    end_date   DATE NOT NULL,
    CHECK (start_date <= end_date)
);

CREATE TABLE ExchangeRates (
    id               INT IDENTITY(1,1) PRIMARY KEY,
    rate_date        DATE  NOT NULL,
    from_currency_id INT   NOT NULL REFERENCES Currencies(id),
    to_currency_id   INT   NOT NULL REFERENCES Currencies(id),
    rate             FLOAT NOT NULL CHECK (rate > 0),
    CHECK (from_currency_id <> to_currency_id),
    UNIQUE (rate_date, from_currency_id, to_currency_id)
);

CREATE TABLE IndicatorValues (
    id            INT IDENTITY(1,1) PRIMARY KEY,
    enterprise_id INT   NOT NULL REFERENCES Enterprises(id),
    indicator_id  INT   NOT NULL REFERENCES Indicators(id),
    period_id     INT   NOT NULL REFERENCES Periods(id),
    value         FLOAT NOT NULL,
    UNIQUE (enterprise_id, indicator_id, period_id)
);

CREATE TABLE Users (
    id            INT IDENTITY(1,1) PRIMARY KEY,
    login         NVARCHAR(50)  NOT NULL UNIQUE,
    fio           NVARCHAR(150) NOT NULL,
    password_hash CHAR(64)      NOT NULL,                -- SHA-256, HEX в верхнем регистре
    role          VARCHAR(20)   NOT NULL CHECK (role IN ('admin', 'analyst'))
);
GO

/* ---------------- Тестовые данные ---------------- */
INSERT INTO Currencies(code, name) VALUES
 ('RUB', N'Российский рубль'), ('USD', N'Доллар США'), ('EUR', N'Евро');

INSERT INTO Enterprises(name, requisites, phone, contact_person) VALUES
 (N'ООО «Альфа-Металл»', N'ИНН 2460000001, КПП 246001001', N'+7 (391) 200-10-01', N'Иванов Иван Иванович'),
 (N'АО «Бета-Энерго»',   N'ИНН 2460000002, КПП 246001002', N'+7 (391) 200-10-02', N'Петрова Анна Сергеевна'),
 (N'ПАО «Гамма-Транс»',  N'ИНН 2460000003, КПП 246001003', N'+7 (391) 200-10-03', N'Сидоров Олег Петрович');

INSERT INTO Indicators(name, weight, unit, currency_id) VALUES
 (N'Выручка',          5, N'млн', (SELECT id FROM Currencies WHERE code='RUB')),
 (N'Чистая прибыль',   4, N'млн', (SELECT id FROM Currencies WHERE code='RUB')),
 (N'EBITDA',           3, N'млн', (SELECT id FROM Currencies WHERE code='USD')),
 (N'Капитализация',    2, N'млн', (SELECT id FROM Currencies WHERE code='EUR')),
 (N'Рентабельность',   3, N'%',   NULL);

INSERT INTO Periods(name, start_date, end_date) VALUES
 (N'2025 Q1', '2025-01-01', '2025-03-31'),
 (N'2025 Q2', '2025-04-01', '2025-06-30'),
 (N'2025 Q3', '2025-07-01', '2025-09-30'),
 (N'2025 Q4', '2025-10-01', '2025-12-31');

DECLARE @rub INT = (SELECT id FROM Currencies WHERE code='RUB');
DECLARE @usd INT = (SELECT id FROM Currencies WHERE code='USD');
DECLARE @eur INT = (SELECT id FROM Currencies WHERE code='EUR');

INSERT INTO ExchangeRates(rate_date, from_currency_id, to_currency_id, rate)
SELECT end_date, @usd, @rub, 88 + id * 1.5  FROM Periods
UNION ALL
SELECT end_date, @eur, @rub, 96 + id * 1.7  FROM Periods
UNION ALL
SELECT end_date, @eur, @usd, 1.08 + id * 0.01 FROM Periods;

INSERT INTO IndicatorValues(enterprise_id, indicator_id, period_id, value)
SELECT e.id, i.id, p.id,
       CASE WHEN i.unit = N'%'
            THEN ROUND(8 + e.id * 2.5 + p.id * 0.7 + (i.id % 3), 2)
            ELSE ROUND(i.weight * 1000 * (1 + e.id * 0.25) * (1 + p.id * 0.06)
                       + ((e.id * 7 + i.id * 3 + p.id * 5) % 11) * 10, 2)
       END
FROM Enterprises e CROSS JOIN Indicators i CROSS JOIN Periods p;

INSERT INTO Users(login, fio, password_hash, role) VALUES
 (N'admin',   N'Администратор системы', CONVERT(CHAR(64), HASHBYTES('SHA2_256', 'admin'),   2), 'admin'),
 (N'analyst', N'Петров Пётр Петрович',  CONVERT(CHAR(64), HASHBYTES('SHA2_256', 'analyst'), 2), 'analyst');
GO

SELECT 'OK' AS result,
       (SELECT COUNT(*) FROM IndicatorValues) AS values_count,
       (SELECT COUNT(*) FROM ExchangeRates)   AS rates_count;
GO
