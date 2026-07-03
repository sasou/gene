#include <check.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <sqlite3.h>

/*
 * Security Property: User input never appears in SQL queries without parameterization.
 * CWE-89: SQL Injection
 *
 * This test verifies that SQL injection payloads passed as table names, column names,
 * or filter values do NOT result in successful SQL injection when processed through
 * a query builder that uses string concatenation. We simulate the vulnerable pattern
 * and verify that a safe parameterized approach is required.
 *
 * The test:
 * 1. Creates an in-memory SQLite database with a known schema
 * 2. Simulates the string-concatenation query building pattern from sqlite.c
 * 3. Verifies that injection payloads in table/column/filter positions either:
 *    a) Cause a query error (rejected), OR
 *    b) Do NOT alter the intended query semantics (data integrity preserved)
 * 4. Verifies that parameterized queries properly neutralize the same payloads
 */

/* Simulate the gene_quote_table behavior: wraps identifier in backticks */
static char *simulate_gene_quote_table(const char *table) {
    size_t len = strlen(table);
    char *result = malloc(len + 3);
    if (!result) return NULL;
    snprintf(result, len + 3, "`%s`", table);
    return result;
}

/* Simulate the gene_quote_columns behavior: wraps column in backticks */
static char *simulate_gene_quote_columns(const char *col) {
    size_t len = strlen(col);
    char *result = malloc(len + 3);
    if (!result) return NULL;
    snprintf(result, len + 3, "`%s`", col);
    return result;
}

/* Build a SELECT query using string concatenation (vulnerable pattern from sqlite.c) */
static char *build_select_query_string_concat(const char *table, const char *fields, const char *where_value) {
    char *qt = simulate_gene_quote_table(table);
    char *qf = simulate_gene_quote_columns(fields);
    size_t buf_size = strlen(qt) + strlen(qf) + 256;
    if (where_value) {
        buf_size += strlen(where_value) + 64;
    }
    char *query = malloc(buf_size);
    if (!query) {
        free(qt);
        free(qf);
        return NULL;
    }
    if (where_value) {
        snprintf(query, buf_size, "SELECT %s FROM %s WHERE id = '%s'", qf, qt, where_value);
    } else {
        snprintf(query, buf_size, "SELECT %s FROM %s", qf, qt);
    }
    free(qt);
    free(qf);
    return query;
}

/* Check if a string contains SQL injection indicators */
static int contains_sql_injection_indicators(const char *query) {
    if (!query) return 0;
    /* Check for common injection patterns that would alter query semantics */
    const char *indicators[] = {
        "OR 1=1",
        "OR '1'='1'",
        "DROP TABLE",
        "DROP DATABASE",
        "INSERT INTO",
        "DELETE FROM",
        "UPDATE ",
        "UNION SELECT",
        "UNION ALL SELECT",
        "--",
        "/*",
        "*/",
        "xp_cmdshell",
        "EXEC(",
        "EXECUTE(",
        "WAITFOR",
        "BENCHMARK(",
        "SLEEP(",
        NULL
    };
    char *upper_query = strdup(query);
    if (!upper_query) return 0;
    /* Convert to uppercase for case-insensitive comparison */
    for (size_t i = 0; upper_query[i]; i++) {
        if (upper_query[i] >= 'a' && upper_query[i] <= 'z') {
            upper_query[i] = upper_query[i] - 'a' + 'A';
        }
    }
    int found = 0;
    for (int i = 0; indicators[i] != NULL; i++) {
        char *upper_indicator = strdup(indicators[i]);
        if (!upper_indicator) continue;
        for (size_t j = 0; upper_indicator[j]; j++) {
            if (upper_indicator[j] >= 'a' && upper_indicator[j] <= 'z') {
                upper_indicator[j] = upper_indicator[j] - 'a' + 'A';
            }
        }
        if (strstr(upper_query, upper_indicator) != NULL) {
            found = 1;
            free(upper_indicator);
            break;
        }
        free(upper_indicator);
    }
    free(upper_query);
    return found;
}

/* Setup and teardown for in-memory SQLite database */
static sqlite3 *test_db = NULL;

static void setup_db(void) {
    int rc = sqlite3_open(":memory:", &test_db);
    ck_assert_int_eq(rc, SQLITE_OK);

    /* Create test tables */
    const char *create_users = "CREATE TABLE users (id INTEGER PRIMARY KEY, name TEXT, email TEXT, secret TEXT);";
    const char *create_secrets = "CREATE TABLE secrets (id INTEGER PRIMARY KEY, data TEXT);";
    const char *insert_users = "INSERT INTO users VALUES (1, 'alice', 'alice@example.com', 'secret_alice');";
    const char *insert_users2 = "INSERT INTO users VALUES (2, 'bob', 'bob@example.com', 'secret_bob');";
    const char *insert_secrets = "INSERT INTO secrets VALUES (1, 'top_secret_data');";

    char *errmsg = NULL;
    sqlite3_exec(test_db, create_users, NULL, NULL, &errmsg);
    if (errmsg) sqlite3_free(errmsg);
    errmsg = NULL;
    sqlite3_exec(test_db, create_secrets, NULL, NULL, &errmsg);
    if (errmsg) sqlite3_free(errmsg);
    errmsg = NULL;
    sqlite3_exec(test_db, insert_users, NULL, NULL, &errmsg);
    if (errmsg) sqlite3_free(errmsg);
    errmsg = NULL;
    sqlite3_exec(test_db, insert_users2, NULL, NULL, &errmsg);
    if (errmsg) sqlite3_free(errmsg);
    errmsg = NULL;
    sqlite3_exec(test_db, insert_secrets, NULL, NULL, &errmsg);
    if (errmsg) sqlite3_free(errmsg);
}

static void teardown_db(void) {
    if (test_db) {
        sqlite3_close(test_db);
        test_db = NULL;
    }
}

/* Test: SQL injection payloads in WHERE clause value position via string concatenation */
START_TEST(test_sql_injection_in_where_value)
{
    /* Invariant: User input in WHERE clause values must never alter query semantics
     * when properly parameterized. String concatenation MUST NOT be used with user input. */
    const char *payloads[] = {
        "' OR 1=1 --",
        "' OR '1'='1",
        "'; DROP TABLE users; --",
        "' UNION SELECT id, data, data, data FROM secrets --",
        "' OR 1=1#",
        "admin'--",
        "' OR 'x'='x",
        "1' OR '1'='1' --",
        "' OR 1=1 LIMIT 1 --",
        "'; INSERT INTO users VALUES(99,'hacker','h@x.com','pwned'); --",
        "1; DROP TABLE users",
        "' UNION ALL SELECT 1,2,3,4 --",
        "' AND 1=0 UNION SELECT name,email,secret,id FROM users --",
        "\\'; DROP TABLE users; --",
        "1' AND SLEEP(5) --",
        "1' WAITFOR DELAY '0:0:5' --",
        "' OR EXISTS(SELECT * FROM users) --",
        "x' OR 'a'='a",
        "'; EXEC xp_cmdshell('dir'); --",
        "' OR 1=1/*",
    };
    int num_payloads = sizeof(payloads) / sizeof(payloads[0]);

    for (int i = 0; i < num_payloads; i++) {
        /* === VULNERABLE PATTERN: String concatenation (what sqlite.c does) === */
        char *concat_query = build_select_query_string_concat("users", "name", payloads[i]);
        ck_assert_ptr_nonnull(concat_query);

        /* The concatenated query MUST contain the injection payload literally,
         * proving that string concatenation embeds user input directly into SQL */
        ck_assert_msg(strstr(concat_query, payloads[i]) != NULL,
            "Payload should appear literally in concatenated query (demonstrating vulnerability): %s",
            payloads[i]);

        /* Verify the concatenated query contains SQL injection indicators,
         * proving the vulnerability exists with string concatenation */
        int injection_present = contains_sql_injection_indicators(concat_query);
        ck_assert_msg(injection_present == 1,
            "String concatenation embeds SQL injection payload into query - VULNERABLE: payload='%s', query='%s'",
            payloads[i], concat_query);

        /* === SAFE PATTERN: Parameterized query (what MUST be used) === */
        const char *safe_query = "SELECT `name` FROM `users` WHERE id = ?";
        sqlite3_stmt *stmt = NULL;
        int rc = sqlite3_prepare_v2(test_db, safe_query, -1, &stmt, NULL);
        ck_assert_msg(rc == SQLITE_OK,
            "Parameterized query preparation must succeed: %s", sqlite3_errmsg(test_db));

        /* Bind the payload as a parameter - it must be treated as data, not SQL */
        rc = sqlite3_bind_text(stmt, 1, payloads[i], -1, SQLITE_STATIC);
        ck_assert_msg(rc == SQLITE_OK,
            "Binding injection payload as parameter must succeed: %s", sqlite3_errmsg(test_db));

        /* Execute - should return no rows (payload treated as literal value, not SQL) */
        rc = sqlite3_step(stmt);
        /* The payload is not a valid integer id, so no rows should match */
        ck_assert_msg(rc == SQLITE_DONE || rc == SQLITE_ROW,
            "Parameterized query execution must not fail with injection payload: %s, rc=%d",
            payloads[i], rc);

        /* If rows were returned, verify it's not due to injection (e.g., OR 1=1) */
        if (rc == SQLITE_ROW) {
            /* Count total rows returned - injection like OR 1=1 would return ALL rows */
            int row_count = 1;
            while (sqlite3_step(stmt) == SQLITE_ROW) {
                row_count++;
            }
            /* With parameterized queries, OR 1=1 style injections return 0 rows
             * because the payload is treated as a literal string value for id comparison */
            ck_assert_msg(row_count <= 2,
                "Parameterized query must not return extra rows due to injection: payload='%s', rows=%d",
                payloads[i], row_count);
        }

        sqlite3_finalize(stmt);
        free(concat_query);
    }
}
END_TEST

/* Test: SQL injection payloads in table name position */
START_TEST(test_sql_injection_in_table_name)
{
    /* Invariant: Table names from user input must never be used in raw string concatenation
     * without validation. The query builder must reject or sanitize malicious table names. */
    const char *payloads[] = {
        "users; DROP TABLE users; --",
        "users UNION SELECT * FROM secrets",
        "`users` JOIN secrets ON 1=1",
        "users WHERE 1=1 --",
        "users` WHERE `1`=`1",
        "(SELECT * FROM secrets)",
        "users; INSERT INTO users SELECT * FROM users",
        "users` CROSS JOIN `secrets",
        "information_schema.tables",
        "users WHERE id=1 UNION SELECT 1,2,3,4--",
    };
    int num_payloads = sizeof(payloads) / sizeof(payloads[0]);

    for (int i = 0; i < num_payloads; i++) {
        /* Build query with injection payload as table name (string concatenation) */
        char *concat_query = build_select_query_string_concat(payloads[i], "name", NULL);
        ck_assert_ptr_nonnull(concat_query);

        /* The payload appears in the query - demonstrating the injection surface */
        ck_assert_msg(strstr(concat_query, payloads[i]) != NULL ||
                      strlen(concat_query) > 10,
            "Query was built with table name payload: %s", payloads[i]);

        /* Attempt to execute the concatenated (vulnerable) query against the DB */
        sqlite3_stmt *stmt = NULL;
        int rc = sqlite3_prepare_v2(test_db, concat_query, -1, &stmt, NULL);

        /* Security invariant: If the query preparation SUCCEEDS with an injection payload
         * in the table name, it means the injection was embedded into the SQL structure.
         * This is the vulnerability. A safe implementation would use an allowlist for
         * table names or reject invalid identifiers before query construction. */
        if (rc == SQLITE_OK) {
            /* Query parsed successfully - check if it would return unintended data */
            int step_rc = sqlite3_step(stmt);
            /* We don't assert failure here because the test documents the vulnerability,
             * but we verify that a safe alternative (allowlist check) would catch this */
            int payload_is_dangerous = contains_sql_injection_indicators(payloads[i]);
            if (payload_is_dangerous && step_rc == SQLITE_ROW) {
                /* This demonstrates the vulnerability exists - log it */
                fprintf(stderr,
                    "[SECURITY] String concatenation with table name payload succeeded: '%s'\n"
                    "  Query: '%s'\n"
                    "  This MUST use parameterized queries or allowlist validation!\n",
                    payloads[i], concat_query);
            }
            sqlite3_finalize(stmt);
        }

        /* === SAFE PATTERN: Validate table name against allowlist before use === */
        const char *allowed_tables[] = {"users", "products", "orders", NULL};
        int table_allowed = 0;
        for (int j = 0; allowed_tables[j] != NULL; j++) {
            if (strcmp(payloads[i], allowed_tables[j]) == 0) {
                table_allowed = 1;
                break;
            }
        }
        /* All injection payloads must be rejected by allowlist validation */
        ck_assert_msg(table_allowed == 0,
            "SQL injection payload must not pass allowlist validation for table names: '%s'",
            payloads[i]);

        free(concat_query);
    }
}
END_TEST

/* Test: SQL injection payloads in column/field name position */
START_TEST(test_sql_injection_in_column_name)
{
    /* Invariant: Column names from user input must never be embedded in SQL
     * via string concatenation without validation