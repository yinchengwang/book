/**
 * @file geography.c
 * @brief geography 类型 + 球面距离实现
 */
#include "db/geography.h"

#include <math.h>
#include <stddef.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

double geography_distance_haversine(const geography_point_t *a,
                                    const geography_point_t *b) {
    if (a == NULL || b == NULL) return -1.0;
    /* Haversine: a = sin²(Δφ/2) + cos(φ1)·cos(φ2)·sin²(Δλ/2)
     *         c = 2·atan2(√a, √(1-a))
     *         d = R·c
     */
    double dlat = b->lat_rad - a->lat_rad;
    double dlon = b->lon_rad - a->lon_rad;
    double sin_dlat = sin(dlat / 2.0);
    double sin_dlon = sin(dlon / 2.0);
    double aa = sin_dlat * sin_dlat
             + cos(a->lat_rad) * cos(b->lat_rad) * sin_dlon * sin_dlon;
    double c = 2.0 * atan2(sqrt(aa), sqrt(1.0 - aa));
    return GEO_WGS84_A * c;  /* 球面近似 WGS84 长半轴 */
}

double geography_distance_vincenty(const geography_point_t *a,
                                   const geography_point_t *b) {
    if (a == NULL || b == NULL) return -1.0;
    /* Vincenty 逆公式（WGS84 椭球面） */
    double a_ax = GEO_WGS84_A;
    double b_ax = GEO_WGS84_A * (1.0 - 1.0 / GEO_WGS84_F_INV);
    double f = 1.0 / GEO_WGS84_F_INV;
    double L = b->lon_rad - a->lon_rad;
    double U1 = atan((1.0 - f) * tan(a->lat_rad));
    double U2 = atan((1.0 - f) * tan(b->lat_rad));
    double sin_U1 = sin(U1), cos_U1 = cos(U1);
    double sin_U2 = sin(U2), cos_U2 = cos(U2);
    double lambda = L, lambda_p;
    int iter = 0;
    double sin_lambda, cos_lambda, sin_sigma, cos_sigma, sigma, sin_alpha, cos_sq_alpha, cos2_sigma_m, C;
    do {
        sin_lambda = sin(lambda);
        cos_lambda = cos(lambda);
        sin_sigma = sqrt((cos_U2 * sin_lambda) * (cos_U2 * sin_lambda)
                      + (cos_U1 * sin_U2 - sin_U1 * cos_U2 * cos_lambda)
                      * (cos_U1 * sin_U2 - sin_U1 * cos_U2 * cos_lambda));
        if (sin_sigma == 0) return 0.0;  /* 共点 */
        cos_sigma = sin_U1 * sin_U2 + cos_U1 * cos_U2 * cos_lambda;
        sigma = atan2(sin_sigma, cos_sigma);
        sin_alpha = cos_U1 * cos_U2 * sin_lambda / sin_sigma;
        cos_sq_alpha = 1.0 - sin_alpha * sin_alpha;
        cos2_sigma_m = (cos_sq_alpha == 0) ? 0 : cos_sigma - 2.0 * sin_U1 * sin_U2 / cos_sq_alpha;  /* 球面退化时 cos_sq_alpha=0 */
        C = f / 16.0 * cos_sq_alpha * (4.0 + f * (4.0 - 3.0 * cos_sq_alpha));
        lambda_p = lambda;
        lambda = L + (1.0 - C) * f * sin_alpha
               * (sigma + C * sin_sigma * (cos2_sigma_m + C * cos_sigma
               * (-1.0 + 2.0 * cos2_sigma_m * cos2_sigma_m)));
    } while (fabs(lambda - lambda_p) > 1e-12 && ++iter < 100);
    if (iter >= 100) return -1.0;  /* 未收敛 */
    double u_sq = cos_sq_alpha * (a_ax * a_ax - b_ax * b_ax) / (b_ax * b_ax);
    double A = 1.0 + u_sq / 16384.0 * (4096.0 + u_sq * (-768.0 + u_sq * (320.0 - 175.0 * u_sq)));
    double B = u_sq / 1024.0 * (256.0 + u_sq * (-128.0 + u_sq * (74.0 - 47.0 * u_sq)));
    double delta_sigma = B * sin_sigma * (cos2_sigma_m + B / 4.0
                       * (cos_sigma * (-1.0 + 2.0 * cos2_sigma_m * cos2_sigma_m)
                       - B / 6.0 * cos2_sigma_m * (-3.0 + 4.0 * sin_sigma * sin_sigma)
                       * (-3.0 + 4.0 * cos2_sigma_m * cos2_sigma_m)));
    return b_ax * A * (sigma - delta_sigma);
}

double geography_deg_lat_to_m(double deg_lat) {
    return deg_lat * GEO_DEG_LAT_TO_M;
}

double geography_deg_lon_to_m(double deg_lon, double at_lat_deg) {
    return deg_lon * GEO_DEG_LAT_TO_M * cos(GEO_DEG_TO_RAD(at_lat_deg));
}