/*
 * variance.h - Calculate variance for points.
 *
 * author       : Jeroen van der Heijden
 * email        : jeroen@transceptor.technology
 * copyright    : 2016, Transceptor Technology
 *
 * changes
 *  - initial version, 10-08-2016
 *
 */
#pragma once

#include <siri/db/points.h>

double siridb_variance(siridb_points_t * points);
