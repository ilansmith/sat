#include "util.h"

int code2code(code2code_t *list, int code)
{
    for (; list->code != -1 && list->code != code; list++);

    return list->code == -1 ? -1 : list->val;
}

char *code2str(code2str_t *list, int code)
{
    for (; list->code != -1 && list->code != code; list++);

    return list->code == -1 ? "" : list->str;
}

void coordinate_moore_neighbour(int hight, int length, coordinate_euclid_t *co, 
    wind_dir_t dir, coordinate_euclid_t *neighbour)
{
#define MOORE_SP_MOD(x, m) (((x) + (m)) % (m))
#define MOORE_SP_NEI(x) \
    (((4 - ((x) + 8) % 8) % 4 ? 1 : 0) * ((4 < (x + 8 ) % 8) ? -1 : 1))

    neighbour->n = MOORE_SP_MOD(co->n + MOORE_SP_NEI(dir - 2), hight);
    neighbour->m = MOORE_SP_MOD(co->m + MOORE_SP_NEI(dir), length);

#undef MOORE_SP_NEI
#undef MOORE_SP_MOD
}

wind_dir_t wind_dir_offset(wind_dir_t dir, int offset)
{
    return (dir + ((8 + offset % 8) % 8 )) % 8;
}

