CREATE OR REPLACE TABLE telemetry (
    record_time TIMESTAMP,
    source_ip_hash TEXT,
    source_ip_location TEXT,
    aoi GEOMETRY,
    client TEXT,
    endpoint TEXT,
    duration REAL, /* seconds */
    status_code INTEGER,
    account TEXT,
    version TEXT,
    message TEXT
);

CREATE OR REPLACE TABLE alerts (
    record_time TIMESTAMP,
    status_code INTEGER,
    account TEXT,
    version TEXT,
    message TEXT
);
