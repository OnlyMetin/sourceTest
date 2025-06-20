// Basic features

// Enable or disable heap allocation debugging
//#define DEBUG_ALLOC

#pragma warning(disable : 4244) // disable conversion warning
#pragma warning(disable : 4018)

#include "debug_allocator.h"

#include "../../libthecore/include/stdafx.h"

#include "../../common/singleton.h"
#include "../../common/utils.h"
#include "../../common/service.h"
#include "../../common/length.h"

#include <algorithm>
#include <math.h>
#include <list>
#include <map>
#include <set>
#include <queue>
#include <string>
#include <array>
#include <vector>
#include <random>

#include <float.h>

#include <unordered_map>
#include <unordered_set>

#define isdigit iswdigit
#define isspace iswspace

#include "typedef.h"
#include "locale.hpp"
#include "event.h"

#define PASSES_PER_SEC(sec) ((sec) * passes_per_sec)

#ifndef M_PI
#define M_PI 3.14159265358979323846 /* pi */
#endif
#ifndef M_PI_2
#define M_PI_2 1.57079632679489661923 /* pi/2 */
#endif

#define IN
#define OUT
