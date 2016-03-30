union siridb_ts_u
{
    uint32_t * ts32;
    uint64_t * ts64;
};

typedef struct siridb_points_s
{
    size_t len;

    union siridb_ts_u ts;
}siridb_points_t;
