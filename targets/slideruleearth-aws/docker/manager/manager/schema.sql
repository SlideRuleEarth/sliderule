CREATE OR REPLACE TABLE requests (
    request_time TIMESTAMP,
    source_ip TEXT,
    aoi GEOMETRY,
    client TEXT,
    endpoint TEXT,
    duration REAL, /* seconds */
    status_code INTEGER,
    organization TEXT,
    version TEXT,
    message TEXT
);

CREATE OR REPLACE TABLE alarms (
    alarm_time TIMESTAMP,
    status_code INTEGER,
    organization TEXT,
    version TEXT,
    message TEXT
);
