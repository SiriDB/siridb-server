import logging
import re
from siridbhelp import help_structure
from pyleri import Choice
from pyleri import Grammar
from pyleri import Keyword
from pyleri import List
from pyleri import Optional
from pyleri import Prio
from pyleri import Regex
from pyleri import Repeat
from pyleri import Sequence
from pyleri import THIS
from pyleri import Token
from pyleri import Tokens


class SiriGrammar(Grammar):
    '''
    SiriDB grammar.

    Note: choices can be optimized using most_greedy=False when there
          is a preferable order in choices.
          This only should be used when there's no conflict in making a
          decision by the parser. (e.g. two choices should start with the
          same keyword because in that case we should usually take the most
          greedy one)
    '''

    RE_KEYWORDS = re.compile('[a-z_]+')

    # Regular expressions
    r_float = Regex('[-+]?[0-9]*\.?[0-9]+')
    r_integer = Regex('[-+]?[0-9]+')
    r_uinteger = Regex('[0-9]+')
    r_time_str = Regex('[0-9]+[smhdw]')
    r_singleq_str = Regex('(?:\'(?:[^\']*)\')+')
    r_doubleq_str = Regex('(?:"(?:[^"]*)")+')
    r_grave_str = Regex('(?:`(?:[^`]*)`)+')
    r_uuid_str = Regex(
        '[0-9a-f]{8}\-[0-9a-f]{4}\-[0-9a-f]{4}\-[0-9a-f]{4}\-[0-9a-f]{12}')
    # we only allow an optional 'i' for case-insensitive regex
    r_regex = Regex('(/[^/\\\\]*(?:\\\\.[^/\\\\]*)*/i?)')
    r_comment = Regex('#.*')

    # Keywords
    k_access = Keyword('access')
    k_active_handles = Keyword('active_handles')
    k_address = Keyword('address')
    k_after = Keyword('after')
    k_alter = Keyword('alter')
    k_and = Keyword('and')
    k_as = Keyword('as')
    k_backup_mode = Keyword('backup_mode')
    k_before = Keyword('before')
    k_buffer_size = Keyword('buffer_size')
    k_buffer_path = Keyword('buffer_path')
    k_between = Keyword('between')
    k_count = Keyword('count')
    k_create = Keyword('create')
    k_critical = Keyword('critical')
    k_database = Keyword('database')
    k_dbname = Keyword('dbname')
    k_dbpath = Keyword('dbpath')
    k_debug = Keyword('debug')
    k_derivative = Keyword('derivative')
    k_difference = Keyword('difference')
    k_drop = Keyword('drop')
    k_drop_threshold = Keyword('drop_threshold')
    k_duration_log = Keyword('duration_log')
    k_duration_num = Keyword('duration_num')
    k_end = Keyword('end')
    k_error = Keyword('error')
    k_expression = Keyword('expression')
    k_false = Keyword('false')
    k_filter = Keyword('filter')
    k_float = Keyword('float')
    k_for = Keyword('for')
    k_from = Keyword('from')
    k_full = Keyword('full')
    k_grant = Keyword('grant')
    k_group = Keyword('group')
    k_groups = Keyword('groups')
    k_help = Choice(Keyword('help'), Token('?'))
    k_info = Keyword('info')
    k_ignore_threshold = Keyword('ignore_threshold')
    k_insert = Keyword('insert')
    k_integer = Keyword('integer')
    k_intersection = Choice(
        Token('&'),
        Keyword('intersection'),
        most_greedy=False)
    k_ip_support = Keyword('ip_support')
    k_length = Keyword('length')
    k_libuv = Keyword('libuv')
    k_limit = Keyword('limit')
    k_list = Keyword('list')
    k_log = Keyword('log')
    k_log_level = Keyword('log_level')
    k_max = Keyword('max')
    k_max_open_files = Keyword('max_open_files')
    k_mean = Keyword('mean')
    k_median = Keyword('median')
    k_median_low = Keyword('median_low')
    k_median_high = Keyword('median_high')
    k_mem_usage = Keyword('mem_usage')
    k_merge = Keyword('merge')
    k_min = Keyword('min')
    k_modify = Keyword('modify')
    k_name = Keyword('name')
    k_now = Keyword('now')
    k_number = Keyword('number')
    k_online = Keyword('online')
    k_open_files = Keyword('open_files')
    k_or = Keyword('or')
    k_password = Keyword('password')
    k_points = Keyword('points')
    k_pool = Keyword('pool')
    k_pools = Keyword('pools')
    k_port = Keyword('port')
    k_prefix = Keyword('prefix')
    k_pvariance = Keyword('pvariance')
    k_read = Keyword('read')
    k_received_points = Keyword('received_points')
    k_reindex_progress = Keyword('reindex_progress')
    k_revoke = Keyword('revoke')
    k_select = Keyword('select')
    k_series = Keyword('series')
    k_server = Keyword('server')
    k_servers = Keyword('servers')
    k_set = Keyword('set')
    k_sid = Keyword('sid')
    k_shards = Keyword('shards')
    k_show = Keyword('show')
    k_size = Keyword('size')
    k_start = Keyword('start')
    k_startup_time = Keyword('startup_time')
    k_status = Keyword('status')
    k_string = Keyword('string')
    k_suffix = Keyword('suffix')
    k_sum = Keyword('sum')
    k_symmetric_difference = Choice(
        Token('^'),
        Keyword('symmetric_difference'),
        most_greedy=False)
    k_sync_progress = Keyword('sync_progress')
    k_timeit = Keyword('timeit')
    k_timezone = Keyword('timezone')
    k_time_precision = Keyword('time_precision')
    k_to = Keyword('to')
    k_true = Keyword('true')
    k_type = Keyword('type')
    k_union = Choice(
        Tokens(', |'),
        Keyword('union'),
        most_greedy=False)
    k_uptime = Keyword('uptime')
    k_user = Keyword('user')
    k_users = Keyword('users')
    k_using = Keyword('using')
    k_uuid = Keyword('uuid')
    k_variance = Keyword('variance')
    k_version = Keyword('version')
    k_warning = Keyword('warning')
    k_where = Keyword('where')
    k_who_am_i = Keyword('who_am_i')
    k_write = Keyword('write')
    c_difference = Choice(
        Token('-'),
        k_difference,
        most_greedy=False)

    access_keywords = Choice(
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
        most_greedy=False)

    _boolean = Choice(k_true, k_false, most_greedy=False)

    log_keywords = Choice(
        k_debug,
        k_info,
        k_warning,
        k_error,
        k_critical,
        most_greedy=False)

    int_expr = Prio(
        r_integer,
        Sequence('(', THIS, ')'),
        Sequence(THIS, Tokens('+ - * % /'), THIS))

    string = Choice(r_singleq_str, r_doubleq_str, most_greedy=False)

    time_expr = Prio(
        r_time_str,
        k_now,
        string,
        r_integer,
        Sequence('(', THIS, ')'),
        Sequence(THIS, Tokens('+ - * % /'), THIS))

    series_columns = List(Choice(
        k_name,
        k_type,
        k_length,
        k_start,
        k_end,
        k_pool,
        most_greedy=False), ',', 1)

    shard_columns = List(Choice(
        k_sid,
        k_pool,
        k_server,
        k_size,
        k_start,
        k_end,
        k_type,
        k_status,
        most_greedy=False), ',', 1)

    server_columns = List(Choice(
        # Local properties
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
        # Remote properties
        k_active_handles,
        k_log_level,
        k_max_open_files,
        k_mem_usage,
        k_open_files,
        k_received_points,
        k_reindex_progress,
        k_sync_progress,
        k_uptime,
        most_greedy=False), ',', 1)

    group_columns = List(Choice(
        k_expression,
        k_name,
        k_series,
        most_greedy=False), ',', 1)

    user_columns = List(Choice(
        k_name,
        k_access,
        most_greedy=False), ',', 1)

    pool_props = Choice(
        k_pool,
        k_servers,
        k_series,
        most_greedy=False)
    pool_columns = List(pool_props, ',', 1)

    bool_operator = Tokens('== !=')
    int_operator = Tokens('< > == != <= >=')
    str_operator = Tokens('< > == != <= >= ~ !~')

    # where group
    where_group = Sequence(k_where, Prio(
        Sequence(k_series, int_operator, int_expr),
        Sequence(Choice(k_expression, k_name, most_greedy=False), str_operator, string),
        Sequence('(', THIS, ')'),
        Sequence(THIS, k_and, THIS),
        Sequence(THIS, k_or, THIS)))

    # where pool
    where_pool = Sequence(k_where, Prio(
        Sequence(pool_props, int_operator, int_expr),
        Sequence('(', THIS, ')'),
        Sequence(THIS, k_and, THIS),
        Sequence(THIS, k_or, THIS)))

    # where series
    where_series = Sequence(k_where, Prio(
        Sequence(Choice(k_length, k_pool, most_greedy=False), int_operator, int_expr),
        Sequence(k_name, str_operator, string),
        Sequence(Choice(k_start, k_end, most_greedy=False), int_operator, time_expr),
        Sequence(k_type, bool_operator, Choice(k_string, k_integer, k_float, most_greedy=False)),
        Sequence('(', THIS, ')'),
        Sequence(THIS, k_and, THIS),
        Sequence(THIS, k_or, THIS)))

    # where server
    where_server = Sequence(k_where, Prio(
        Sequence(Choice(
            k_active_handles,
            k_buffer_size,
            k_port,
            k_pool,
            k_startup_time,
            k_max_open_files,
            k_mem_usage,
            k_open_files,
            k_received_points,
            k_uptime,
            most_greedy=False), int_operator, int_expr),
        Sequence(Choice(
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
            k_sync_progress,
            most_greedy=False), str_operator, string),
        Sequence(k_online, bool_operator, _boolean),
        Sequence(k_log_level, int_operator, log_keywords),
        Sequence('(', THIS, ')'),
        Sequence(THIS, k_and, THIS),
        Sequence(THIS, k_or, THIS)))

    # where shard
    where_shard = Sequence(k_where, Prio(
        Sequence(Choice(k_sid, k_pool, k_size, most_greedy=False), int_operator, int_expr),
        Sequence(Choice(k_server, k_status), str_operator, string),
        Sequence(Choice(k_start, k_end, most_greedy=False), int_operator, time_expr),
        Sequence(k_type, bool_operator, Choice(k_number, k_log, most_greedy=False)),
        Sequence('(', THIS, ')'),
        Sequence(THIS, k_and, THIS),
        Sequence(THIS, k_or, THIS)))

    # where user
    where_user = Sequence(k_where, Prio(
        Sequence(k_name, str_operator, string),
        Sequence(k_access, int_operator, access_keywords),
        Sequence('(', THIS, ')'),
        Sequence(THIS, k_and, THIS),
        Sequence(THIS, k_or, THIS)))

    series_sep = Choice(
        k_union,
        c_difference,
        k_intersection,
        k_symmetric_difference,
        most_greedy=False)

    series_name = Repeat(string, 1, 1)
    group_name = Repeat(r_grave_str, 1, 1)
    series_re = Repeat(r_regex, 1, 1)
    uuid = Choice(r_uuid_str, string, most_greedy=False)
    group_match = Repeat(r_grave_str, 1, 1)
    series_match = List(
        Choice(series_name, group_match, series_re, most_greedy=False),
        series_sep, 1)
    limit_expr = Sequence(k_limit, int_expr)

    before_expr = Sequence(k_before, time_expr)
    after_expr = Sequence(k_after, time_expr)
    between_expr = Sequence(k_between, time_expr, k_and, time_expr)
    access_expr = List(access_keywords, ',', 1)

    prefix_expr = Sequence(k_prefix, string)
    suffix_expr = Sequence(k_suffix, string)

    f_points = Choice(
        Token('*'),
        k_points,
        most_greedy=False)

    f_difference = Sequence(
        k_difference,
        '(',
        Optional(time_expr),
        ')')
    f_derivative = Sequence(
        k_derivative,
        '(',
        List(time_expr, ',', 0, 2),
        ')')
    f_mean = Sequence(
        k_mean,
        '(', time_expr, ')')
    f_median = Sequence(
        k_median,
        '(', time_expr, ')')
    f_median_low = Sequence(
        k_median_low,
        '(', time_expr, ')')
    f_median_high = Sequence(
        k_median_high,
        '(', time_expr, ')')
    f_sum = Sequence(
        k_sum,
        '(', time_expr, ')')
    f_min = Sequence(
        k_min,
        '(', time_expr, ')')
    f_max = Sequence(
        k_max,
        '(', time_expr, ')')
    f_count = Sequence(
        k_count,
        '(', time_expr, ')')
    f_variance = Sequence(
        k_variance,
        '(', time_expr, ')')
    f_pvariance = Sequence(
        k_pvariance,
        '(', time_expr, ')')
    f_filter = Sequence(
        k_filter,
        '(',
        Optional(str_operator),
        Choice(
            string,
            r_integer,
            r_float,
            most_greedy=True),
        ')')

    aggregate_functions = List(Choice(
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
        f_filter,
        most_greedy=False), '=>', 1)

    select_aggregate = Sequence(
        aggregate_functions,
        Optional(prefix_expr),
        Optional(suffix_expr))

    merge_as = Sequence(
        k_merge,
        k_as,
        string,
        Optional(Sequence(k_using, aggregate_functions)))

    set_address = Sequence(k_set, k_address, string)
    set_backup_mode = Sequence(k_set, k_backup_mode, _boolean)
    set_drop_threshold = Sequence(k_set, k_drop_threshold, r_float)
    set_expression = Sequence(k_set, k_expression, r_regex)
    set_ignore_threshold = Sequence(k_set, k_ignore_threshold, _boolean)
    set_log_level = Sequence(k_set, k_log_level, log_keywords)
    set_name = Sequence(k_set, k_name, string)
    set_password = Sequence(k_set, k_password, string)
    set_port = Sequence(k_set, k_port, r_uinteger)
    set_timezone = Sequence(k_set, k_timezone, string)

    alter_database = Sequence(k_database, Choice(
        set_drop_threshold,
        set_timezone,
        most_greedy=False))

    alter_group = Sequence(k_group, group_name, Choice(
        set_expression,
        set_name,
        most_greedy=False))

    alter_server = Sequence(k_server, uuid, Choice(
        set_log_level,
        set_backup_mode,
        set_address,
        set_port,
        most_greedy=False))

    alter_servers = Sequence(k_servers, Optional(where_server), set_log_level)

    alter_user = Sequence(k_user, string, Choice(
        set_password,
        set_name,
        most_greedy=False))

    count_groups = Sequence(
        k_groups, Optional(where_group))
    count_pools = Sequence(
        k_pools, Optional(where_pool))
    count_series = Sequence(
        k_series, Optional(series_match), Optional(where_series))
    count_servers = Sequence(
        k_servers, Optional(where_server))
    count_servers_received = Sequence(
        k_servers,
        k_received_points,
        Optional(where_server))
    count_shards = Sequence(
        k_shards, Optional(where_shard))
    count_shards_size = Sequence(
        k_shards, k_size, Optional(where_shard))
    count_users = Sequence(
        k_users, Optional(where_user))
    count_series_length = Sequence(
        k_series,
        k_length,
        Optional(series_match),
        Optional(where_series))

    create_group = Sequence(
        k_group, group_name, k_for, r_regex)
    create_user = Sequence(
        k_user, string, set_password)

    drop_group = Sequence(k_group, group_name)
    # Drop statement needs at least a series_math or where STMT or both
    drop_series = Sequence(
        k_series,
        Optional(series_match),
        Optional(where_series),
        Optional(set_ignore_threshold))
    drop_shards = Sequence(
        k_shards,
        Optional(where_shard),
        Optional(set_ignore_threshold))
    drop_server = Sequence(k_server, uuid)
    drop_user = Sequence(k_user, string)

    grant_user = Sequence(
        k_user, string, Optional(set_password))

    list_groups = Sequence(
        k_groups, Optional(group_columns), Optional(where_group))
    list_pools = Sequence(
        k_pools, Optional(pool_columns), Optional(where_pool))
    list_series = Sequence(
        k_series,
        Optional(series_columns),
        Optional(series_match),
        Optional(where_series))
    list_servers = Sequence(
        k_servers, Optional(server_columns), Optional(where_server))
    list_shards = Sequence(
        k_shards, Optional(shard_columns), Optional(where_shard))
    list_users = Sequence(
        k_users, Optional(user_columns), Optional(where_user))

    revoke_user = Sequence(k_user, string)

    alter_stmt = Sequence(k_alter, Choice(
        alter_user,
        alter_group,
        alter_server,
        alter_servers,
        alter_database,
        most_greedy=False))

    calc_stmt = Repeat(time_expr, 1, 1)

    count_stmt = Sequence(k_count, Choice(
        count_groups,
        count_pools,
        count_series,
        count_servers,
        count_servers_received,
        count_shards,
        count_shards_size,
        count_users,
        count_series_length,
        most_greedy=True))

    create_stmt = Sequence(k_create, Choice(
        create_group,
        create_user))

    drop_stmt = Sequence(k_drop, Choice(
        drop_group,
        drop_series,
        drop_shards,
        drop_server,
        drop_user,
        most_greedy=False))

    grant_stmt = Sequence(k_grant, access_expr, k_to, Choice(
        grant_user,
        most_greedy=False))

    list_stmt = Sequence(k_list, Choice(
        list_series,
        list_users,
        list_shards,
        list_groups,
        list_servers,
        list_pools,
        most_greedy=False
    ), Optional(limit_expr))

    revoke_stmt = Sequence(k_revoke, access_expr, k_from, Choice(
        revoke_user,
        most_greedy=False))

    select_stmt = Sequence(
        k_select,
        List(select_aggregate, ',', 1),
        k_from,
        series_match,
        Optional(where_series),
        Optional(Choice(
            after_expr,
            between_expr,
            before_expr,
            most_greedy=False)),
        Optional(merge_as))

    show_stmt = Sequence(k_show, List(Choice(
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
        k_who_am_i,
        most_greedy=False), ',', 0))

    timeit_stmt = Repeat(k_timeit, 1, 1)


def _set_attribute(cls, name, value):
    setattr(cls, name, value)
    if name not in cls._order:
        cls._order.append(name)
    if hasattr(value, 'name'):
        raise SyntaxError('Element name is set to {0!r} and therefore cannot '
                          'be set to {1!r}. Use Repeat({0}, 1, 1) as a '
                          'workaround.'.format(value.name, name))
    value.name = name


def _walk(cls, path, structure):
    name = '_'.join(path)
    try:
        if structure:
            for child_path, child in structure.items():
                _walk(cls, child_path, child)
            value = Sequence(*[getattr(cls,
                                       'k_' + path[-1],
                                       Keyword(path[-1])), Optional(
                Choice(*[
                    getattr(cls, '_'.join(p)) for p in structure.keys()]))])
        else:
            value = Keyword(path[-1])
    except AttributeError:
        logging.critical('Cannot parse help file: {!r}'.format(
            '{}.md'.format('_'.join(path))))
        import sys
        sys.exit(1)
    else:
        _set_attribute(cls, name, value)


def _build_help(cls):
    '''Add 'help' statements to this class.

    Help files are MarkDown files which are read by the help module.
    '''
    for path, structure in help_structure.items():
        _walk(cls, path, structure)

_build_help(SiriGrammar)
_set_attribute(SiriGrammar, 'START', Sequence(
    Optional(SiriGrammar.timeit_stmt),
    Optional(Choice(
        SiriGrammar.select_stmt,
        SiriGrammar.list_stmt,
        SiriGrammar.count_stmt,
        SiriGrammar.alter_stmt,
        SiriGrammar.create_stmt,
        SiriGrammar.drop_stmt,
        SiriGrammar.grant_stmt,
        SiriGrammar.revoke_stmt,
        SiriGrammar.show_stmt,
        SiriGrammar.calc_stmt,
        SiriGrammar.help,
        most_greedy=False)),
    Optional(SiriGrammar.r_comment)))


siri_grammar = SiriGrammar()
