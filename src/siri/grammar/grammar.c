/*
 * This grammar is generated using the Grammar.export_c() method and
 * should be used with the cleri module.
 *
 * Source class: SiriGrammar
 * Created at: 2016-10-25 17:23:57
 */

#include <siri/grammar/grammar.h>
#include <stdio.h>

#define CLERI_CASE_SENSITIVE 0
#define CLERI_CASE_INSENSITIVE 1

#define CLERI_FIRST_MATCH 0
#define CLERI_MOST_GREEDY 1

cleri_grammar_t * compile_grammar(void)
{
    cleri_object_t * r_float = cleri_regex(CLERI_GID_R_FLOAT, "^[-+]?[0-9]*\\.?[0-9]+");
    cleri_object_t * r_integer = cleri_regex(CLERI_GID_R_INTEGER, "^[-+]?[0-9]+");
    cleri_object_t * r_uinteger = cleri_regex(CLERI_GID_R_UINTEGER, "^[0-9]+");
    cleri_object_t * r_time_str = cleri_regex(CLERI_GID_R_TIME_STR, "^[0-9]+[smhdw]");
    cleri_object_t * r_singleq_str = cleri_regex(CLERI_GID_R_SINGLEQ_STR, "^(?:\'(?:[^\']*)\')+");
    cleri_object_t * r_doubleq_str = cleri_regex(CLERI_GID_R_DOUBLEQ_STR, "^(?:\"(?:[^\"]*)\")+");
    cleri_object_t * r_grave_str = cleri_regex(CLERI_GID_R_GRAVE_STR, "^(?:`(?:[^`]*)`)+");
    cleri_object_t * r_uuid_str = cleri_regex(CLERI_GID_R_UUID_STR, "^[0-9a-f]{8}\\-[0-9a-f]{4}\\-[0-9a-f]{4}\\-[0-9a-f]{4}\\-[0-9a-f]{12}");
    cleri_object_t * r_regex = cleri_regex(CLERI_GID_R_REGEX, "^(/[^/\\\\]*(?:\\\\.[^/\\\\]*)*/i?)");
    cleri_object_t * r_comment = cleri_regex(CLERI_GID_R_COMMENT, "^#.*");
    cleri_object_t * k_access = cleri_keyword(CLERI_GID_K_ACCESS, "access", CLERI_CASE_INSENSITIVE);
    cleri_object_t * k_active_handles = cleri_keyword(CLERI_GID_K_ACTIVE_HANDLES, "active_handles", CLERI_CASE_INSENSITIVE);
    cleri_object_t * k_address = cleri_keyword(CLERI_GID_K_ADDRESS, "address", CLERI_CASE_INSENSITIVE);
    cleri_object_t * k_after = cleri_keyword(CLERI_GID_K_AFTER, "after", CLERI_CASE_INSENSITIVE);
    cleri_object_t * k_alter = cleri_keyword(CLERI_GID_K_ALTER, "alter", CLERI_CASE_INSENSITIVE);
    cleri_object_t * k_and = cleri_keyword(CLERI_GID_K_AND, "and", CLERI_CASE_INSENSITIVE);
    cleri_object_t * k_as = cleri_keyword(CLERI_GID_K_AS, "as", CLERI_CASE_INSENSITIVE);
    cleri_object_t * k_backup_mode = cleri_keyword(CLERI_GID_K_BACKUP_MODE, "backup_mode", CLERI_CASE_INSENSITIVE);
    cleri_object_t * k_before = cleri_keyword(CLERI_GID_K_BEFORE, "before", CLERI_CASE_INSENSITIVE);
    cleri_object_t * k_buffer_size = cleri_keyword(CLERI_GID_K_BUFFER_SIZE, "buffer_size", CLERI_CASE_INSENSITIVE);
    cleri_object_t * k_buffer_path = cleri_keyword(CLERI_GID_K_BUFFER_PATH, "buffer_path", CLERI_CASE_INSENSITIVE);
    cleri_object_t * k_between = cleri_keyword(CLERI_GID_K_BETWEEN, "between", CLERI_CASE_INSENSITIVE);
    cleri_object_t * k_count = cleri_keyword(CLERI_GID_K_COUNT, "count", CLERI_CASE_INSENSITIVE);
    cleri_object_t * k_create = cleri_keyword(CLERI_GID_K_CREATE, "create", CLERI_CASE_INSENSITIVE);
    cleri_object_t * k_critical = cleri_keyword(CLERI_GID_K_CRITICAL, "critical", CLERI_CASE_INSENSITIVE);
    cleri_object_t * k_database = cleri_keyword(CLERI_GID_K_DATABASE, "database", CLERI_CASE_INSENSITIVE);
    cleri_object_t * k_dbname = cleri_keyword(CLERI_GID_K_DBNAME, "dbname", CLERI_CASE_INSENSITIVE);
    cleri_object_t * k_dbpath = cleri_keyword(CLERI_GID_K_DBPATH, "dbpath", CLERI_CASE_INSENSITIVE);
    cleri_object_t * k_debug = cleri_keyword(CLERI_GID_K_DEBUG, "debug", CLERI_CASE_INSENSITIVE);
    cleri_object_t * k_derivative = cleri_keyword(CLERI_GID_K_DERIVATIVE, "derivative", CLERI_CASE_INSENSITIVE);
    cleri_object_t * k_difference = cleri_keyword(CLERI_GID_K_DIFFERENCE, "difference", CLERI_CASE_INSENSITIVE);
    cleri_object_t * k_drop = cleri_keyword(CLERI_GID_K_DROP, "drop", CLERI_CASE_INSENSITIVE);
    cleri_object_t * k_drop_threshold = cleri_keyword(CLERI_GID_K_DROP_THRESHOLD, "drop_threshold", CLERI_CASE_INSENSITIVE);
    cleri_object_t * k_duration_log = cleri_keyword(CLERI_GID_K_DURATION_LOG, "duration_log", CLERI_CASE_INSENSITIVE);
    cleri_object_t * k_duration_num = cleri_keyword(CLERI_GID_K_DURATION_NUM, "duration_num", CLERI_CASE_INSENSITIVE);
    cleri_object_t * k_end = cleri_keyword(CLERI_GID_K_END, "end", CLERI_CASE_INSENSITIVE);
    cleri_object_t * k_error = cleri_keyword(CLERI_GID_K_ERROR, "error", CLERI_CASE_INSENSITIVE);
    cleri_object_t * k_expression = cleri_keyword(CLERI_GID_K_EXPRESSION, "expression", CLERI_CASE_INSENSITIVE);
    cleri_object_t * k_false = cleri_keyword(CLERI_GID_K_FALSE, "false", CLERI_CASE_INSENSITIVE);
    cleri_object_t * k_filter = cleri_keyword(CLERI_GID_K_FILTER, "filter", CLERI_CASE_INSENSITIVE);
    cleri_object_t * k_float = cleri_keyword(CLERI_GID_K_FLOAT, "float", CLERI_CASE_INSENSITIVE);
    cleri_object_t * k_for = cleri_keyword(CLERI_GID_K_FOR, "for", CLERI_CASE_INSENSITIVE);
    cleri_object_t * k_from = cleri_keyword(CLERI_GID_K_FROM, "from", CLERI_CASE_INSENSITIVE);
    cleri_object_t * k_full = cleri_keyword(CLERI_GID_K_FULL, "full", CLERI_CASE_INSENSITIVE);
    cleri_object_t * k_grant = cleri_keyword(CLERI_GID_K_GRANT, "grant", CLERI_CASE_INSENSITIVE);
    cleri_object_t * k_group = cleri_keyword(CLERI_GID_K_GROUP, "group", CLERI_CASE_INSENSITIVE);
    cleri_object_t * k_groups = cleri_keyword(CLERI_GID_K_GROUPS, "groups", CLERI_CASE_INSENSITIVE);
    cleri_object_t * k_help = cleri_choice(
        CLERI_GID_K_HELP,
        CLERI_MOST_GREEDY,
        2,
        cleri_keyword(CLERI_NONE, "help", CLERI_CASE_INSENSITIVE),
        cleri_token(CLERI_NONE, "?")
    );
    cleri_object_t * k_info = cleri_keyword(CLERI_GID_K_INFO, "info", CLERI_CASE_INSENSITIVE);
    cleri_object_t * k_ignore_threshold = cleri_keyword(CLERI_GID_K_IGNORE_THRESHOLD, "ignore_threshold", CLERI_CASE_INSENSITIVE);
    cleri_object_t * k_insert = cleri_keyword(CLERI_GID_K_INSERT, "insert", CLERI_CASE_INSENSITIVE);
    cleri_object_t * k_integer = cleri_keyword(CLERI_GID_K_INTEGER, "integer", CLERI_CASE_INSENSITIVE);
    cleri_object_t * k_intersection = cleri_choice(
        CLERI_GID_K_INTERSECTION,
        CLERI_FIRST_MATCH,
        2,
        cleri_token(CLERI_NONE, "&"),
        cleri_keyword(CLERI_NONE, "intersection", CLERI_CASE_INSENSITIVE)
    );
    cleri_object_t * k_ip_support = cleri_keyword(CLERI_GID_K_IP_SUPPORT, "ip_support", CLERI_CASE_INSENSITIVE);
    cleri_object_t * k_length = cleri_keyword(CLERI_GID_K_LENGTH, "length", CLERI_CASE_INSENSITIVE);
    cleri_object_t * k_libuv = cleri_keyword(CLERI_GID_K_LIBUV, "libuv", CLERI_CASE_INSENSITIVE);
    cleri_object_t * k_limit = cleri_keyword(CLERI_GID_K_LIMIT, "limit", CLERI_CASE_INSENSITIVE);
    cleri_object_t * k_list = cleri_keyword(CLERI_GID_K_LIST, "list", CLERI_CASE_INSENSITIVE);
    cleri_object_t * k_log = cleri_keyword(CLERI_GID_K_LOG, "log", CLERI_CASE_INSENSITIVE);
    cleri_object_t * k_log_level = cleri_keyword(CLERI_GID_K_LOG_LEVEL, "log_level", CLERI_CASE_INSENSITIVE);
    cleri_object_t * k_max = cleri_keyword(CLERI_GID_K_MAX, "max", CLERI_CASE_INSENSITIVE);
    cleri_object_t * k_max_open_files = cleri_keyword(CLERI_GID_K_MAX_OPEN_FILES, "max_open_files", CLERI_CASE_INSENSITIVE);
    cleri_object_t * k_mean = cleri_keyword(CLERI_GID_K_MEAN, "mean", CLERI_CASE_INSENSITIVE);
    cleri_object_t * k_median = cleri_keyword(CLERI_GID_K_MEDIAN, "median", CLERI_CASE_INSENSITIVE);
    cleri_object_t * k_median_low = cleri_keyword(CLERI_GID_K_MEDIAN_LOW, "median_low", CLERI_CASE_INSENSITIVE);
    cleri_object_t * k_median_high = cleri_keyword(CLERI_GID_K_MEDIAN_HIGH, "median_high", CLERI_CASE_INSENSITIVE);
    cleri_object_t * k_mem_usage = cleri_keyword(CLERI_GID_K_MEM_USAGE, "mem_usage", CLERI_CASE_INSENSITIVE);
    cleri_object_t * k_merge = cleri_keyword(CLERI_GID_K_MERGE, "merge", CLERI_CASE_INSENSITIVE);
    cleri_object_t * k_min = cleri_keyword(CLERI_GID_K_MIN, "min", CLERI_CASE_INSENSITIVE);
    cleri_object_t * k_modify = cleri_keyword(CLERI_GID_K_MODIFY, "modify", CLERI_CASE_INSENSITIVE);
    cleri_object_t * k_name = cleri_keyword(CLERI_GID_K_NAME, "name", CLERI_CASE_INSENSITIVE);
    cleri_object_t * k_now = cleri_keyword(CLERI_GID_K_NOW, "now", CLERI_CASE_INSENSITIVE);
    cleri_object_t * k_number = cleri_keyword(CLERI_GID_K_NUMBER, "number", CLERI_CASE_INSENSITIVE);
    cleri_object_t * k_online = cleri_keyword(CLERI_GID_K_ONLINE, "online", CLERI_CASE_INSENSITIVE);
    cleri_object_t * k_open_files = cleri_keyword(CLERI_GID_K_OPEN_FILES, "open_files", CLERI_CASE_INSENSITIVE);
    cleri_object_t * k_or = cleri_keyword(CLERI_GID_K_OR, "or", CLERI_CASE_INSENSITIVE);
    cleri_object_t * k_password = cleri_keyword(CLERI_GID_K_PASSWORD, "password", CLERI_CASE_INSENSITIVE);
    cleri_object_t * k_points = cleri_keyword(CLERI_GID_K_POINTS, "points", CLERI_CASE_INSENSITIVE);
    cleri_object_t * k_pool = cleri_keyword(CLERI_GID_K_POOL, "pool", CLERI_CASE_INSENSITIVE);
    cleri_object_t * k_pools = cleri_keyword(CLERI_GID_K_POOLS, "pools", CLERI_CASE_INSENSITIVE);
    cleri_object_t * k_port = cleri_keyword(CLERI_GID_K_PORT, "port", CLERI_CASE_INSENSITIVE);
    cleri_object_t * k_prefix = cleri_keyword(CLERI_GID_K_PREFIX, "prefix", CLERI_CASE_INSENSITIVE);
    cleri_object_t * k_pvariance = cleri_keyword(CLERI_GID_K_PVARIANCE, "pvariance", CLERI_CASE_INSENSITIVE);
    cleri_object_t * k_read = cleri_keyword(CLERI_GID_K_READ, "read", CLERI_CASE_INSENSITIVE);
    cleri_object_t * k_received_points = cleri_keyword(CLERI_GID_K_RECEIVED_POINTS, "received_points", CLERI_CASE_INSENSITIVE);
    cleri_object_t * k_reindex_progress = cleri_keyword(CLERI_GID_K_REINDEX_PROGRESS, "reindex_progress", CLERI_CASE_INSENSITIVE);
    cleri_object_t * k_revoke = cleri_keyword(CLERI_GID_K_REVOKE, "revoke", CLERI_CASE_INSENSITIVE);
    cleri_object_t * k_select = cleri_keyword(CLERI_GID_K_SELECT, "select", CLERI_CASE_INSENSITIVE);
    cleri_object_t * k_series = cleri_keyword(CLERI_GID_K_SERIES, "series", CLERI_CASE_INSENSITIVE);
    cleri_object_t * k_server = cleri_keyword(CLERI_GID_K_SERVER, "server", CLERI_CASE_INSENSITIVE);
    cleri_object_t * k_servers = cleri_keyword(CLERI_GID_K_SERVERS, "servers", CLERI_CASE_INSENSITIVE);
    cleri_object_t * k_set = cleri_keyword(CLERI_GID_K_SET, "set", CLERI_CASE_INSENSITIVE);
    cleri_object_t * k_sid = cleri_keyword(CLERI_GID_K_SID, "sid", CLERI_CASE_INSENSITIVE);
    cleri_object_t * k_shards = cleri_keyword(CLERI_GID_K_SHARDS, "shards", CLERI_CASE_INSENSITIVE);
    cleri_object_t * k_show = cleri_keyword(CLERI_GID_K_SHOW, "show", CLERI_CASE_INSENSITIVE);
    cleri_object_t * k_size = cleri_keyword(CLERI_GID_K_SIZE, "size", CLERI_CASE_INSENSITIVE);
    cleri_object_t * k_start = cleri_keyword(CLERI_GID_K_START, "start", CLERI_CASE_INSENSITIVE);
    cleri_object_t * k_startup_time = cleri_keyword(CLERI_GID_K_STARTUP_TIME, "startup_time", CLERI_CASE_INSENSITIVE);
    cleri_object_t * k_status = cleri_keyword(CLERI_GID_K_STATUS, "status", CLERI_CASE_INSENSITIVE);
    cleri_object_t * k_string = cleri_keyword(CLERI_GID_K_STRING, "string", CLERI_CASE_INSENSITIVE);
    cleri_object_t * k_suffix = cleri_keyword(CLERI_GID_K_SUFFIX, "suffix", CLERI_CASE_INSENSITIVE);
    cleri_object_t * k_sum = cleri_keyword(CLERI_GID_K_SUM, "sum", CLERI_CASE_INSENSITIVE);
    cleri_object_t * k_symmetric_difference = cleri_choice(
        CLERI_GID_K_SYMMETRIC_DIFFERENCE,
        CLERI_FIRST_MATCH,
        2,
        cleri_token(CLERI_NONE, "^"),
        cleri_keyword(CLERI_NONE, "symmetric_difference", CLERI_CASE_INSENSITIVE)
    );
    cleri_object_t * k_sync_progress = cleri_keyword(CLERI_GID_K_SYNC_PROGRESS, "sync_progress", CLERI_CASE_INSENSITIVE);
    cleri_object_t * k_timeit = cleri_keyword(CLERI_GID_K_TIMEIT, "timeit", CLERI_CASE_INSENSITIVE);
    cleri_object_t * k_timezone = cleri_keyword(CLERI_GID_K_TIMEZONE, "timezone", CLERI_CASE_INSENSITIVE);
    cleri_object_t * k_time_precision = cleri_keyword(CLERI_GID_K_TIME_PRECISION, "time_precision", CLERI_CASE_INSENSITIVE);
    cleri_object_t * k_to = cleri_keyword(CLERI_GID_K_TO, "to", CLERI_CASE_INSENSITIVE);
    cleri_object_t * k_true = cleri_keyword(CLERI_GID_K_TRUE, "true", CLERI_CASE_INSENSITIVE);
    cleri_object_t * k_type = cleri_keyword(CLERI_GID_K_TYPE, "type", CLERI_CASE_INSENSITIVE);
    cleri_object_t * k_union = cleri_choice(
        CLERI_GID_K_UNION,
        CLERI_FIRST_MATCH,
        2,
        cleri_tokens(CLERI_NONE, ", |"),
        cleri_keyword(CLERI_NONE, "union", CLERI_CASE_INSENSITIVE)
    );
    cleri_object_t * k_uptime = cleri_keyword(CLERI_GID_K_UPTIME, "uptime", CLERI_CASE_INSENSITIVE);
    cleri_object_t * k_user = cleri_keyword(CLERI_GID_K_USER, "user", CLERI_CASE_INSENSITIVE);
    cleri_object_t * k_users = cleri_keyword(CLERI_GID_K_USERS, "users", CLERI_CASE_INSENSITIVE);
    cleri_object_t * k_using = cleri_keyword(CLERI_GID_K_USING, "using", CLERI_CASE_INSENSITIVE);
    cleri_object_t * k_uuid = cleri_keyword(CLERI_GID_K_UUID, "uuid", CLERI_CASE_INSENSITIVE);
    cleri_object_t * k_variance = cleri_keyword(CLERI_GID_K_VARIANCE, "variance", CLERI_CASE_INSENSITIVE);
    cleri_object_t * k_version = cleri_keyword(CLERI_GID_K_VERSION, "version", CLERI_CASE_INSENSITIVE);
    cleri_object_t * k_warning = cleri_keyword(CLERI_GID_K_WARNING, "warning", CLERI_CASE_INSENSITIVE);
    cleri_object_t * k_where = cleri_keyword(CLERI_GID_K_WHERE, "where", CLERI_CASE_INSENSITIVE);
    cleri_object_t * k_who_am_i = cleri_keyword(CLERI_GID_K_WHO_AM_I, "who_am_i", CLERI_CASE_INSENSITIVE);
    cleri_object_t * k_write = cleri_keyword(CLERI_GID_K_WRITE, "write", CLERI_CASE_INSENSITIVE);
    cleri_object_t * c_difference = cleri_choice(
        CLERI_GID_C_DIFFERENCE,
        CLERI_FIRST_MATCH,
        2,
        cleri_token(CLERI_NONE, "-"),
        k_difference
    );
    cleri_object_t * access_keywords = cleri_choice(
        CLERI_GID_ACCESS_KEYWORDS,
        CLERI_FIRST_MATCH,
        14,
        k_read,
        k_write,
        k_modify,
        k_full,
        k_select,
        k_show,
        k_list,
        k_count,
        k_create,
        k_insert,
        k_drop,
        k_grant,
        k_revoke,
        k_alter
    );
    cleri_object_t * _boolean = cleri_choice(
        CLERI_GID__BOOLEAN,
        CLERI_FIRST_MATCH,
        2,
        k_true,
        k_false
    );
    cleri_object_t * log_keywords = cleri_choice(
        CLERI_GID_LOG_KEYWORDS,
        CLERI_FIRST_MATCH,
        5,
        k_debug,
        k_info,
        k_warning,
        k_error,
        k_critical
    );
    cleri_object_t * int_expr = cleri_prio(
        CLERI_GID_INT_EXPR,
        3,
        r_integer,
        cleri_sequence(
            CLERI_NONE,
            3,
            cleri_token(CLERI_NONE, "("),
            CLERI_THIS,
            cleri_token(CLERI_NONE, ")")
        ),
        cleri_sequence(
            CLERI_NONE,
            3,
            CLERI_THIS,
            cleri_tokens(CLERI_NONE, "+ - * % /"),
            CLERI_THIS
        )
    );
    cleri_object_t * string = cleri_choice(
        CLERI_GID_STRING,
        CLERI_FIRST_MATCH,
        2,
        r_singleq_str,
        r_doubleq_str
    );
    cleri_object_t * time_expr = cleri_prio(
        CLERI_GID_TIME_EXPR,
        6,
        r_time_str,
        k_now,
        string,
        r_integer,
        cleri_sequence(
            CLERI_NONE,
            3,
            cleri_token(CLERI_NONE, "("),
            CLERI_THIS,
            cleri_token(CLERI_NONE, ")")
        ),
        cleri_sequence(
            CLERI_NONE,
            3,
            CLERI_THIS,
            cleri_tokens(CLERI_NONE, "+ - * % /"),
            CLERI_THIS
        )
    );
    cleri_object_t * series_columns = cleri_list(CLERI_GID_SERIES_COLUMNS, cleri_choice(
        CLERI_NONE,
        CLERI_FIRST_MATCH,
        6,
        k_name,
        k_type,
        k_length,
        k_start,
        k_end,
        k_pool
    ), cleri_token(CLERI_NONE, ","), 1, 0, 0);
    cleri_object_t * shard_columns = cleri_list(CLERI_GID_SHARD_COLUMNS, cleri_choice(
        CLERI_NONE,
        CLERI_FIRST_MATCH,
        8,
        k_sid,
        k_pool,
        k_server,
        k_size,
        k_start,
        k_end,
        k_type,
        k_status
    ), cleri_token(CLERI_NONE, ","), 1, 0, 0);
    cleri_object_t * server_columns = cleri_list(CLERI_GID_SERVER_COLUMNS, cleri_choice(
        CLERI_NONE,
        CLERI_FIRST_MATCH,
        23,
        k_address,
        k_buffer_path,
        k_buffer_size,
        k_dbpath,
        k_ip_support,
        k_libuv,
        k_name,
        k_port,
        k_uuid,
        k_pool,
        k_version,
        k_online,
        k_startup_time,
        k_status,
        k_active_handles,
        k_log_level,
        k_max_open_files,
        k_mem_usage,
        k_open_files,
        k_received_points,
        k_reindex_progress,
        k_sync_progress,
        k_uptime
    ), cleri_token(CLERI_NONE, ","), 1, 0, 0);
    cleri_object_t * group_columns = cleri_list(CLERI_GID_GROUP_COLUMNS, cleri_choice(
        CLERI_NONE,
        CLERI_FIRST_MATCH,
        3,
        k_expression,
        k_name,
        k_series
    ), cleri_token(CLERI_NONE, ","), 1, 0, 0);
    cleri_object_t * user_columns = cleri_list(CLERI_GID_USER_COLUMNS, cleri_choice(
        CLERI_NONE,
        CLERI_FIRST_MATCH,
        2,
        k_name,
        k_access
    ), cleri_token(CLERI_NONE, ","), 1, 0, 0);
    cleri_object_t * pool_props = cleri_choice(
        CLERI_GID_POOL_PROPS,
        CLERI_FIRST_MATCH,
        3,
        k_pool,
        k_servers,
        k_series
    );
    cleri_object_t * pool_columns = cleri_list(CLERI_GID_POOL_COLUMNS, pool_props, cleri_token(CLERI_NONE, ","), 1, 0, 0);
    cleri_object_t * bool_operator = cleri_tokens(CLERI_GID_BOOL_OPERATOR, "== !=");
    cleri_object_t * int_operator = cleri_tokens(CLERI_GID_INT_OPERATOR, "== != <= >= < >");
    cleri_object_t * str_operator = cleri_tokens(CLERI_GID_STR_OPERATOR, "== != <= >= !~ < > ~");
    cleri_object_t * where_group = cleri_sequence(
        CLERI_GID_WHERE_GROUP,
        2,
        k_where,
        cleri_prio(
            CLERI_NONE,
            5,
            cleri_sequence(
                CLERI_NONE,
                3,
                k_series,
                int_operator,
                int_expr
            ),
            cleri_sequence(
                CLERI_NONE,
                3,
                cleri_choice(
                    CLERI_NONE,
                    CLERI_FIRST_MATCH,
                    2,
                    k_expression,
                    k_name
                ),
                str_operator,
                string
            ),
            cleri_sequence(
                CLERI_NONE,
                3,
                cleri_token(CLERI_NONE, "("),
                CLERI_THIS,
                cleri_token(CLERI_NONE, ")")
            ),
            cleri_sequence(
                CLERI_NONE,
                3,
                CLERI_THIS,
                k_and,
                CLERI_THIS
            ),
            cleri_sequence(
                CLERI_NONE,
                3,
                CLERI_THIS,
                k_or,
                CLERI_THIS
            )
        )
    );
    cleri_object_t * where_pool = cleri_sequence(
        CLERI_GID_WHERE_POOL,
        2,
        k_where,
        cleri_prio(
            CLERI_NONE,
            4,
            cleri_sequence(
                CLERI_NONE,
                3,
                pool_props,
                int_operator,
                int_expr
            ),
            cleri_sequence(
                CLERI_NONE,
                3,
                cleri_token(CLERI_NONE, "("),
                CLERI_THIS,
                cleri_token(CLERI_NONE, ")")
            ),
            cleri_sequence(
                CLERI_NONE,
                3,
                CLERI_THIS,
                k_and,
                CLERI_THIS
            ),
            cleri_sequence(
                CLERI_NONE,
                3,
                CLERI_THIS,
                k_or,
                CLERI_THIS
            )
        )
    );
    cleri_object_t * where_series = cleri_sequence(
        CLERI_GID_WHERE_SERIES,
        2,
        k_where,
        cleri_prio(
            CLERI_NONE,
            7,
            cleri_sequence(
                CLERI_NONE,
                3,
                cleri_choice(
                    CLERI_NONE,
                    CLERI_FIRST_MATCH,
                    2,
                    k_length,
                    k_pool
                ),
                int_operator,
                int_expr
            ),
            cleri_sequence(
                CLERI_NONE,
                3,
                k_name,
                str_operator,
                string
            ),
            cleri_sequence(
                CLERI_NONE,
                3,
                cleri_choice(
                    CLERI_NONE,
                    CLERI_FIRST_MATCH,
                    2,
                    k_start,
                    k_end
                ),
                int_operator,
                time_expr
            ),
            cleri_sequence(
                CLERI_NONE,
                3,
                k_type,
                bool_operator,
                cleri_choice(
                    CLERI_NONE,
                    CLERI_FIRST_MATCH,
                    3,
                    k_string,
                    k_integer,
                    k_float
                )
            ),
            cleri_sequence(
                CLERI_NONE,
                3,
                cleri_token(CLERI_NONE, "("),
                CLERI_THIS,
                cleri_token(CLERI_NONE, ")")
            ),
            cleri_sequence(
                CLERI_NONE,
                3,
                CLERI_THIS,
                k_and,
                CLERI_THIS
            ),
            cleri_sequence(
                CLERI_NONE,
                3,
                CLERI_THIS,
                k_or,
                CLERI_THIS
            )
        )
    );
    cleri_object_t * where_server = cleri_sequence(
        CLERI_GID_WHERE_SERVER,
        2,
        k_where,
        cleri_prio(
            CLERI_NONE,
            7,
            cleri_sequence(
                CLERI_NONE,
                3,
                cleri_choice(
                    CLERI_NONE,
                    CLERI_FIRST_MATCH,
                    10,
                    k_active_handles,
                    k_buffer_size,
                    k_port,
                    k_pool,
                    k_startup_time,
                    k_max_open_files,
                    k_mem_usage,
                    k_open_files,
                    k_received_points,
                    k_uptime
                ),
                int_operator,
                int_expr
            ),
            cleri_sequence(
                CLERI_NONE,
                3,
                cleri_choice(
                    CLERI_NONE,
                    CLERI_FIRST_MATCH,
                    11,
                    k_address,
                    k_buffer_path,
                    k_dbpath,
                    k_ip_support,
                    k_libuv,
                    k_name,
                    k_uuid,
                    k_version,
                    k_status,
                    k_reindex_progress,
                    k_sync_progress
                ),
                str_operator,
                string
            ),
            cleri_sequence(
                CLERI_NONE,
                3,
                k_online,
                bool_operator,
                _boolean
            ),
            cleri_sequence(
                CLERI_NONE,
                3,
                k_log_level,
                int_operator,
                log_keywords
            ),
            cleri_sequence(
                CLERI_NONE,
                3,
                cleri_token(CLERI_NONE, "("),
                CLERI_THIS,
                cleri_token(CLERI_NONE, ")")
            ),
            cleri_sequence(
                CLERI_NONE,
                3,
                CLERI_THIS,
                k_and,
                CLERI_THIS
            ),
            cleri_sequence(
                CLERI_NONE,
                3,
                CLERI_THIS,
                k_or,
                CLERI_THIS
            )
        )
    );
    cleri_object_t * where_shard = cleri_sequence(
        CLERI_GID_WHERE_SHARD,
        2,
        k_where,
        cleri_prio(
            CLERI_NONE,
            7,
            cleri_sequence(
                CLERI_NONE,
                3,
                cleri_choice(
                    CLERI_NONE,
                    CLERI_FIRST_MATCH,
                    3,
                    k_sid,
                    k_pool,
                    k_size
                ),
                int_operator,
                int_expr
            ),
            cleri_sequence(
                CLERI_NONE,
                3,
                cleri_choice(
                    CLERI_NONE,
                    CLERI_MOST_GREEDY,
                    2,
                    k_server,
                    k_status
                ),
                str_operator,
                string
            ),
            cleri_sequence(
                CLERI_NONE,
                3,
                cleri_choice(
                    CLERI_NONE,
                    CLERI_FIRST_MATCH,
                    2,
                    k_start,
                    k_end
                ),
                int_operator,
                time_expr
            ),
            cleri_sequence(
                CLERI_NONE,
                3,
                k_type,
                bool_operator,
                cleri_choice(
                    CLERI_NONE,
                    CLERI_FIRST_MATCH,
                    2,
                    k_number,
                    k_log
                )
            ),
            cleri_sequence(
                CLERI_NONE,
                3,
                cleri_token(CLERI_NONE, "("),
                CLERI_THIS,
                cleri_token(CLERI_NONE, ")")
            ),
            cleri_sequence(
                CLERI_NONE,
                3,
                CLERI_THIS,
                k_and,
                CLERI_THIS
            ),
            cleri_sequence(
                CLERI_NONE,
                3,
                CLERI_THIS,
                k_or,
                CLERI_THIS
            )
        )
    );
    cleri_object_t * where_user = cleri_sequence(
        CLERI_GID_WHERE_USER,
        2,
        k_where,
        cleri_prio(
            CLERI_NONE,
            5,
            cleri_sequence(
                CLERI_NONE,
                3,
                k_name,
                str_operator,
                string
            ),
            cleri_sequence(
                CLERI_NONE,
                3,
                k_access,
                int_operator,
                access_keywords
            ),
            cleri_sequence(
                CLERI_NONE,
                3,
                cleri_token(CLERI_NONE, "("),
                CLERI_THIS,
                cleri_token(CLERI_NONE, ")")
            ),
            cleri_sequence(
                CLERI_NONE,
                3,
                CLERI_THIS,
                k_and,
                CLERI_THIS
            ),
            cleri_sequence(
                CLERI_NONE,
                3,
                CLERI_THIS,
                k_or,
                CLERI_THIS
            )
        )
    );
    cleri_object_t * series_sep = cleri_choice(
        CLERI_GID_SERIES_SEP,
        CLERI_FIRST_MATCH,
        4,
        k_union,
        c_difference,
        k_intersection,
        k_symmetric_difference
    );
    cleri_object_t * series_name = cleri_repeat(CLERI_GID_SERIES_NAME, string, 1, 1);
    cleri_object_t * group_name = cleri_repeat(CLERI_GID_GROUP_NAME, r_grave_str, 1, 1);
    cleri_object_t * series_re = cleri_repeat(CLERI_GID_SERIES_RE, r_regex, 1, 1);
    cleri_object_t * uuid = cleri_choice(
        CLERI_GID_UUID,
        CLERI_FIRST_MATCH,
        2,
        r_uuid_str,
        string
    );
    cleri_object_t * group_match = cleri_repeat(CLERI_GID_GROUP_MATCH, r_grave_str, 1, 1);
    cleri_object_t * series_match = cleri_list(CLERI_GID_SERIES_MATCH, cleri_choice(
        CLERI_NONE,
        CLERI_FIRST_MATCH,
        3,
        series_name,
        group_match,
        series_re
    ), series_sep, 1, 0, 0);
    cleri_object_t * limit_expr = cleri_sequence(
        CLERI_GID_LIMIT_EXPR,
        2,
        k_limit,
        int_expr
    );
    cleri_object_t * before_expr = cleri_sequence(
        CLERI_GID_BEFORE_EXPR,
        2,
        k_before,
        time_expr
    );
    cleri_object_t * after_expr = cleri_sequence(
        CLERI_GID_AFTER_EXPR,
        2,
        k_after,
        time_expr
    );
    cleri_object_t * between_expr = cleri_sequence(
        CLERI_GID_BETWEEN_EXPR,
        4,
        k_between,
        time_expr,
        k_and,
        time_expr
    );
    cleri_object_t * access_expr = cleri_list(CLERI_GID_ACCESS_EXPR, access_keywords, cleri_token(CLERI_NONE, ","), 1, 0, 0);
    cleri_object_t * prefix_expr = cleri_sequence(
        CLERI_GID_PREFIX_EXPR,
        2,
        k_prefix,
        string
    );
    cleri_object_t * suffix_expr = cleri_sequence(
        CLERI_GID_SUFFIX_EXPR,
        2,
        k_suffix,
        string
    );
    cleri_object_t * f_points = cleri_choice(
        CLERI_GID_F_POINTS,
        CLERI_FIRST_MATCH,
        2,
        cleri_token(CLERI_NONE, "*"),
        k_points
    );
    cleri_object_t * f_difference = cleri_sequence(
        CLERI_GID_F_DIFFERENCE,
        4,
        k_difference,
        cleri_token(CLERI_NONE, "("),
        cleri_optional(CLERI_NONE, time_expr),
        cleri_token(CLERI_NONE, ")")
    );
    cleri_object_t * f_derivative = cleri_sequence(
        CLERI_GID_F_DERIVATIVE,
        4,
        k_derivative,
        cleri_token(CLERI_NONE, "("),
        cleri_list(CLERI_NONE, time_expr, cleri_token(CLERI_NONE, ","), 0, 2, 0),
        cleri_token(CLERI_NONE, ")")
    );
    cleri_object_t * f_mean = cleri_sequence(
        CLERI_GID_F_MEAN,
        4,
        k_mean,
        cleri_token(CLERI_NONE, "("),
        time_expr,
        cleri_token(CLERI_NONE, ")")
    );
    cleri_object_t * f_median = cleri_sequence(
        CLERI_GID_F_MEDIAN,
        4,
        k_median,
        cleri_token(CLERI_NONE, "("),
        time_expr,
        cleri_token(CLERI_NONE, ")")
    );
    cleri_object_t * f_median_low = cleri_sequence(
        CLERI_GID_F_MEDIAN_LOW,
        4,
        k_median_low,
        cleri_token(CLERI_NONE, "("),
        time_expr,
        cleri_token(CLERI_NONE, ")")
    );
    cleri_object_t * f_median_high = cleri_sequence(
        CLERI_GID_F_MEDIAN_HIGH,
        4,
        k_median_high,
        cleri_token(CLERI_NONE, "("),
        time_expr,
        cleri_token(CLERI_NONE, ")")
    );
    cleri_object_t * f_sum = cleri_sequence(
        CLERI_GID_F_SUM,
        4,
        k_sum,
        cleri_token(CLERI_NONE, "("),
        time_expr,
        cleri_token(CLERI_NONE, ")")
    );
    cleri_object_t * f_min = cleri_sequence(
        CLERI_GID_F_MIN,
        4,
        k_min,
        cleri_token(CLERI_NONE, "("),
        time_expr,
        cleri_token(CLERI_NONE, ")")
    );
    cleri_object_t * f_max = cleri_sequence(
        CLERI_GID_F_MAX,
        4,
        k_max,
        cleri_token(CLERI_NONE, "("),
        time_expr,
        cleri_token(CLERI_NONE, ")")
    );
    cleri_object_t * f_count = cleri_sequence(
        CLERI_GID_F_COUNT,
        4,
        k_count,
        cleri_token(CLERI_NONE, "("),
        time_expr,
        cleri_token(CLERI_NONE, ")")
    );
    cleri_object_t * f_variance = cleri_sequence(
        CLERI_GID_F_VARIANCE,
        4,
        k_variance,
        cleri_token(CLERI_NONE, "("),
        time_expr,
        cleri_token(CLERI_NONE, ")")
    );
    cleri_object_t * f_pvariance = cleri_sequence(
        CLERI_GID_F_PVARIANCE,
        4,
        k_pvariance,
        cleri_token(CLERI_NONE, "("),
        time_expr,
        cleri_token(CLERI_NONE, ")")
    );
    cleri_object_t * f_filter = cleri_sequence(
        CLERI_GID_F_FILTER,
        5,
        k_filter,
        cleri_token(CLERI_NONE, "("),
        cleri_optional(CLERI_NONE, str_operator),
        cleri_choice(
            CLERI_NONE,
            CLERI_MOST_GREEDY,
            3,
            string,
            r_integer,
            r_float
        ),
        cleri_token(CLERI_NONE, ")")
    );
    cleri_object_t * aggregate_functions = cleri_list(CLERI_GID_AGGREGATE_FUNCTIONS, cleri_choice(
        CLERI_NONE,
        CLERI_FIRST_MATCH,
        14,
        f_points,
        f_mean,
        f_sum,
        f_median,
        f_median_low,
        f_median_high,
        f_min,
        f_max,
        f_count,
        f_variance,
        f_pvariance,
        f_difference,
        f_derivative,
        f_filter
    ), cleri_token(CLERI_NONE, "=>"), 1, 0, 0);
    cleri_object_t * select_aggregate = cleri_sequence(
        CLERI_GID_SELECT_AGGREGATE,
        3,
        aggregate_functions,
        cleri_optional(CLERI_NONE, prefix_expr),
        cleri_optional(CLERI_NONE, suffix_expr)
    );
    cleri_object_t * merge_as = cleri_sequence(
        CLERI_GID_MERGE_AS,
        4,
        k_merge,
        k_as,
        string,
        cleri_optional(CLERI_NONE, cleri_sequence(
            CLERI_NONE,
            2,
            k_using,
            aggregate_functions
        ))
    );
    cleri_object_t * set_address = cleri_sequence(
        CLERI_GID_SET_ADDRESS,
        3,
        k_set,
        k_address,
        string
    );
    cleri_object_t * set_backup_mode = cleri_sequence(
        CLERI_GID_SET_BACKUP_MODE,
        3,
        k_set,
        k_backup_mode,
        _boolean
    );
    cleri_object_t * set_drop_threshold = cleri_sequence(
        CLERI_GID_SET_DROP_THRESHOLD,
        3,
        k_set,
        k_drop_threshold,
        r_float
    );
    cleri_object_t * set_expression = cleri_sequence(
        CLERI_GID_SET_EXPRESSION,
        3,
        k_set,
        k_expression,
        r_regex
    );
    cleri_object_t * set_ignore_threshold = cleri_sequence(
        CLERI_GID_SET_IGNORE_THRESHOLD,
        3,
        k_set,
        k_ignore_threshold,
        _boolean
    );
    cleri_object_t * set_log_level = cleri_sequence(
        CLERI_GID_SET_LOG_LEVEL,
        3,
        k_set,
        k_log_level,
        log_keywords
    );
    cleri_object_t * set_name = cleri_sequence(
        CLERI_GID_SET_NAME,
        3,
        k_set,
        k_name,
        string
    );
    cleri_object_t * set_password = cleri_sequence(
        CLERI_GID_SET_PASSWORD,
        3,
        k_set,
        k_password,
        string
    );
    cleri_object_t * set_port = cleri_sequence(
        CLERI_GID_SET_PORT,
        3,
        k_set,
        k_port,
        r_uinteger
    );
    cleri_object_t * set_timezone = cleri_sequence(
        CLERI_GID_SET_TIMEZONE,
        3,
        k_set,
        k_timezone,
        string
    );
    cleri_object_t * alter_database = cleri_sequence(
        CLERI_GID_ALTER_DATABASE,
        2,
        k_database,
        cleri_choice(
            CLERI_NONE,
            CLERI_FIRST_MATCH,
            2,
            set_drop_threshold,
            set_timezone
        )
    );
    cleri_object_t * alter_group = cleri_sequence(
        CLERI_GID_ALTER_GROUP,
        3,
        k_group,
        group_name,
        cleri_choice(
            CLERI_NONE,
            CLERI_FIRST_MATCH,
            2,
            set_expression,
            set_name
        )
    );
    cleri_object_t * alter_server = cleri_sequence(
        CLERI_GID_ALTER_SERVER,
        3,
        k_server,
        uuid,
        cleri_choice(
            CLERI_NONE,
            CLERI_FIRST_MATCH,
            4,
            set_log_level,
            set_backup_mode,
            set_address,
            set_port
        )
    );
    cleri_object_t * alter_servers = cleri_sequence(
        CLERI_GID_ALTER_SERVERS,
        3,
        k_servers,
        cleri_optional(CLERI_NONE, where_server),
        set_log_level
    );
    cleri_object_t * alter_user = cleri_sequence(
        CLERI_GID_ALTER_USER,
        3,
        k_user,
        string,
        cleri_choice(
            CLERI_NONE,
            CLERI_FIRST_MATCH,
            2,
            set_password,
            set_name
        )
    );
    cleri_object_t * count_groups = cleri_sequence(
        CLERI_GID_COUNT_GROUPS,
        2,
        k_groups,
        cleri_optional(CLERI_NONE, where_group)
    );
    cleri_object_t * count_pools = cleri_sequence(
        CLERI_GID_COUNT_POOLS,
        2,
        k_pools,
        cleri_optional(CLERI_NONE, where_pool)
    );
    cleri_object_t * count_series = cleri_sequence(
        CLERI_GID_COUNT_SERIES,
        3,
        k_series,
        cleri_optional(CLERI_NONE, series_match),
        cleri_optional(CLERI_NONE, where_series)
    );
    cleri_object_t * count_servers = cleri_sequence(
        CLERI_GID_COUNT_SERVERS,
        2,
        k_servers,
        cleri_optional(CLERI_NONE, where_server)
    );
    cleri_object_t * count_servers_received = cleri_sequence(
        CLERI_GID_COUNT_SERVERS_RECEIVED,
        3,
        k_servers,
        k_received_points,
        cleri_optional(CLERI_NONE, where_server)
    );
    cleri_object_t * count_shards = cleri_sequence(
        CLERI_GID_COUNT_SHARDS,
        2,
        k_shards,
        cleri_optional(CLERI_NONE, where_shard)
    );
    cleri_object_t * count_shards_size = cleri_sequence(
        CLERI_GID_COUNT_SHARDS_SIZE,
        3,
        k_shards,
        k_size,
        cleri_optional(CLERI_NONE, where_shard)
    );
    cleri_object_t * count_users = cleri_sequence(
        CLERI_GID_COUNT_USERS,
        2,
        k_users,
        cleri_optional(CLERI_NONE, where_user)
    );
    cleri_object_t * count_series_length = cleri_sequence(
        CLERI_GID_COUNT_SERIES_LENGTH,
        4,
        k_series,
        k_length,
        cleri_optional(CLERI_NONE, series_match),
        cleri_optional(CLERI_NONE, where_series)
    );
    cleri_object_t * create_group = cleri_sequence(
        CLERI_GID_CREATE_GROUP,
        4,
        k_group,
        group_name,
        k_for,
        r_regex
    );
    cleri_object_t * create_user = cleri_sequence(
        CLERI_GID_CREATE_USER,
        3,
        k_user,
        string,
        set_password
    );
    cleri_object_t * drop_group = cleri_sequence(
        CLERI_GID_DROP_GROUP,
        2,
        k_group,
        group_name
    );
    cleri_object_t * drop_series = cleri_sequence(
        CLERI_GID_DROP_SERIES,
        4,
        k_series,
        cleri_optional(CLERI_NONE, series_match),
        cleri_optional(CLERI_NONE, where_series),
        cleri_optional(CLERI_NONE, set_ignore_threshold)
    );
    cleri_object_t * drop_shards = cleri_sequence(
        CLERI_GID_DROP_SHARDS,
        3,
        k_shards,
        cleri_optional(CLERI_NONE, where_shard),
        cleri_optional(CLERI_NONE, set_ignore_threshold)
    );
    cleri_object_t * drop_server = cleri_sequence(
        CLERI_GID_DROP_SERVER,
        2,
        k_server,
        uuid
    );
    cleri_object_t * drop_user = cleri_sequence(
        CLERI_GID_DROP_USER,
        2,
        k_user,
        string
    );
    cleri_object_t * grant_user = cleri_sequence(
        CLERI_GID_GRANT_USER,
        3,
        k_user,
        string,
        cleri_optional(CLERI_NONE, set_password)
    );
    cleri_object_t * list_groups = cleri_sequence(
        CLERI_GID_LIST_GROUPS,
        3,
        k_groups,
        cleri_optional(CLERI_NONE, group_columns),
        cleri_optional(CLERI_NONE, where_group)
    );
    cleri_object_t * list_pools = cleri_sequence(
        CLERI_GID_LIST_POOLS,
        3,
        k_pools,
        cleri_optional(CLERI_NONE, pool_columns),
        cleri_optional(CLERI_NONE, where_pool)
    );
    cleri_object_t * list_series = cleri_sequence(
        CLERI_GID_LIST_SERIES,
        4,
        k_series,
        cleri_optional(CLERI_NONE, series_columns),
        cleri_optional(CLERI_NONE, series_match),
        cleri_optional(CLERI_NONE, where_series)
    );
    cleri_object_t * list_servers = cleri_sequence(
        CLERI_GID_LIST_SERVERS,
        3,
        k_servers,
        cleri_optional(CLERI_NONE, server_columns),
        cleri_optional(CLERI_NONE, where_server)
    );
    cleri_object_t * list_shards = cleri_sequence(
        CLERI_GID_LIST_SHARDS,
        3,
        k_shards,
        cleri_optional(CLERI_NONE, shard_columns),
        cleri_optional(CLERI_NONE, where_shard)
    );
    cleri_object_t * list_users = cleri_sequence(
        CLERI_GID_LIST_USERS,
        3,
        k_users,
        cleri_optional(CLERI_NONE, user_columns),
        cleri_optional(CLERI_NONE, where_user)
    );
    cleri_object_t * revoke_user = cleri_sequence(
        CLERI_GID_REVOKE_USER,
        2,
        k_user,
        string
    );
    cleri_object_t * alter_stmt = cleri_sequence(
        CLERI_GID_ALTER_STMT,
        2,
        k_alter,
        cleri_choice(
            CLERI_NONE,
            CLERI_FIRST_MATCH,
            5,
            alter_user,
            alter_group,
            alter_server,
            alter_servers,
            alter_database
        )
    );
    cleri_object_t * calc_stmt = cleri_repeat(CLERI_GID_CALC_STMT, time_expr, 1, 1);
    cleri_object_t * count_stmt = cleri_sequence(
        CLERI_GID_COUNT_STMT,
        2,
        k_count,
        cleri_choice(
            CLERI_NONE,
            CLERI_MOST_GREEDY,
            9,
            count_groups,
            count_pools,
            count_series,
            count_servers,
            count_servers_received,
            count_shards,
            count_shards_size,
            count_users,
            count_series_length
        )
    );
    cleri_object_t * create_stmt = cleri_sequence(
        CLERI_GID_CREATE_STMT,
        2,
        k_create,
        cleri_choice(
            CLERI_NONE,
            CLERI_MOST_GREEDY,
            2,
            create_group,
            create_user
        )
    );
    cleri_object_t * drop_stmt = cleri_sequence(
        CLERI_GID_DROP_STMT,
        2,
        k_drop,
        cleri_choice(
            CLERI_NONE,
            CLERI_FIRST_MATCH,
            5,
            drop_group,
            drop_series,
            drop_shards,
            drop_server,
            drop_user
        )
    );
    cleri_object_t * grant_stmt = cleri_sequence(
        CLERI_GID_GRANT_STMT,
        4,
        k_grant,
        access_expr,
        k_to,
        cleri_choice(
            CLERI_NONE,
            CLERI_FIRST_MATCH,
            1,
            grant_user
        )
    );
    cleri_object_t * list_stmt = cleri_sequence(
        CLERI_GID_LIST_STMT,
        3,
        k_list,
        cleri_choice(
            CLERI_NONE,
            CLERI_FIRST_MATCH,
            6,
            list_series,
            list_users,
            list_shards,
            list_groups,
            list_servers,
            list_pools
        ),
        cleri_optional(CLERI_NONE, limit_expr)
    );
    cleri_object_t * revoke_stmt = cleri_sequence(
        CLERI_GID_REVOKE_STMT,
        4,
        k_revoke,
        access_expr,
        k_from,
        cleri_choice(
            CLERI_NONE,
            CLERI_FIRST_MATCH,
            1,
            revoke_user
        )
    );
    cleri_object_t * select_stmt = cleri_sequence(
        CLERI_GID_SELECT_STMT,
        7,
        k_select,
        cleri_list(CLERI_NONE, select_aggregate, cleri_token(CLERI_NONE, ","), 1, 0, 0),
        k_from,
        series_match,
        cleri_optional(CLERI_NONE, where_series),
        cleri_optional(CLERI_NONE, cleri_choice(
            CLERI_NONE,
            CLERI_FIRST_MATCH,
            3,
            after_expr,
            between_expr,
            before_expr
        )),
        cleri_optional(CLERI_NONE, merge_as)
    );
    cleri_object_t * show_stmt = cleri_sequence(
        CLERI_GID_SHOW_STMT,
        2,
        k_show,
        cleri_list(CLERI_NONE, cleri_choice(
            CLERI_NONE,
            CLERI_FIRST_MATCH,
            27,
            k_active_handles,
            k_buffer_path,
            k_buffer_size,
            k_dbname,
            k_dbpath,
            k_drop_threshold,
            k_duration_log,
            k_duration_num,
            k_ip_support,
            k_libuv,
            k_log_level,
            k_max_open_files,
            k_mem_usage,
            k_open_files,
            k_pool,
            k_received_points,
            k_reindex_progress,
            k_server,
            k_startup_time,
            k_status,
            k_sync_progress,
            k_time_precision,
            k_timezone,
            k_uptime,
            k_uuid,
            k_version,
            k_who_am_i
        ), cleri_token(CLERI_NONE, ","), 0, 0, 0)
    );
    cleri_object_t * timeit_stmt = cleri_repeat(CLERI_GID_TIMEIT_STMT, k_timeit, 1, 1);
    cleri_object_t * help_select = cleri_keyword(CLERI_GID_HELP_SELECT, "select", CLERI_CASE_INSENSITIVE);
    cleri_object_t * help_alter_group = cleri_keyword(CLERI_GID_HELP_ALTER_GROUP, "group", CLERI_CASE_INSENSITIVE);
    cleri_object_t * help_alter_database = cleri_keyword(CLERI_GID_HELP_ALTER_DATABASE, "database", CLERI_CASE_INSENSITIVE);
    cleri_object_t * help_alter_user = cleri_keyword(CLERI_GID_HELP_ALTER_USER, "user", CLERI_CASE_INSENSITIVE);
    cleri_object_t * help_alter_server = cleri_keyword(CLERI_GID_HELP_ALTER_SERVER, "server", CLERI_CASE_INSENSITIVE);
    cleri_object_t * help_alter = cleri_sequence(
        CLERI_GID_HELP_ALTER,
        2,
        k_alter,
        cleri_optional(CLERI_NONE, cleri_choice(
            CLERI_NONE,
            CLERI_MOST_GREEDY,
            4,
            help_alter_group,
            help_alter_database,
            help_alter_user,
            help_alter_server
        ))
    );
    cleri_object_t * help_functions = cleri_keyword(CLERI_GID_HELP_FUNCTIONS, "functions", CLERI_CASE_INSENSITIVE);
    cleri_object_t * help_create_user = cleri_keyword(CLERI_GID_HELP_CREATE_USER, "user", CLERI_CASE_INSENSITIVE);
    cleri_object_t * help_create_group = cleri_keyword(CLERI_GID_HELP_CREATE_GROUP, "group", CLERI_CASE_INSENSITIVE);
    cleri_object_t * help_create = cleri_sequence(
        CLERI_GID_HELP_CREATE,
        2,
        k_create,
        cleri_optional(CLERI_NONE, cleri_choice(
            CLERI_NONE,
            CLERI_MOST_GREEDY,
            2,
            help_create_user,
            help_create_group
        ))
    );
    cleri_object_t * help_count_groups = cleri_keyword(CLERI_GID_HELP_COUNT_GROUPS, "groups", CLERI_CASE_INSENSITIVE);
    cleri_object_t * help_count_series = cleri_keyword(CLERI_GID_HELP_COUNT_SERIES, "series", CLERI_CASE_INSENSITIVE);
    cleri_object_t * help_count_shards = cleri_keyword(CLERI_GID_HELP_COUNT_SHARDS, "shards", CLERI_CASE_INSENSITIVE);
    cleri_object_t * help_count_servers = cleri_keyword(CLERI_GID_HELP_COUNT_SERVERS, "servers", CLERI_CASE_INSENSITIVE);
    cleri_object_t * help_count_pools = cleri_keyword(CLERI_GID_HELP_COUNT_POOLS, "pools", CLERI_CASE_INSENSITIVE);
    cleri_object_t * help_count_users = cleri_keyword(CLERI_GID_HELP_COUNT_USERS, "users", CLERI_CASE_INSENSITIVE);
    cleri_object_t * help_count = cleri_sequence(
        CLERI_GID_HELP_COUNT,
        2,
        k_count,
        cleri_optional(CLERI_NONE, cleri_choice(
            CLERI_NONE,
            CLERI_MOST_GREEDY,
            6,
            help_count_groups,
            help_count_series,
            help_count_shards,
            help_count_servers,
            help_count_pools,
            help_count_users
        ))
    );
    cleri_object_t * help_show = cleri_keyword(CLERI_GID_HELP_SHOW, "show", CLERI_CASE_INSENSITIVE);
    cleri_object_t * help_revoke = cleri_keyword(CLERI_GID_HELP_REVOKE, "revoke", CLERI_CASE_INSENSITIVE);
    cleri_object_t * help_access = cleri_keyword(CLERI_GID_HELP_ACCESS, "access", CLERI_CASE_INSENSITIVE);
    cleri_object_t * help_timeit = cleri_keyword(CLERI_GID_HELP_TIMEIT, "timeit", CLERI_CASE_INSENSITIVE);
    cleri_object_t * help_list_series = cleri_keyword(CLERI_GID_HELP_LIST_SERIES, "series", CLERI_CASE_INSENSITIVE);
    cleri_object_t * help_list_shards = cleri_keyword(CLERI_GID_HELP_LIST_SHARDS, "shards", CLERI_CASE_INSENSITIVE);
    cleri_object_t * help_list_groups = cleri_keyword(CLERI_GID_HELP_LIST_GROUPS, "groups", CLERI_CASE_INSENSITIVE);
    cleri_object_t * help_list_users = cleri_keyword(CLERI_GID_HELP_LIST_USERS, "users", CLERI_CASE_INSENSITIVE);
    cleri_object_t * help_list_servers = cleri_keyword(CLERI_GID_HELP_LIST_SERVERS, "servers", CLERI_CASE_INSENSITIVE);
    cleri_object_t * help_list_pools = cleri_keyword(CLERI_GID_HELP_LIST_POOLS, "pools", CLERI_CASE_INSENSITIVE);
    cleri_object_t * help_list = cleri_sequence(
        CLERI_GID_HELP_LIST,
        2,
        k_list,
        cleri_optional(CLERI_NONE, cleri_choice(
            CLERI_NONE,
            CLERI_MOST_GREEDY,
            6,
            help_list_series,
            help_list_shards,
            help_list_groups,
            help_list_users,
            help_list_servers,
            help_list_pools
        ))
    );
    cleri_object_t * help_timezones = cleri_keyword(CLERI_GID_HELP_TIMEZONES, "timezones", CLERI_CASE_INSENSITIVE);
    cleri_object_t * help_drop_server = cleri_keyword(CLERI_GID_HELP_DROP_SERVER, "server", CLERI_CASE_INSENSITIVE);
    cleri_object_t * help_drop_group = cleri_keyword(CLERI_GID_HELP_DROP_GROUP, "group", CLERI_CASE_INSENSITIVE);
    cleri_object_t * help_drop_series = cleri_keyword(CLERI_GID_HELP_DROP_SERIES, "series", CLERI_CASE_INSENSITIVE);
    cleri_object_t * help_drop_shards = cleri_keyword(CLERI_GID_HELP_DROP_SHARDS, "shards", CLERI_CASE_INSENSITIVE);
    cleri_object_t * help_drop_user = cleri_keyword(CLERI_GID_HELP_DROP_USER, "user", CLERI_CASE_INSENSITIVE);
    cleri_object_t * help_drop = cleri_sequence(
        CLERI_GID_HELP_DROP,
        2,
        k_drop,
        cleri_optional(CLERI_NONE, cleri_choice(
            CLERI_NONE,
            CLERI_MOST_GREEDY,
            5,
            help_drop_server,
            help_drop_group,
            help_drop_series,
            help_drop_shards,
            help_drop_user
        ))
    );
    cleri_object_t * help_noaccess = cleri_keyword(CLERI_GID_HELP_NOACCESS, "noaccess", CLERI_CASE_INSENSITIVE);
    cleri_object_t * help_grant = cleri_keyword(CLERI_GID_HELP_GRANT, "grant", CLERI_CASE_INSENSITIVE);
    cleri_object_t * help = cleri_sequence(
        CLERI_GID_HELP,
        2,
        k_help,
        cleri_optional(CLERI_NONE, cleri_choice(
            CLERI_NONE,
            CLERI_MOST_GREEDY,
            14,
            help_select,
            help_alter,
            help_functions,
            help_create,
            help_count,
            help_show,
            help_revoke,
            help_access,
            help_timeit,
            help_list,
            help_timezones,
            help_drop,
            help_noaccess,
            help_grant
        ))
    );
    cleri_object_t * START = cleri_sequence(
        CLERI_GID_START,
        3,
        cleri_optional(CLERI_NONE, timeit_stmt),
        cleri_optional(CLERI_NONE, cleri_choice(
            CLERI_NONE,
            CLERI_FIRST_MATCH,
            11,
            select_stmt,
            list_stmt,
            count_stmt,
            alter_stmt,
            create_stmt,
            drop_stmt,
            grant_stmt,
            revoke_stmt,
            show_stmt,
            calc_stmt,
            help
        )),
        cleri_optional(CLERI_NONE, r_comment)
    );

    cleri_grammar_t * grammar = cleri_grammar(START, "^[a-z_]+");

    return grammar;
}
