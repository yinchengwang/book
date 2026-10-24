/**
 * @file geography.h
 * @brief geography 类型 + Haversine/Vincenty 球面距离（C2-3 T5）
 *
 * 经纬度地理坐标类型与球面距离计算。区别于 planar geometry（Cartesian）。
 */
#ifndef DB_GEOGRAPHY_H
#define DB_GEOGRAPHY_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** geography 类型：经纬度（弧度）+ WGS84 椭球参数 */
typedef struct geography_point_s {
    double lon_rad;   /**< 经度（弧度） */
    double lat_rad;   /**< 纬度（弧度） */
} geography_point_t;

/** WGS84 椭球参数（与 PostGIS geography 一致） */
#define GEO_WGS84_A            6378137.0        /**< 长半轴 (m) */
#define GEO_WGS84_F_INV        298.257223563    /**< 扁率倒数 */

/** 角度↔弧度 */
#define GEO_DEG_TO_RAD(deg) ((deg) * 0.017453292519943295769)
#define GEO_RAD_TO_DEG(rad) ((rad) * 57.295779513082320877)

/** 1° 纬度 ≈ 111000 m（用于度↔米快速转换） */
#define GEO_DEG_LAT_TO_M      111000.0

/**
 * @brief Haversine 球面距离（m）—— 假设球面，对短距离精度足够
 */
double geography_distance_haversine(const geography_point_t *a,
                                    const geography_point_t *b);

/**
 * @brief Vincenty 大圆距离（m）—— 椭球面，精度高（WGS84）
 */
double geography_distance_vincenty(const geography_point_t *a,
                                   const geography_point_t *b);

/**
 * @brief 度↔米转换（粗略）：lat 1° 固定 111km，lon 按 cos(lat) 缩放
 */
double geography_deg_lat_to_m(double deg_lat);
double geography_deg_lon_to_m(double deg_lon, double at_lat_deg);

#ifdef __cplusplus
}
#endif

#endif /* DB_GEOGRAPHY_H */
