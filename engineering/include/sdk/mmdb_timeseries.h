/**
 * @file mmdb_timeseries.h
 * @brief 时序模型 API
 */
#ifndef SDK_MMDB_TIMESERIES_H
#define SDK_MMDB_TIMESERIES_H

#include "sdk/mmdb.h"

#ifdef __cplusplus
extern "C" {
#endif

int mmdb_timeseries_append(mmdb_collection_t* c, const mmdb_datapoint_t* dp);
int mmdb_timeseries_append_batch(mmdb_collection_t* c, const mmdb_datapoint_t* dps,
                                   size_t n);
int mmdb_timeseries_query(mmdb_collection_t* c, const mmdb_ts_query_t* q,
                           mmdb_result_t* out);

#ifdef __cplusplus
}
#endif

#endif