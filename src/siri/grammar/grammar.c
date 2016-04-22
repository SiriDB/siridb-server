/*
 * This grammar is generated using the Grammar.export_c() method and
 * should be used with the cleri module.
 *
 * Source class: SiriGrammar
 * Created at: 2016-04-22 13:13:47
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
    cleri_object_t * r_integer = cleri_regex(CLERI_GID_R_INTEGER, "^[0-9]+");
    cleri_object_t * r_time_str = cleri_regex(CLERI_GID_R_TIME_STR, "^[0-9]+[smhdw]");
    cleri_object_t * r_singleq_str = cleri_regex(CLERI_GID_R_SINGLEQ_STR, "^(?:\'(?:[^\']*)\')+");
    cleri_object_t * r_doubleq_str = cleri_regex(CLERI_GID_R_DOUBLEQ_STR, "^(?:\"(?:[^\"]*)\")+");
    cleri_object_t * r_grave_str = cleri_regex(CLERI_GID_R_GRAVE_STR, "^(?:`(?:[^`]*)`)+");
    cleri_object_t * r_uuid_str = cleri_regex(CLERI_GID_R_UUID_STR, "^[0-9a-f]{8}\\-[0-9a-f]{4}\\-[0-9a-f]{4}\\-[0-9a-f]{4}\\-[0-9a-f]{12}");
    cleri_object_t * r_regex = cleri_regex(CLERI_GID_R_REGEX, "^(/[^/\\\\]*(?:\\\\.[^/\\\\]*)*/i?)");
    cleri_object_t * r_comment = cleri_regex(CLERI_GID_R_COMMENT, "^#.*");
    cleri_object_t * k_access = cleri_keyword(CLERI_GID_K_ACCESS, "access", CLERI_CASE_INSENSITIVE);
    cleri_object_t * k_address = cleri_keyword(CLERI_GID_K_ADDRESS, "address", CLERI_CASE_INSENSITIVE);
    cleri_object_t * k_after = cleri_keyword(CLERI_GID_K_AFTER, "after", CLERI_CASE_INSENSITIVE);
    cleri_object_t * k_alter = cleri_keyword(CLERI_GID_K_ALTER, "alter", CLERI_CASE_INSENSITIVE);
    cleri_object_t * k_and = cleri_keyword(CLERI_GID_K_AND, "and", CLERI_CASE_INSENSITIVE);
    cleri_object_t * k_as = cleri_keyword(CLERI_GID_K_AS, "as", CLERI_CASE_INSENSITIVE);
    cleri_object_t * k_before = cleri_keyword(CLERI_GID_K_BEFORE, "before", CLERI_CASE_INSENSITIVE);
    cleri_object_t * k_buffer_size = cleri_keyword(CLERI_GID_K_BUFFER_SIZE, "buffer_size", CLERI_CASE_INSENSITIVE);
    cleri_object_t * k_buffer_path = cleri_keyword(CLERI_GID_K_BUFFER_PATH, "buffer_path", CLERI_CASE_INSENSITIVE);
    cleri_object_t * k_between = cleri_keyword(CLERI_GID_K_BETWEEN, "between", CLERI_CASE_INSENSITIVE);
    cleri_object_t * k_cached = cleri_keyword(CLERI_GID_K_CACHED, "cached", CLERI_CASE_INSENSITIVE);
    cleri_object_t * k_comment = cleri_keyword(CLERI_GID_K_COMMENT, "comment", CLERI_CASE_INSENSITIVE);
    cleri_object_t * k_contains = cleri_keyword(CLERI_GID_K_CONTAINS, "contains", CLERI_CASE_INSENSITIVE);
    cleri_object_t * k_continue = cleri_keyword(CLERI_GID_K_CONTINUE, "continue", CLERI_CASE_INSENSITIVE);
    cleri_object_t * k_count = cleri_keyword(CLERI_GID_K_COUNT, "count", CLERI_CASE_INSENSITIVE);
    cleri_object_t * k_create = cleri_keyword(CLERI_GID_K_CREATE, "create", CLERI_CASE_INSENSITIVE);
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
    cleri_object_t * k_expression = cleri_keyword(CLERI_GID_K_EXPRESSION, "expression", CLERI_CASE_INSENSITIVE);
    cleri_object_t * k_false = cleri_keyword(CLERI_GID_K_FALSE, "false", CLERI_CASE_INSENSITIVE);
    cleri_object_t * k_filter = cleri_keyword(CLERI_GID_K_FILTER, "filter", CLERI_CASE_INSENSITIVE);
    cleri_object_t * k_first = cleri_keyword(CLERI_GID_K_FIRST, "first", CLERI_CASE_INSENSITIVE);
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
    cleri_object_t * k_ignore_threshold = cleri_keyword(CLERI_GID_K_IGNORE_THRESHOLD, "ignore_threshold", CLERI_CASE_INSENSITIVE);
    cleri_object_t * k_indexed = cleri_keyword(CLERI_GID_K_INDEXED, "indexed", CLERI_CASE_INSENSITIVE);
    cleri_object_t * k_insert = cleri_keyword(CLERI_GID_K_INSERT, "insert", CLERI_CASE_INSENSITIVE);
    cleri_object_t * k_intersection = cleri_choice(
        CLERI_GID_K_INTERSECTION,
        CLERI_FIRST_MATCH,
        2,
        cleri_token(CLERI_NONE, "&"),
        cleri_keyword(CLERI_NONE, "intersection", CLERI_CASE_INSENSITIVE)
    );
    cleri_object_t * k_length = cleri_keyword(CLERI_GID_K_LENGTH, "length", CLERI_CASE_INSENSITIVE);
    cleri_object_t * k_last = cleri_keyword(CLERI_GID_K_LAST, "last", CLERI_CASE_INSENSITIVE);
    cleri_object_t * k_libuv = cleri_keyword(CLERI_GID_K_LIBUV, "libuv", CLERI_CASE_INSENSITIVE);
    cleri_object_t * k_license = cleri_keyword(CLERI_GID_K_LICENSE, "license", CLERI_CASE_INSENSITIVE);
    cleri_object_t * k_limit = cleri_keyword(CLERI_GID_K_LIMIT, "limit", CLERI_CASE_INSENSITIVE);
    cleri_object_t * k_list = cleri_keyword(CLERI_GID_K_LIST, "list", CLERI_CASE_INSENSITIVE);
    cleri_object_t * k_log_level = cleri_keyword(CLERI_GID_K_LOG_LEVEL, "log_level", CLERI_CASE_INSENSITIVE);
    cleri_object_t * k_manhole = cleri_keyword(CLERI_GID_K_MANHOLE, "manhole", CLERI_CASE_INSENSITIVE);
    cleri_object_t * k_max = cleri_keyword(CLERI_GID_K_MAX, "max", CLERI_CASE_INSENSITIVE);
    cleri_object_t * k_max_cache_expressions = cleri_keyword(CLERI_GID_K_MAX_CACHE_EXPRESSIONS, "max_cache_expressions", CLERI_CASE_INSENSITIVE);
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
    cleri_object_t * k_network = cleri_keyword(CLERI_GID_K_NETWORK, "network", CLERI_CASE_INSENSITIVE);
    cleri_object_t * k_networks = cleri_keyword(CLERI_GID_K_NETWORKS, "networks", CLERI_CASE_INSENSITIVE);
    cleri_object_t * k_now = cleri_keyword(CLERI_GID_K_NOW, "now", CLERI_CASE_INSENSITIVE);
    cleri_object_t * k_null = cleri_keyword(CLERI_GID_K_NULL, "null", CLERI_CASE_INSENSITIVE);
    cleri_object_t * k_online = cleri_keyword(CLERI_GID_K_ONLINE, "online", CLERI_CASE_INSENSITIVE);
    cleri_object_t * k_open_files = cleri_keyword(CLERI_GID_K_OPEN_FILES, "open_files", CLERI_CASE_INSENSITIVE);
    cleri_object_t * k_optimize_pool = cleri_keyword(CLERI_GID_K_OPTIMIZE_POOL, "optimize_pool", CLERI_CASE_INSENSITIVE);
    cleri_object_t * k_optimize_task = cleri_keyword(CLERI_GID_K_OPTIMIZE_TASK, "optimize_task", CLERI_CASE_INSENSITIVE);
    cleri_object_t * k_or = cleri_keyword(CLERI_GID_K_OR, "or", CLERI_CASE_INSENSITIVE);
    cleri_object_t * k_password = cleri_keyword(CLERI_GID_K_PASSWORD, "password", CLERI_CASE_INSENSITIVE);
    cleri_object_t * k_pause = cleri_keyword(CLERI_GID_K_PAUSE, "pause", CLERI_CASE_INSENSITIVE);
    cleri_object_t * k_points = cleri_choice(
        CLERI_GID_K_POINTS,
        CLERI_FIRST_MATCH,
        2,
        cleri_token(CLERI_NONE, "*"),
        cleri_keyword(CLERI_NONE, "points", CLERI_CASE_INSENSITIVE)
    );
    cleri_object_t * k_pool = cleri_keyword(CLERI_GID_K_POOL, "pool", CLERI_CASE_INSENSITIVE);
    cleri_object_t * k_pools = cleri_keyword(CLERI_GID_K_POOLS, "pools", CLERI_CASE_INSENSITIVE);
    cleri_object_t * k_port = cleri_keyword(CLERI_GID_K_PORT, "port", CLERI_CASE_INSENSITIVE);
    cleri_object_t * k_prefix = cleri_keyword(CLERI_GID_K_PREFIX, "prefix", CLERI_CASE_INSENSITIVE);
    cleri_object_t * k_pvariance = cleri_keyword(CLERI_GID_K_PVARIANCE, "pvariance", CLERI_CASE_INSENSITIVE);
    cleri_object_t * k_query_timeout = cleri_keyword(CLERI_GID_K_QUERY_TIMEOUT, "query_timeout", CLERI_CASE_INSENSITIVE);
    cleri_object_t * k_read = cleri_keyword(CLERI_GID_K_READ, "read", CLERI_CASE_INSENSITIVE);
    cleri_object_t * k_received_points = cleri_keyword(CLERI_GID_K_RECEIVED_POINTS, "received_points", CLERI_CASE_INSENSITIVE);
    cleri_object_t * k_reindex_progress = cleri_keyword(CLERI_GID_K_REINDEX_PROGRESS, "reindex_progress", CLERI_CASE_INSENSITIVE);
    cleri_object_t * k_reject = cleri_keyword(CLERI_GID_K_REJECT, "reject", CLERI_CASE_INSENSITIVE);
    cleri_object_t * k_revoke = cleri_keyword(CLERI_GID_K_REVOKE, "revoke", CLERI_CASE_INSENSITIVE);
    cleri_object_t * k_select = cleri_keyword(CLERI_GID_K_SELECT, "select", CLERI_CASE_INSENSITIVE);
    cleri_object_t * k_series = cleri_keyword(CLERI_GID_K_SERIES, "series", CLERI_CASE_INSENSITIVE);
    cleri_object_t * k_server = cleri_keyword(CLERI_GID_K_SERVER, "server", CLERI_CASE_INSENSITIVE);
    cleri_object_t * k_servers = cleri_keyword(CLERI_GID_K_SERVERS, "servers", CLERI_CASE_INSENSITIVE);
    cleri_object_t * k_set = cleri_keyword(CLERI_GID_K_SET, "set", CLERI_CASE_INSENSITIVE);
    cleri_object_t * k_shard = cleri_keyword(CLERI_GID_K_SHARD, "shard", CLERI_CASE_INSENSITIVE);
    cleri_object_t * k_sharding_buffering = cleri_keyword(CLERI_GID_K_SHARDING_BUFFERING, "sharding_buffering", CLERI_CASE_INSENSITIVE);
    cleri_object_t * k_sharding_max_chunk_points = cleri_keyword(CLERI_GID_K_SHARDING_MAX_CHUNK_POINTS, "sharding_max_chunk_points", CLERI_CASE_INSENSITIVE);
    cleri_object_t * k_shards = cleri_keyword(CLERI_GID_K_SHARDS, "shards", CLERI_CASE_INSENSITIVE);
    cleri_object_t * k_show = cleri_keyword(CLERI_GID_K_SHOW, "show", CLERI_CASE_INSENSITIVE);
    cleri_object_t * k_sid = cleri_keyword(CLERI_GID_K_SID, "sid", CLERI_CASE_INSENSITIVE);
    cleri_object_t * k_size = cleri_keyword(CLERI_GID_K_SIZE, "size", CLERI_CASE_INSENSITIVE);
    cleri_object_t * k_start = cleri_keyword(CLERI_GID_K_START, "start", CLERI_CASE_INSENSITIVE);
    cleri_object_t * k_startup_time = cleri_keyword(CLERI_GID_K_STARTUP_TIME, "startup_time", CLERI_CASE_INSENSITIVE);
    cleri_object_t * k_status = cleri_keyword(CLERI_GID_K_STATUS, "status", CLERI_CASE_INSENSITIVE);
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
    cleri_object_t * k_task_queue = cleri_keyword(CLERI_GID_K_TASK_QUEUE, "task_queue", CLERI_CASE_INSENSITIVE);
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
        16,
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
        k_alter,
        k_pause,
        k_continue
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
        7,
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
        ),
        cleri_sequence(
            CLERI_NONE,
            2,
            cleri_token(CLERI_NONE, "-"),
            r_integer
        )
    );
    cleri_object_t * series_props = cleri_choice(
        CLERI_GID_SERIES_PROPS,
        CLERI_FIRST_MATCH,
        8,
        k_name,
        k_type,
        k_length,
        k_start,
        k_end,
        k_first,
        k_last,
        k_pool
    );
    cleri_object_t * series_columns = cleri_list(CLERI_GID_SERIES_COLUMNS, series_props, cleri_token(CLERI_NONE, ","), 1, 0, 0);
    cleri_object_t * shard_props = cleri_choice(
        CLERI_GID_SHARD_PROPS,
        CLERI_FIRST_MATCH,
        5,
        k_sid,
        k_size,
        k_start,
        k_end,
        k_type
    );
    cleri_object_t * shard_columns = cleri_list(CLERI_GID_SHARD_COLUMNS, shard_props, cleri_token(CLERI_NONE, ","), 1, 0, 0);
    cleri_object_t * server_props = cleri_choice(
        CLERI_GID_SERVER_PROPS,
        CLERI_FIRST_MATCH,
        27,
        k_address,
        k_name,
        k_port,
        k_uuid,
        k_pool,
        k_version,
        k_online,
        k_status,
        k_buffer_path,
        k_buffer_size,
        k_dbpath,
        k_debug,
        k_log_level,
        k_manhole,
        k_max_open_files,
        k_mem_usage,
        k_open_files,
        k_optimize_pool,
        k_optimize_task,
        k_received_points,
        k_reindex_progress,
        k_sharding_buffering,
        k_sharding_max_chunk_points,
        k_startup_time,
        k_sync_progress,
        k_task_queue,
        k_uptime
    );
    cleri_object_t * server_columns = cleri_list(CLERI_GID_SERVER_COLUMNS, server_props, cleri_token(CLERI_NONE, ","), 1, 0, 0);
    cleri_object_t * group_props = cleri_choice(
        CLERI_GID_GROUP_PROPS,
        CLERI_FIRST_MATCH,
        5,
        k_cached,
        k_expression,
        k_indexed,
        k_name,
        k_series
    );
    cleri_object_t * group_columns = cleri_list(CLERI_GID_GROUP_COLUMNS, group_props, cleri_token(CLERI_NONE, ","), 1, 0, 0);
    cleri_object_t * user_props = cleri_choice(
        CLERI_GID_USER_PROPS,
        CLERI_FIRST_MATCH,
        2,
        k_user,
        k_access
    );
    cleri_object_t * user_columns = cleri_list(CLERI_GID_USER_COLUMNS, user_props, cleri_token(CLERI_NONE, ","), 1, 0, 0);
    cleri_object_t * network_props = cleri_choice(
        CLERI_GID_NETWORK_PROPS,
        CLERI_FIRST_MATCH,
        3,
        k_network,
        k_access,
        k_comment
    );
    cleri_object_t * network_columns = cleri_list(CLERI_GID_NETWORK_COLUMNS, network_props, cleri_token(CLERI_NONE, ","), 1, 0, 0);
    cleri_object_t * pool_props = cleri_choice(
        CLERI_GID_POOL_PROPS,
        CLERI_FIRST_MATCH,
        3,
        k_pool,
        k_servers,
        k_series
    );
    cleri_object_t * pool_columns = cleri_list(CLERI_GID_POOL_COLUMNS, pool_props, cleri_token(CLERI_NONE, ","), 1, 0, 0);
    cleri_object_t * where_operator = cleri_choice(
        CLERI_GID_WHERE_OPERATOR,
        CLERI_FIRST_MATCH,
        3,
        cleri_tokens(CLERI_NONE, "== != <= >= < >"),
        cleri_keyword(CLERI_NONE, "in", CLERI_CASE_INSENSITIVE),
        cleri_sequence(
            CLERI_NONE,
            2,
            cleri_keyword(CLERI_NONE, "not", CLERI_CASE_INSENSITIVE),
            cleri_keyword(CLERI_NONE, "in", CLERI_CASE_INSENSITIVE)
        )
    );
    cleri_object_t * where_series_opts = cleri_choice(
        CLERI_GID_WHERE_SERIES_OPTS,
        CLERI_FIRST_MATCH,
        7,
        string,
        int_expr,
        time_expr,
        k_false,
        k_true,
        k_null,
        series_props
    );
    cleri_object_t * where_series_stmt = cleri_sequence(
        CLERI_GID_WHERE_SERIES_STMT,
        2,
        k_where,
        cleri_prio(
            CLERI_NONE,
            4,
            cleri_sequence(
                CLERI_NONE,
                3,
                where_series_opts,
                where_operator,
                where_series_opts
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
    cleri_object_t * where_group_opts = cleri_choice(
        CLERI_GID_WHERE_GROUP_OPTS,
        CLERI_FIRST_MATCH,
        7,
        string,
        int_expr,
        time_expr,
        k_false,
        k_true,
        k_null,
        group_props
    );
    cleri_object_t * where_group_stmt = cleri_sequence(
        CLERI_GID_WHERE_GROUP_STMT,
        2,
        k_where,
        cleri_prio(
            CLERI_NONE,
            4,
            cleri_sequence(
                CLERI_NONE,
                3,
                where_group_opts,
                where_operator,
                where_group_opts
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
    cleri_object_t * where_pool_opts = cleri_choice(
        CLERI_GID_WHERE_POOL_OPTS,
        CLERI_FIRST_MATCH,
        7,
        string,
        int_expr,
        time_expr,
        k_false,
        k_true,
        k_null,
        pool_props
    );
    cleri_object_t * where_pool_stmt = cleri_sequence(
        CLERI_GID_WHERE_POOL_STMT,
        2,
        k_where,
        cleri_prio(
            CLERI_NONE,
            4,
            cleri_sequence(
                CLERI_NONE,
                3,
                where_pool_opts,
                where_operator,
                where_pool_opts
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
    cleri_object_t * where_server_opts = cleri_choice(
        CLERI_GID_WHERE_SERVER_OPTS,
        CLERI_FIRST_MATCH,
        7,
        string,
        int_expr,
        time_expr,
        k_false,
        k_true,
        k_null,
        server_props
    );
    cleri_object_t * where_server_stmt = cleri_sequence(
        CLERI_GID_WHERE_SERVER_STMT,
        2,
        k_where,
        cleri_prio(
            CLERI_NONE,
            4,
            cleri_sequence(
                CLERI_NONE,
                3,
                where_server_opts,
                where_operator,
                where_server_opts
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
    cleri_object_t * where_user_opts = cleri_choice(
        CLERI_GID_WHERE_USER_OPTS,
        CLERI_FIRST_MATCH,
        7,
        string,
        int_expr,
        time_expr,
        k_false,
        k_true,
        k_null,
        user_props
    );
    cleri_object_t * where_user_stmt = cleri_sequence(
        CLERI_GID_WHERE_USER_STMT,
        2,
        k_where,
        cleri_prio(
            CLERI_NONE,
            4,
            cleri_sequence(
                CLERI_NONE,
                3,
                where_user_opts,
                where_operator,
                where_user_opts
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
    cleri_object_t * where_network_opts = cleri_choice(
        CLERI_GID_WHERE_NETWORK_OPTS,
        CLERI_FIRST_MATCH,
        7,
        string,
        int_expr,
        time_expr,
        k_false,
        k_true,
        k_null,
        network_props
    );
    cleri_object_t * where_network_stmt = cleri_sequence(
        CLERI_GID_WHERE_NETWORK_STMT,
        2,
        k_where,
        cleri_prio(
            CLERI_NONE,
            4,
            cleri_sequence(
                CLERI_NONE,
                3,
                where_network_opts,
                where_operator,
                where_network_opts
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
    cleri_object_t * where_shard_opts = cleri_choice(
        CLERI_GID_WHERE_SHARD_OPTS,
        CLERI_FIRST_MATCH,
        7,
        string,
        int_expr,
        time_expr,
        k_false,
        k_true,
        k_null,
        shard_props
    );
    cleri_object_t * where_shard_stmt = cleri_sequence(
        CLERI_GID_WHERE_SHARD_STMT,
        2,
        k_where,
        cleri_prio(
            CLERI_NONE,
            4,
            cleri_sequence(
                CLERI_NONE,
                3,
                where_shard_opts,
                where_operator,
                where_shard_opts
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
    cleri_object_t * delimiter_match = cleri_choice(
        CLERI_GID_DELIMITER_MATCH,
        CLERI_FIRST_MATCH,
        4,
        k_union,
        c_difference,
        k_intersection,
        k_symmetric_difference
    );
    cleri_object_t * series_name = cleri_repeat(CLERI_GID_SERIES_NAME, string, 1, 1);
    cleri_object_t * group_name = cleri_repeat(CLERI_GID_GROUP_NAME, r_grave_str, 1, 1);
    cleri_object_t * _boolean = cleri_choice(
        CLERI_GID__BOOLEAN,
        CLERI_FIRST_MATCH,
        2,
        k_true,
        k_false
    );
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
    ), delimiter_match, 1, 0, 0);
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
    cleri_object_t * difference_expr = cleri_sequence(
        CLERI_GID_DIFFERENCE_EXPR,
        4,
        k_difference,
        cleri_token(CLERI_NONE, "("),
        cleri_optional(CLERI_NONE, time_expr),
        cleri_token(CLERI_NONE, ")")
    );
    cleri_object_t * derivative_expr = cleri_sequence(
        CLERI_GID_DERIVATIVE_EXPR,
        4,
        k_derivative,
        cleri_token(CLERI_NONE, "("),
        cleri_list(CLERI_NONE, time_expr, cleri_token(CLERI_NONE, ","), 0, 2, 0),
        cleri_token(CLERI_NONE, ")")
    );
    cleri_object_t * mean_expr = cleri_sequence(
        CLERI_GID_MEAN_EXPR,
        4,
        k_mean,
        cleri_token(CLERI_NONE, "("),
        time_expr,
        cleri_token(CLERI_NONE, ")")
    );
    cleri_object_t * median_expr = cleri_sequence(
        CLERI_GID_MEDIAN_EXPR,
        4,
        k_median,
        cleri_token(CLERI_NONE, "("),
        time_expr,
        cleri_token(CLERI_NONE, ")")
    );
    cleri_object_t * median_low_expr = cleri_sequence(
        CLERI_GID_MEDIAN_LOW_EXPR,
        4,
        k_median_low,
        cleri_token(CLERI_NONE, "("),
        time_expr,
        cleri_token(CLERI_NONE, ")")
    );
    cleri_object_t * median_high_expr = cleri_sequence(
        CLERI_GID_MEDIAN_HIGH_EXPR,
        4,
        k_median_high,
        cleri_token(CLERI_NONE, "("),
        time_expr,
        cleri_token(CLERI_NONE, ")")
    );
    cleri_object_t * sum_expr = cleri_sequence(
        CLERI_GID_SUM_EXPR,
        4,
        k_sum,
        cleri_token(CLERI_NONE, "("),
        time_expr,
        cleri_token(CLERI_NONE, ")")
    );
    cleri_object_t * min_expr = cleri_sequence(
        CLERI_GID_MIN_EXPR,
        4,
        k_min,
        cleri_token(CLERI_NONE, "("),
        time_expr,
        cleri_token(CLERI_NONE, ")")
    );
    cleri_object_t * max_expr = cleri_sequence(
        CLERI_GID_MAX_EXPR,
        4,
        k_max,
        cleri_token(CLERI_NONE, "("),
        time_expr,
        cleri_token(CLERI_NONE, ")")
    );
    cleri_object_t * count_expr = cleri_sequence(
        CLERI_GID_COUNT_EXPR,
        4,
        k_count,
        cleri_token(CLERI_NONE, "("),
        time_expr,
        cleri_token(CLERI_NONE, ")")
    );
    cleri_object_t * variance_expr = cleri_sequence(
        CLERI_GID_VARIANCE_EXPR,
        4,
        k_variance,
        cleri_token(CLERI_NONE, "("),
        time_expr,
        cleri_token(CLERI_NONE, ")")
    );
    cleri_object_t * pvariance_expr = cleri_sequence(
        CLERI_GID_PVARIANCE_EXPR,
        4,
        k_pvariance,
        cleri_token(CLERI_NONE, "("),
        time_expr,
        cleri_token(CLERI_NONE, ")")
    );
    cleri_object_t * filter_expr = cleri_sequence(
        CLERI_GID_FILTER_EXPR,
        5,
        k_filter,
        cleri_token(CLERI_NONE, "("),
        cleri_optional(CLERI_NONE, cleri_choice(
            CLERI_NONE,
            CLERI_FIRST_MATCH,
            2,
            k_contains,
            cleri_tokens(CLERI_NONE, "== != <= >= < >")
        )),
        cleri_choice(
            CLERI_NONE,
            CLERI_FIRST_MATCH,
            3,
            string,
            r_float,
            r_integer
        ),
        cleri_token(CLERI_NONE, ")")
    );
    cleri_object_t * reject_expr = cleri_sequence(
        CLERI_GID_REJECT_EXPR,
        5,
        k_reject,
        cleri_token(CLERI_NONE, "("),
        cleri_optional(CLERI_NONE, cleri_choice(
            CLERI_NONE,
            CLERI_MOST_GREEDY,
            2,
            k_contains,
            cleri_tokens(CLERI_NONE, "== != <= >= < >")
        )),
        cleri_choice(
            CLERI_NONE,
            CLERI_MOST_GREEDY,
            3,
            string,
            r_float,
            r_integer
        ),
        cleri_token(CLERI_NONE, ")")
    );
    cleri_object_t * aggregate_functions = cleri_list(CLERI_GID_AGGREGATE_FUNCTIONS, cleri_choice(
        CLERI_NONE,
        CLERI_FIRST_MATCH,
        14,
        mean_expr,
        sum_expr,
        median_expr,
        median_low_expr,
        median_high_expr,
        min_expr,
        max_expr,
        count_expr,
        variance_expr,
        pvariance_expr,
        difference_expr,
        derivative_expr,
        filter_expr,
        reject_expr
    ), cleri_token(CLERI_NONE, "=>"), 1, 0, 0);
    cleri_object_t * select_aggregate_expr = cleri_sequence(
        CLERI_GID_SELECT_AGGREGATE_EXPR,
        3,
        cleri_choice(
            CLERI_NONE,
            CLERI_FIRST_MATCH,
            2,
            k_points,
            aggregate_functions
        ),
        cleri_optional(CLERI_NONE, prefix_expr),
        cleri_optional(CLERI_NONE, suffix_expr)
    );
    cleri_object_t * merge_expr = cleri_sequence(
        CLERI_GID_MERGE_EXPR,
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
    cleri_object_t * set_comment_expr = cleri_sequence(
        CLERI_GID_SET_COMMENT_EXPR,
        3,
        k_set,
        k_comment,
        string
    );
    cleri_object_t * set_password_expr = cleri_sequence(
        CLERI_GID_SET_PASSWORD_EXPR,
        3,
        k_set,
        k_password,
        string
    );
    cleri_object_t * set_indexed_expr = cleri_sequence(
        CLERI_GID_SET_INDEXED_EXPR,
        3,
        k_set,
        k_indexed,
        _boolean
    );
    cleri_object_t * set_address_expr = cleri_sequence(
        CLERI_GID_SET_ADDRESS_EXPR,
        3,
        k_set,
        k_address,
        string
    );
    cleri_object_t * set_port_expr = cleri_sequence(
        CLERI_GID_SET_PORT_EXPR,
        3,
        k_set,
        k_port,
        r_integer
    );
    cleri_object_t * set_license_expr = cleri_sequence(
        CLERI_GID_SET_LICENSE_EXPR,
        3,
        k_set,
        k_license,
        string
    );
    cleri_object_t * set_debug_expr = cleri_sequence(
        CLERI_GID_SET_DEBUG_EXPR,
        3,
        k_set,
        k_debug,
        _boolean
    );
    cleri_object_t * set_log_level_expr = cleri_sequence(
        CLERI_GID_SET_LOG_LEVEL_EXPR,
        3,
        k_set,
        k_log_level,
        string
    );
    cleri_object_t * set_series_name_expr = cleri_sequence(
        CLERI_GID_SET_SERIES_NAME_EXPR,
        3,
        k_set,
        k_name,
        string
    );
    cleri_object_t * set_ignore_threshold_expr = cleri_sequence(
        CLERI_GID_SET_IGNORE_THRESHOLD_EXPR,
        3,
        k_set,
        k_ignore_threshold,
        _boolean
    );
    cleri_object_t * set_drop_threshold_expr = cleri_sequence(
        CLERI_GID_SET_DROP_THRESHOLD_EXPR,
        3,
        k_set,
        k_drop_threshold,
        r_float
    );
    cleri_object_t * set_query_timeout_expr = cleri_sequence(
        CLERI_GID_SET_QUERY_TIMEOUT_EXPR,
        3,
        k_set,
        k_query_timeout,
        r_float
    );
    cleri_object_t * set_timezone_expr = cleri_sequence(
        CLERI_GID_SET_TIMEZONE_EXPR,
        3,
        k_set,
        k_timezone,
        string
    );
    cleri_object_t * set_max_cache_expressions_expr = cleri_sequence(
        CLERI_GID_SET_MAX_CACHE_EXPRESSIONS_EXPR,
        3,
        k_set,
        k_max_cache_expressions,
        r_integer
    );
    cleri_object_t * set_max_open_files_expr = cleri_sequence(
        CLERI_GID_SET_MAX_OPEN_FILES_EXPR,
        3,
        k_set,
        k_max_open_files,
        r_integer
    );
    cleri_object_t * set_expression_expr = cleri_sequence(
        CLERI_GID_SET_EXPRESSION_EXPR,
        3,
        k_set,
        k_expression,
        r_regex
    );
    cleri_object_t * for_regex_expr = cleri_sequence(
        CLERI_GID_FOR_REGEX_EXPR,
        2,
        k_for,
        r_regex
    );
    cleri_object_t * alter_user_stmt = cleri_sequence(
        CLERI_GID_ALTER_USER_STMT,
        3,
        k_user,
        string,
        set_password_expr
    );
    cleri_object_t * alter_network_stmt = cleri_sequence(
        CLERI_GID_ALTER_NETWORK_STMT,
        3,
        k_network,
        string,
        set_comment_expr
    );
    cleri_object_t * alter_group_stmt = cleri_sequence(
        CLERI_GID_ALTER_GROUP_STMT,
        3,
        k_group,
        group_name,
        cleri_choice(
            CLERI_NONE,
            CLERI_FIRST_MATCH,
            2,
            set_expression_expr,
            set_indexed_expr
        )
    );
    cleri_object_t * alter_server_stmt = cleri_sequence(
        CLERI_GID_ALTER_SERVER_STMT,
        3,
        k_server,
        uuid,
        cleri_choice(
            CLERI_NONE,
            CLERI_FIRST_MATCH,
            4,
            set_address_expr,
            set_port_expr,
            set_debug_expr,
            set_log_level_expr
        )
    );
    cleri_object_t * alter_database_stmt = cleri_sequence(
        CLERI_GID_ALTER_DATABASE_STMT,
        2,
        k_database,
        cleri_choice(
            CLERI_NONE,
            CLERI_FIRST_MATCH,
            6,
            set_license_expr,
            set_drop_threshold_expr,
            set_query_timeout_expr,
            set_timezone_expr,
            set_max_cache_expressions_expr,
            set_max_open_files_expr
        )
    );
    cleri_object_t * alter_series_stmt = cleri_sequence(
        CLERI_GID_ALTER_SERIES_STMT,
        3,
        k_series,
        series_name,
        set_series_name_expr
    );
    cleri_object_t * count_groups_stmt = cleri_sequence(
        CLERI_GID_COUNT_GROUPS_STMT,
        2,
        k_groups,
        cleri_optional(CLERI_NONE, where_group_stmt)
    );
    cleri_object_t * count_groups_props_stmt = cleri_sequence(
        CLERI_GID_COUNT_GROUPS_PROPS_STMT,
        3,
        k_groups,
        k_series,
        cleri_optional(CLERI_NONE, where_group_stmt)
    );
    cleri_object_t * count_networks_stmt = cleri_sequence(
        CLERI_GID_COUNT_NETWORKS_STMT,
        2,
        k_networks,
        cleri_optional(CLERI_NONE, where_network_stmt)
    );
    cleri_object_t * count_pools_stmt = cleri_sequence(
        CLERI_GID_COUNT_POOLS_STMT,
        2,
        k_pools,
        cleri_optional(CLERI_NONE, where_pool_stmt)
    );
    cleri_object_t * count_pools_props_stmt = cleri_sequence(
        CLERI_GID_COUNT_POOLS_PROPS_STMT,
        3,
        k_pools,
        cleri_choice(
            CLERI_NONE,
            CLERI_FIRST_MATCH,
            2,
            k_series,
            k_servers
        ),
        cleri_optional(CLERI_NONE, where_pool_stmt)
    );
    cleri_object_t * count_series_stmt = cleri_sequence(
        CLERI_GID_COUNT_SERIES_STMT,
        3,
        k_series,
        cleri_optional(CLERI_NONE, series_match),
        cleri_optional(CLERI_NONE, where_series_stmt)
    );
    cleri_object_t * count_servers_stmt = cleri_sequence(
        CLERI_GID_COUNT_SERVERS_STMT,
        2,
        k_servers,
        cleri_optional(CLERI_NONE, where_server_stmt)
    );
    cleri_object_t * count_servers_props_stmt = cleri_sequence(
        CLERI_GID_COUNT_SERVERS_PROPS_STMT,
        3,
        k_servers,
        cleri_choice(
            CLERI_NONE,
            CLERI_FIRST_MATCH,
            3,
            k_received_points,
            k_mem_usage,
            k_open_files
        ),
        cleri_optional(CLERI_NONE, where_server_stmt)
    );
    cleri_object_t * count_shards_stmt = cleri_sequence(
        CLERI_GID_COUNT_SHARDS_STMT,
        2,
        k_shards,
        cleri_optional(CLERI_NONE, where_shard_stmt)
    );
    cleri_object_t * count_shards_props_stmt = cleri_sequence(
        CLERI_GID_COUNT_SHARDS_PROPS_STMT,
        3,
        k_shards,
        k_size,
        cleri_optional(CLERI_NONE, where_shard_stmt)
    );
    cleri_object_t * count_users_stmt = cleri_sequence(
        CLERI_GID_COUNT_USERS_STMT,
        2,
        k_users,
        cleri_optional(CLERI_NONE, where_user_stmt)
    );
    cleri_object_t * count_series_length_stmt = cleri_sequence(
        CLERI_GID_COUNT_SERIES_LENGTH_STMT,
        4,
        k_series,
        k_length,
        cleri_optional(CLERI_NONE, series_match),
        cleri_optional(CLERI_NONE, where_series_stmt)
    );
    cleri_object_t * create_group_stmt = cleri_sequence(
        CLERI_GID_CREATE_GROUP_STMT,
        4,
        k_group,
        group_name,
        for_regex_expr,
        cleri_optional(CLERI_NONE, set_indexed_expr)
    );
    cleri_object_t * create_user_stmt = cleri_sequence(
        CLERI_GID_CREATE_USER_STMT,
        3,
        k_user,
        string,
        set_password_expr
    );
    cleri_object_t * create_network_stmt = cleri_sequence(
        CLERI_GID_CREATE_NETWORK_STMT,
        3,
        k_network,
        string,
        cleri_optional(CLERI_NONE, set_comment_expr)
    );
    cleri_object_t * drop_group_stmt = cleri_sequence(
        CLERI_GID_DROP_GROUP_STMT,
        2,
        k_group,
        group_name
    );
    cleri_object_t * drop_series_stmt = cleri_sequence(
        CLERI_GID_DROP_SERIES_STMT,
        3,
        k_series,
        cleri_choice(
            CLERI_NONE,
            CLERI_MOST_GREEDY,
            3,
            series_match,
            where_series_stmt,
            cleri_sequence(
                CLERI_NONE,
                2,
                series_match,
                where_series_stmt
            )
        ),
        cleri_optional(CLERI_NONE, set_ignore_threshold_expr)
    );
    cleri_object_t * drop_shard_stmt = cleri_sequence(
        CLERI_GID_DROP_SHARD_STMT,
        2,
        k_shard,
        r_integer
    );
    cleri_object_t * drop_shards_stmt = cleri_sequence(
        CLERI_GID_DROP_SHARDS_STMT,
        3,
        k_shards,
        cleri_optional(CLERI_NONE, where_shard_stmt),
        cleri_optional(CLERI_NONE, set_ignore_threshold_expr)
    );
    cleri_object_t * drop_server_stmt = cleri_sequence(
        CLERI_GID_DROP_SERVER_STMT,
        2,
        k_server,
        uuid
    );
    cleri_object_t * drop_user_stmt = cleri_sequence(
        CLERI_GID_DROP_USER_STMT,
        2,
        k_user,
        string
    );
    cleri_object_t * drop_network_stmt = cleri_sequence(
        CLERI_GID_DROP_NETWORK_STMT,
        2,
        k_network,
        string
    );
    cleri_object_t * grant_user_stmt = cleri_sequence(
        CLERI_GID_GRANT_USER_STMT,
        3,
        k_user,
        string,
        cleri_optional(CLERI_NONE, set_password_expr)
    );
    cleri_object_t * grant_network_stmt = cleri_sequence(
        CLERI_GID_GRANT_NETWORK_STMT,
        3,
        k_network,
        string,
        cleri_optional(CLERI_NONE, set_comment_expr)
    );
    cleri_object_t * list_groups_stmt = cleri_sequence(
        CLERI_GID_LIST_GROUPS_STMT,
        3,
        k_groups,
        cleri_optional(CLERI_NONE, group_columns),
        cleri_optional(CLERI_NONE, where_group_stmt)
    );
    cleri_object_t * list_networks_stmt = cleri_sequence(
        CLERI_GID_LIST_NETWORKS_STMT,
        3,
        k_networks,
        cleri_optional(CLERI_NONE, network_columns),
        cleri_optional(CLERI_NONE, where_network_stmt)
    );
    cleri_object_t * list_pools_stmt = cleri_sequence(
        CLERI_GID_LIST_POOLS_STMT,
        3,
        k_pools,
        cleri_optional(CLERI_NONE, pool_columns),
        cleri_optional(CLERI_NONE, where_pool_stmt)
    );
    cleri_object_t * list_series_stmt = cleri_sequence(
        CLERI_GID_LIST_SERIES_STMT,
        4,
        k_series,
        cleri_optional(CLERI_NONE, series_columns),
        cleri_optional(CLERI_NONE, series_match),
        cleri_optional(CLERI_NONE, where_series_stmt)
    );
    cleri_object_t * list_servers_stmt = cleri_sequence(
        CLERI_GID_LIST_SERVERS_STMT,
        3,
        k_servers,
        cleri_optional(CLERI_NONE, server_columns),
        cleri_optional(CLERI_NONE, where_server_stmt)
    );
    cleri_object_t * list_shards_stmt = cleri_sequence(
        CLERI_GID_LIST_SHARDS_STMT,
        3,
        k_shards,
        cleri_optional(CLERI_NONE, shard_columns),
        cleri_optional(CLERI_NONE, where_shard_stmt)
    );
    cleri_object_t * list_users_stmt = cleri_sequence(
        CLERI_GID_LIST_USERS_STMT,
        3,
        k_users,
        cleri_optional(CLERI_NONE, user_columns),
        cleri_optional(CLERI_NONE, where_user_stmt)
    );
    cleri_object_t * revoke_user_stmt = cleri_sequence(
        CLERI_GID_REVOKE_USER_STMT,
        2,
        k_user,
        string
    );
    cleri_object_t * revoke_network_stmt = cleri_sequence(
        CLERI_GID_REVOKE_NETWORK_STMT,
        2,
        k_network,
        string
    );
    cleri_object_t * alter_stmt = cleri_sequence(
        CLERI_GID_ALTER_STMT,
        2,
        k_alter,
        cleri_choice(
            CLERI_NONE,
            CLERI_FIRST_MATCH,
            6,
            alter_user_stmt,
            alter_network_stmt,
            alter_group_stmt,
            alter_server_stmt,
            alter_database_stmt,
            alter_series_stmt
        )
    );
    cleri_object_t * calc_stmt = cleri_repeat(CLERI_GID_CALC_STMT, time_expr, 1, 1);
    cleri_object_t * continue_stmt = cleri_sequence(
        CLERI_GID_CONTINUE_STMT,
        2,
        k_continue,
        uuid
    );
    cleri_object_t * count_stmt = cleri_sequence(
        CLERI_GID_COUNT_STMT,
        2,
        k_count,
        cleri_choice(
            CLERI_NONE,
            CLERI_FIRST_MATCH,
            12,
            count_groups_stmt,
            count_groups_props_stmt,
            count_networks_stmt,
            count_pools_stmt,
            count_pools_props_stmt,
            count_series_stmt,
            count_servers_stmt,
            count_servers_props_stmt,
            count_shards_stmt,
            count_shards_props_stmt,
            count_users_stmt,
            count_series_length_stmt
        )
    );
    cleri_object_t * create_stmt = cleri_sequence(
        CLERI_GID_CREATE_STMT,
        2,
        k_create,
        cleri_choice(
            CLERI_NONE,
            CLERI_MOST_GREEDY,
            3,
            create_group_stmt,
            create_user_stmt,
            create_network_stmt
        )
    );
    cleri_object_t * drop_stmt = cleri_sequence(
        CLERI_GID_DROP_STMT,
        2,
        k_drop,
        cleri_choice(
            CLERI_NONE,
            CLERI_FIRST_MATCH,
            7,
            drop_group_stmt,
            drop_series_stmt,
            drop_shard_stmt,
            drop_shards_stmt,
            drop_server_stmt,
            drop_user_stmt,
            drop_network_stmt
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
            2,
            grant_user_stmt,
            grant_network_stmt
        )
    );
    cleri_object_t * list_stmt = cleri_sequence(
        CLERI_GID_LIST_STMT,
        3,
        k_list,
        cleri_choice(
            CLERI_NONE,
            CLERI_FIRST_MATCH,
            7,
            list_series_stmt,
            list_users_stmt,
            list_networks_stmt,
            list_shards_stmt,
            list_groups_stmt,
            list_servers_stmt,
            list_pools_stmt
        ),
        cleri_optional(CLERI_NONE, limit_expr)
    );
    cleri_object_t * pause_stmt = cleri_sequence(
        CLERI_GID_PAUSE_STMT,
        2,
        k_pause,
        uuid
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
            2,
            revoke_user_stmt,
            revoke_network_stmt
        )
    );
    cleri_object_t * select_stmt = cleri_sequence(
        CLERI_GID_SELECT_STMT,
        7,
        k_select,
        cleri_list(CLERI_NONE, select_aggregate_expr, cleri_token(CLERI_NONE, ","), 1, 0, 0),
        k_from,
        series_match,
        cleri_optional(CLERI_NONE, where_series_stmt),
        cleri_optional(CLERI_NONE, cleri_choice(
            CLERI_NONE,
            CLERI_FIRST_MATCH,
            3,
            after_expr,
            between_expr,
            before_expr
        )),
        cleri_optional(CLERI_NONE, merge_expr)
    );
    cleri_object_t * show_props = cleri_choice(
        CLERI_GID_SHOW_PROPS,
        CLERI_FIRST_MATCH,
        35,
        k_buffer_path,
        k_buffer_size,
        k_dbname,
        k_dbpath,
        k_debug,
        k_drop_threshold,
        k_duration_log,
        k_duration_num,
        k_libuv,
        k_license,
        k_log_level,
        k_manhole,
        k_max_cache_expressions,
        k_max_open_files,
        k_mem_usage,
        k_open_files,
        k_optimize_pool,
        k_optimize_task,
        k_pool,
        k_query_timeout,
        k_received_points,
        k_reindex_progress,
        k_server,
        k_sharding_buffering,
        k_sharding_max_chunk_points,
        k_startup_time,
        k_status,
        k_sync_progress,
        k_task_queue,
        k_timezone,
        k_time_precision,
        k_uptime,
        k_uuid,
        k_version,
        k_who_am_i
    );
    cleri_object_t * show_stmt = cleri_sequence(
        CLERI_GID_SHOW_STMT,
        2,
        k_show,
        cleri_list(CLERI_NONE, show_props, cleri_token(CLERI_NONE, ","), 0, 0, 0)
    );
    cleri_object_t * timeit_stmt = cleri_repeat(CLERI_GID_TIMEIT_STMT, k_timeit, 1, 1);
    cleri_object_t * help_show = cleri_keyword(CLERI_GID_HELP_SHOW, "show", CLERI_CASE_INSENSITIVE);
    cleri_object_t * help_drop_network = cleri_keyword(CLERI_GID_HELP_DROP_NETWORK, "network", CLERI_CASE_INSENSITIVE);
    cleri_object_t * help_drop_shard = cleri_keyword(CLERI_GID_HELP_DROP_SHARD, "shard", CLERI_CASE_INSENSITIVE);
    cleri_object_t * help_drop_series = cleri_keyword(CLERI_GID_HELP_DROP_SERIES, "series", CLERI_CASE_INSENSITIVE);
    cleri_object_t * help_drop_group = cleri_keyword(CLERI_GID_HELP_DROP_GROUP, "group", CLERI_CASE_INSENSITIVE);
    cleri_object_t * help_drop_server = cleri_keyword(CLERI_GID_HELP_DROP_SERVER, "server", CLERI_CASE_INSENSITIVE);
    cleri_object_t * help_drop_user = cleri_keyword(CLERI_GID_HELP_DROP_USER, "user", CLERI_CASE_INSENSITIVE);
    cleri_object_t * help_drop = cleri_sequence(
        CLERI_GID_HELP_DROP,
        2,
        k_drop,
        cleri_optional(CLERI_NONE, cleri_choice(
            CLERI_NONE,
            CLERI_MOST_GREEDY,
            6,
            help_drop_network,
            help_drop_shard,
            help_drop_series,
            help_drop_group,
            help_drop_server,
            help_drop_user
        ))
    );
    cleri_object_t * help_noaccess = cleri_keyword(CLERI_GID_HELP_NOACCESS, "noaccess", CLERI_CASE_INSENSITIVE);
    cleri_object_t * help_timeit = cleri_keyword(CLERI_GID_HELP_TIMEIT, "timeit", CLERI_CASE_INSENSITIVE);
    cleri_object_t * help_timezones = cleri_keyword(CLERI_GID_HELP_TIMEZONES, "timezones", CLERI_CASE_INSENSITIVE);
    cleri_object_t * help_pause = cleri_keyword(CLERI_GID_HELP_PAUSE, "pause", CLERI_CASE_INSENSITIVE);
    cleri_object_t * help_alter_server = cleri_keyword(CLERI_GID_HELP_ALTER_SERVER, "server", CLERI_CASE_INSENSITIVE);
    cleri_object_t * help_alter_database = cleri_keyword(CLERI_GID_HELP_ALTER_DATABASE, "database", CLERI_CASE_INSENSITIVE);
    cleri_object_t * help_alter_user = cleri_keyword(CLERI_GID_HELP_ALTER_USER, "user", CLERI_CASE_INSENSITIVE);
    cleri_object_t * help_alter_network = cleri_keyword(CLERI_GID_HELP_ALTER_NETWORK, "network", CLERI_CASE_INSENSITIVE);
    cleri_object_t * help_alter_series = cleri_keyword(CLERI_GID_HELP_ALTER_SERIES, "series", CLERI_CASE_INSENSITIVE);
    cleri_object_t * help_alter_group = cleri_keyword(CLERI_GID_HELP_ALTER_GROUP, "group", CLERI_CASE_INSENSITIVE);
    cleri_object_t * help_alter = cleri_sequence(
        CLERI_GID_HELP_ALTER,
        2,
        k_alter,
        cleri_optional(CLERI_NONE, cleri_choice(
            CLERI_NONE,
            CLERI_MOST_GREEDY,
            6,
            help_alter_server,
            help_alter_database,
            help_alter_user,
            help_alter_network,
            help_alter_series,
            help_alter_group
        ))
    );
    cleri_object_t * help_count_shards = cleri_keyword(CLERI_GID_HELP_COUNT_SHARDS, "shards", CLERI_CASE_INSENSITIVE);
    cleri_object_t * help_count_networks = cleri_keyword(CLERI_GID_HELP_COUNT_NETWORKS, "networks", CLERI_CASE_INSENSITIVE);
    cleri_object_t * help_count_servers = cleri_keyword(CLERI_GID_HELP_COUNT_SERVERS, "servers", CLERI_CASE_INSENSITIVE);
    cleri_object_t * help_count_pools = cleri_keyword(CLERI_GID_HELP_COUNT_POOLS, "pools", CLERI_CASE_INSENSITIVE);
    cleri_object_t * help_count_series = cleri_keyword(CLERI_GID_HELP_COUNT_SERIES, "series", CLERI_CASE_INSENSITIVE);
    cleri_object_t * help_count_users = cleri_keyword(CLERI_GID_HELP_COUNT_USERS, "users", CLERI_CASE_INSENSITIVE);
    cleri_object_t * help_count_groups = cleri_keyword(CLERI_GID_HELP_COUNT_GROUPS, "groups", CLERI_CASE_INSENSITIVE);
    cleri_object_t * help_count = cleri_sequence(
        CLERI_GID_HELP_COUNT,
        2,
        k_count,
        cleri_optional(CLERI_NONE, cleri_choice(
            CLERI_NONE,
            CLERI_MOST_GREEDY,
            7,
            help_count_shards,
            help_count_networks,
            help_count_servers,
            help_count_pools,
            help_count_series,
            help_count_users,
            help_count_groups
        ))
    );
    cleri_object_t * help_select = cleri_keyword(CLERI_GID_HELP_SELECT, "select", CLERI_CASE_INSENSITIVE);
    cleri_object_t * help_grant = cleri_keyword(CLERI_GID_HELP_GRANT, "grant", CLERI_CASE_INSENSITIVE);
    cleri_object_t * help_revoke = cleri_keyword(CLERI_GID_HELP_REVOKE, "revoke", CLERI_CASE_INSENSITIVE);
    cleri_object_t * help_access = cleri_keyword(CLERI_GID_HELP_ACCESS, "access", CLERI_CASE_INSENSITIVE);
    cleri_object_t * help_create_group = cleri_keyword(CLERI_GID_HELP_CREATE_GROUP, "group", CLERI_CASE_INSENSITIVE);
    cleri_object_t * help_create_user = cleri_keyword(CLERI_GID_HELP_CREATE_USER, "user", CLERI_CASE_INSENSITIVE);
    cleri_object_t * help_create_network = cleri_keyword(CLERI_GID_HELP_CREATE_NETWORK, "network", CLERI_CASE_INSENSITIVE);
    cleri_object_t * help_create = cleri_sequence(
        CLERI_GID_HELP_CREATE,
        2,
        k_create,
        cleri_optional(CLERI_NONE, cleri_choice(
            CLERI_NONE,
            CLERI_MOST_GREEDY,
            3,
            help_create_group,
            help_create_user,
            help_create_network
        ))
    );
    cleri_object_t * help_functions = cleri_keyword(CLERI_GID_HELP_FUNCTIONS, "functions", CLERI_CASE_INSENSITIVE);
    cleri_object_t * help_continue = cleri_keyword(CLERI_GID_HELP_CONTINUE, "continue", CLERI_CASE_INSENSITIVE);
    cleri_object_t * help_list_groups = cleri_keyword(CLERI_GID_HELP_LIST_GROUPS, "groups", CLERI_CASE_INSENSITIVE);
    cleri_object_t * help_list_networks = cleri_keyword(CLERI_GID_HELP_LIST_NETWORKS, "networks", CLERI_CASE_INSENSITIVE);
    cleri_object_t * help_list_servers = cleri_keyword(CLERI_GID_HELP_LIST_SERVERS, "servers", CLERI_CASE_INSENSITIVE);
    cleri_object_t * help_list_series = cleri_keyword(CLERI_GID_HELP_LIST_SERIES, "series", CLERI_CASE_INSENSITIVE);
    cleri_object_t * help_list_pools = cleri_keyword(CLERI_GID_HELP_LIST_POOLS, "pools", CLERI_CASE_INSENSITIVE);
    cleri_object_t * help_list_users = cleri_keyword(CLERI_GID_HELP_LIST_USERS, "users", CLERI_CASE_INSENSITIVE);
    cleri_object_t * help_list_shards = cleri_keyword(CLERI_GID_HELP_LIST_SHARDS, "shards", CLERI_CASE_INSENSITIVE);
    cleri_object_t * help_list = cleri_sequence(
        CLERI_GID_HELP_LIST,
        2,
        k_list,
        cleri_optional(CLERI_NONE, cleri_choice(
            CLERI_NONE,
            CLERI_MOST_GREEDY,
            7,
            help_list_groups,
            help_list_networks,
            help_list_servers,
            help_list_series,
            help_list_pools,
            help_list_users,
            help_list_shards
        ))
    );
    cleri_object_t * help = cleri_sequence(
        CLERI_GID_HELP,
        2,
        k_help,
        cleri_optional(CLERI_NONE, cleri_choice(
            CLERI_NONE,
            CLERI_MOST_GREEDY,
            16,
            help_show,
            help_drop,
            help_noaccess,
            help_timeit,
            help_timezones,
            help_pause,
            help_alter,
            help_count,
            help_select,
            help_grant,
            help_revoke,
            help_access,
            help_create,
            help_functions,
            help_continue,
            help_list
        ))
    );
    cleri_object_t * START = cleri_sequence(
        CLERI_GID_START,
        3,
        cleri_optional(CLERI_NONE, timeit_stmt),
        cleri_optional(CLERI_NONE, cleri_choice(
            CLERI_NONE,
            CLERI_FIRST_MATCH,
            13,
            select_stmt,
            list_stmt,
            count_stmt,
            alter_stmt,
            continue_stmt,
            create_stmt,
            drop_stmt,
            grant_stmt,
            pause_stmt,
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
