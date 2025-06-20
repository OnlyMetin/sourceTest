#ifndef __INC_UTILS_H__
#define __INC_UTILS_H__

#include <math.h>

#define IS_SET(flag, bit) ((flag) & (bit))
#define SET_BIT(var, bit) ((var) |= (bit))
#define REMOVE_BIT(var, bit) ((var) &= ~(bit))
#define TOGGLE_BIT(var, bit) ((var) = (var) ^ (bit))

#define CHECK_VNUM_RANGE(vnum, min, max, range) \
	(range ? \
		((max > 0 && (vnum >= min && vnum <= max)) || \
		(max == 0 && vnum == min)) \
	: \
		((max > 0 && (vnum == min || vnum == max)) || \
		(max == 0 && vnum == min)))

inline float DISTANCE_SQRT(long dx, long dy)
{
	return ::sqrt((float)dx * dx + (float)dy * dy);
}

/*
inline float DISTANCE(int64_t x, int64_t y, int64_t dx, int64_t dy)
{
	return static_cast<float>(sqrt(abs((dx - x) * (dx - x)) + abs((dy - y) * (dy - y))));
}

inline float DISTANCE(const PIXEL_POSITION& rpos1, const PIXEL_POSITION& rpos2)
{
	return DISTANCE(rpos1.x, rpos1.y, rpos2.x, rpos2.y);
}
*/

inline int DISTANCE_APPROX(int dx, int dy)
{
	int min, max;

	if (dx < 0)
		dx = -dx;

	if (dy < 0)
		dy = -dy;

	if (dx < dy)
	{
		min = dx;
		max = dy;
	}
	else
	{
		min = dy;
		max = dx;
	}

	// coefficients equivalent to ( 123/128 * max ) and ( 51/128 * min )
	return (((max << 8) + (max << 3) - (max << 4) - (max << 1) +
		(min << 7) - (min << 5) + (min << 3) - (min << 1)) >> 8);
}

#ifndef __WIN32__
inline WORD MAKEWORD(BYTE a, BYTE b)
{
	return static_cast<WORD>(a) | (static_cast<WORD>(b) << 8);
}
#endif

extern void set_global_time(time_t t);
extern time_t get_global_time();

extern int dice(int number, int size);
extern size_t str_lower(const char* src, char* dest, size_t dest_size);

extern void skip_spaces(char** string);

extern const char* one_argument(const char* argument, char* first_arg, size_t first_size);
extern const char* two_arguments(const char* argument, char* first_arg, size_t first_size, char* second_arg, size_t second_size);
extern const char* three_arguments(const char* argument, char* first_arg, size_t first_size, char* second_arg, size_t second_size, char* third_flag, size_t third_size);
extern const char* first_cmd(const char* argument, char* first_arg, size_t first_arg_size, size_t* first_arg_len_result);

extern int CalculateDuration(int iSpd, int iDur);

extern float gauss_random(float avg = 0, float sigma = 1);

extern int parse_time_str(const char* str);

extern bool WildCaseCmp(const char* w, const char* s);

#endif /* __INC_UTILS_H__ */
