#include "../test.h"
#include <locale.h>
#include <siri/db/series.h>


static int test_series_ensure_type(void)
{
    test_start("siridb (series_ensure_type)");
    (void) setlocale(LC_NUMERIC, "English_Australia.1252");

    siridb_series_t series;
    qp_obj_t qp_obj;

    /* test with integer series  */
    {
        series.tp = TP_INT;

        qp_obj.tp = QP_INT64;
        qp_obj.via.int64 = -1;
        siridb_series_ensure_type(&series, &qp_obj);
        _assert (qp_obj.tp == QP_INT64);
        _assert (qp_obj.via.int64 == -1);

        qp_obj.tp = QP_DOUBLE;
        qp_obj.via.real = -1.0;
        siridb_series_ensure_type(&series, &qp_obj);
        _assert (qp_obj.tp == QP_INT64);
        _assert (qp_obj.via.int64 == -1);

        qp_obj.tp = QP_RAW;
        qp_obj.via.str = "55 percent";
        qp_obj.len = strlen(qp_obj.via.str);
        siridb_series_ensure_type(&series, &qp_obj);
        _assert (qp_obj.tp == QP_INT64);
        _assert (qp_obj.via.int64 == 55);

        qp_obj.tp = QP_RAW;
        qp_obj.via.str = "garbage";
        qp_obj.len = strlen(qp_obj.via.str);
        siridb_series_ensure_type(&series, &qp_obj);
        _assert (qp_obj.tp == QP_INT64);
        _assert (qp_obj.via.int64 == 0);
    }

    /* test with double series */
    {
        series.tp = TP_DOUBLE;

        qp_obj.tp = QP_DOUBLE;
        qp_obj.via.real = -1.1;
        siridb_series_ensure_type(&series, &qp_obj);
        _assert (qp_obj.tp == QP_DOUBLE);
        _assert (qp_obj.via.real == -1.1);

        qp_obj.tp = QP_INT64;
        qp_obj.via.int64 = -1;
        siridb_series_ensure_type(&series, &qp_obj);
        _assert (qp_obj.tp == QP_DOUBLE);
        _assert (qp_obj.via.real == -1.0);

        qp_obj.tp = QP_RAW;
        qp_obj.via.str = "0.5 percent";
        qp_obj.len = strlen(qp_obj.via.str);
        siridb_series_ensure_type(&series, &qp_obj);
        _assert (qp_obj.tp == QP_DOUBLE);
        _assert (qp_obj.via.real == 0.5);

        qp_obj.tp = QP_RAW;
        qp_obj.via.str = "garbage";
        qp_obj.len = strlen(qp_obj.via.str);
        siridb_series_ensure_type(&series, &qp_obj);
        _assert (qp_obj.tp == QP_DOUBLE);
        _assert (qp_obj.via.real == 0.0);
    }

    /* test with string series */
    {
        series.tp = TP_STRING;

        qp_obj.tp = QP_RAW;
        qp_obj.via.str = "55.3 percent";
        qp_obj.len = strlen(qp_obj.via.str);
        siridb_series_ensure_type(&series, &qp_obj);
        _assert (qp_obj.tp == QP_RAW);
        _assert (strlen("55.3 percent") == qp_obj.len);
        _assert (strncmp("55.3 percent", qp_obj.via.str, qp_obj.len) == 0);

        qp_obj.tp = QP_RAW;
        qp_obj.via.str = "";
        qp_obj.len = strlen(qp_obj.via.str);
        siridb_series_ensure_type(&series, &qp_obj);
        _assert (qp_obj.tp == QP_RAW);
        _assert (strlen("") == qp_obj.len);
        _assert (strncmp("", qp_obj.via.str, qp_obj.len) == 0);

        qp_obj.tp = QP_DOUBLE;
        qp_obj.via.real = -1.1;
        siridb_series_ensure_type(&series, &qp_obj);
        _assert (qp_obj.tp == QP_RAW);
        _assert (strlen("-1.100000") == qp_obj.len);
        _assert (strncmp("-1.100000", qp_obj.via.str, qp_obj.len) == 0);

        qp_obj.tp = QP_INT64;
        qp_obj.via.int64 = -1;
        siridb_series_ensure_type(&series, &qp_obj);
        _assert (qp_obj.tp == QP_RAW);
        _assert (strlen("-1") == qp_obj.len);
        _assert (strncmp("-1", qp_obj.via.str, qp_obj.len) == 0);
    }
    (void) setlocale(LC_ALL, NULL);
    return test_end();
};

int main()
{
    return (
        test_series_ensure_type() ||
        0
    );
};