max_repeat_n_map = {}
replace_map = {'r_singleq_str': ''}
max_list_n_map = {'series_match': 2, 'aggregate_functions': 2}
k_map = {
    'r_doubleq_str': {
        'k_as': '"MERGED"',
        'k_suffix': '"SUFFIX"',
        'k_prefix': '"PREFIX"',
        'series_name': '"000000"',
        'k_filter': 10,  # '"10"',
        'uuid': '"koos-VirtualBox:9010"',
        'k_name': '"000000"',
        'k_user': '"USER"',
        'k_password': '"PASSWORD"',
        'k_status': '"running"',
        'k_expression': '"/.*/"',
        'k_address': '"localhost"',
        'k_buffer_path': '"BUFFER_PATH"',
        'k_dbpath': '"DBPATH"',
        'k_uuid': '"UUID"',
        'k_version': '"VERSION"',
        'k_reindex_progress': '"REINDEX_PROGRESS"',
        'k_sync_progress': '"SYNC_PROGRESS"',
        'k_timezone': '"NAIVE"',
        'k_ip_support': '"ALL"',
        'k_libuv': '"1.8.0"',
        'k_server': '"SERVER"',

        'aggregate_functions': '"1970-1-1 1:00:10"',
        'k_start': '"1970-1-1 1:00:00"',
        'k_after': '"1970-1-1 1:00:00"',
        'k_between': '"1970-1-1 1:00:00"',
        'k_before': '"1970-1-1 1:01:00"',
        'k_and': '"1970-1-1 1:01:00"',
        'k_end': '"1970-1-1 1:01:00"',
        },
    'r_integer': {
        'k_series': 0,  # GROUPS
        'k_active_handles': 0,  # SERVERS
        'k_buffer_size': 0,  # SERVERS
        'k_port': 9000,  # SERVERS
        'k_startup_time': 0,  # SERVERS
        'k_max_open_files': 0,  # SERVERS
        'k_mem_usage': 0,  # SERVERS
        'k_open_files': 0,  # SERVERS
        'k_received_points': 0,  # SERVERS
        'k_uptime': 0,  # SERVERS,
        'k_servers': 0,  # POOLS
        'k_limit': 10,
        'k_sid': 0,
        'k_pool': 0,
        'k_filter': 10,
        'k_size': 10,
        'k_length': 10,
        'aggregate_functions': 10,
        'k_start': 0,
        'k_after': 0,
        'k_between': 0,
        'k_before': 60,
        'k_and': 60,
        'k_end': 60,
    },
    'r_float': {
        'k_filter': 10.0,
        'k_drop_threshold': 0.99},
    'r_time_str': {
        'aggregate_functions': '10s',
        'k_start': '0d',
        'k_after': '0d',
        'k_between': '0d',
        'k_before': '1m',
        'k_and': '1m',
        'k_end': '1m'},
    'r_uuid_str': {
        'r_uuid_str': '"UUID"'},
    'r_uinteger': {
    },
    'r_grave_str': {'r_grave_str': '`000000`'},
    'r_regex': {
        'r_regex': '/.*/'
    },
}
