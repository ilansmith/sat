#include "sat.h"
#include "sat_ca.h"
#include "util.h"
#include <stdlib.h>

#define ID_UNDEFINED 0
#define CL_UNDEFINED 0
#define LP_UNDEFINED -1

#define PRINT_SCHEME_START(table) \
	static sat_print_t table[] = {
#define PRINT_SCHEME_ENTRY(val, sym, colour, is_bright) \
	[val] = {sym, colour, is_bright},
#define PRINT_SCHEME_END \
	};

#define ROTATE(dir, ca, exp) do { \
	ca_direction_t X; \
	for (X = DR_UP; X != DR_UNDEFINED && !(exp); X++); \
	dir = X; \
} while (0);

#define IDX_QUIESCENT 0
#define IDX_QUIESCENT_BOUND 1
#define IDX_ERASE_LOOP 2
#define IDX_COLLISION 3
#define IDX_BRANCH_SEQ 4
#define IDX_GEN_ZERO 5
#define IDX_GEN_ONE 6
#define IDX_MONITOR_ACTIVE 7
#define IDX_MONITOR_ALLERT 8

typedef enum {
	CD_QUIESCENT = 0,
	CD_FLOW = 1,
	CD_GROW = 2,
	CD_TURN_LEFT = 3,
	CD_ARM_EXT_START = 4,
	CD_ARM_EXT_END = 5,
	CD_DETACH = 6,
	CD_UNEXPLORED_0 = 7,
	CD_UNEXPLORED_1 = 8,
	CD_ZERO = 9,
	CD_ONE = 10,
	CD_TAUTOLOGY = 11,
	CD_PARADOX = 12,
} ca_code_t;

typedef enum {
	DR_QUIESCENT = 0,
	DR_UP = 1,
	DR_DOWN = 2,
	DR_LEFT = 3,
	DR_RIGHT = 4,
	DR_UNDEFINED = 5
} ca_direction_t;

typedef enum {
	CL_QUIESCENT = 0,
	CL_WHITE = 1,
	CL_YELLOW = 2,
	CL_BLUE = 3,
	CL_RED = 4
} ca_colour_t;

typedef enum {
	FL_QUIESCENT = SHIFT_LEFT(IDX_QUIESCENT),
	FL_ERASE_LOOP = SHIFT_LEFT(IDX_ERASE_LOOP),
	FL_COLLISION = SHIFT_LEFT(IDX_COLLISION),
	FL_BRANCH_SEQ = SHIFT_LEFT(IDX_BRANCH_SEQ),
	FL_GEN_ZERO = SHIFT_LEFT(IDX_GEN_ZERO),
	FL_GEN_ONE = SHIFT_LEFT(IDX_GEN_ONE),
	FL_MONITOR_ACTIVE = SHIFT_LEFT(IDX_MONITOR_ACTIVE),
	FL_MONITOR_ALLERT = SHIFT_LEFT(IDX_MONITOR_ALLERT),
} ca_flag_t;

typedef enum {
	MN_NOOP = 0,
	MN_SUCCESS = 1,
	MN_ACTIVE = 2,
	MN_ALLERT = 3,
	MN_FAIL = 4
} ca_monitor_state_t;

typedef struct monitor_t {
	int *pos_tbl;
	int pos_tbl_sz;
	int offset;
	int is_active;
} monitor_t;

typedef struct ca_t {
	struct ca_t *neighbours[8];
	ca_code_t code;
	ca_direction_t dir;
	ca_colour_t col;
	ca_flag_t flag;
	int id;
	monitor_t mon;
	void *loop;
} ca_t;

typedef struct ca_space_t {
	ca_space_cb_t *cb;
	ca_t **sp;
	int sp_dim;
	int loop_dim;
	sat_print_field_t print_field;
	int x0;
	int y0;
} ca_space_t;

typedef struct sat_loop_t {
	struct sat_loop_t *next;
	ca_t **ca_list;
	int is_success;
} sat_loop_t;

PRINT_SCHEME_START(sat_print_code)
	PRINT_SCHEME_ENTRY(CD_QUIESCENT, ' ', COL_BLACK, ATTR_DULL)
	PRINT_SCHEME_ENTRY(CD_FLOW, 'o', COL_GREY, ATTR_DULL)
	PRINT_SCHEME_ENTRY(CD_GROW, 'G', COL_RED, ATTR_BRIGHT)
	PRINT_SCHEME_ENTRY(CD_TURN_LEFT, 'L', COL_YELLOW, ATTR_DULL)
	PRINT_SCHEME_ENTRY(CD_ARM_EXT_START, 'E', COL_BLUE, ATTR_BRIGHT)
	PRINT_SCHEME_ENTRY(CD_ARM_EXT_END, 'F', COL_MAGENTA, ATTR_DULL)
	PRINT_SCHEME_ENTRY(CD_DETACH, 'D', COL_WHITE, ATTR_BRIGHT)
	PRINT_SCHEME_ENTRY(CD_UNEXPLORED_0, 'A', COL_GREEN, ATTR_BRIGHT)
	PRINT_SCHEME_ENTRY(CD_ZERO, '0', COL_YELLOW, ATTR_BRIGHT)
	PRINT_SCHEME_ENTRY(CD_ONE, '1', COL_CYAN, ATTR_BRIGHT)
	PRINT_SCHEME_END;

PRINT_SCHEME_START(sat_print_dir)
	PRINT_SCHEME_ENTRY(DR_QUIESCENT, ' ', COL_BLACK, ATTR_DULL)
	PRINT_SCHEME_ENTRY(DR_UP, '^', COL_WHITE, ATTR_DULL)
	PRINT_SCHEME_ENTRY(DR_RIGHT, '>', COL_RED, ATTR_DULL)
	PRINT_SCHEME_ENTRY(DR_DOWN, 'v', COL_GREEN, ATTR_DULL)
	PRINT_SCHEME_ENTRY(DR_LEFT, '<', COL_CYAN, ATTR_DULL)
	PRINT_SCHEME_END;

PRINT_SCHEME_START(sat_print_flag)
PRINT_SCHEME_ENTRY(IDX_QUIESCENT, ' ', COL_BLACK, ATTR_DULL)
PRINT_SCHEME_ENTRY(IDX_QUIESCENT_BOUND, ASCII_DOT, COL_GREY, ATTR_DULL)
	PRINT_SCHEME_ENTRY(IDX_ERASE_LOOP, '#', COL_RED, ATTR_BRIGHT)
	PRINT_SCHEME_ENTRY(IDX_COLLISION, '!', COL_BLUE, ATTR_BRIGHT)
	PRINT_SCHEME_ENTRY(IDX_BRANCH_SEQ, '*', COL_MAGENTA, ATTR_DULL)
	PRINT_SCHEME_ENTRY(IDX_GEN_ZERO, '+', COL_YELLOW, ATTR_BRIGHT)
	PRINT_SCHEME_ENTRY(IDX_GEN_ONE, '-', COL_CYAN, ATTR_BRIGHT)
	PRINT_SCHEME_ENTRY(IDX_MONITOR_ACTIVE, '?', COL_GREEN, ATTR_BRIGHT)
	PRINT_SCHEME_ENTRY(IDX_MONITOR_ALLERT, '%', COL_WHITE, ATTR_BRIGHT)
	PRINT_SCHEME_END;

PRINT_SCHEME_START(sat_print_colour)
	PRINT_SCHEME_ENTRY(CL_QUIESCENT, ' ', COL_BLACK, ATTR_DULL)
	PRINT_SCHEME_ENTRY(CL_WHITE, 'w', COL_WHITE, ATTR_DULL)
	PRINT_SCHEME_ENTRY(CL_RED, 'r', COL_RED, ATTR_DULL)
	PRINT_SCHEME_ENTRY(CL_YELLOW, 'g', COL_GREEN, ATTR_DULL)
	PRINT_SCHEME_ENTRY(CL_BLUE, 'b', COL_CYAN, ATTR_DULL)
	PRINT_SCHEME_END;

	void sat_print_init(void *o, int dim, sat_print_speed_t print_speed);
	void sat_print_uninit(void);

	static sat_loop_t *loop_queue_active;
	static sat_loop_t *loop_queue_success;
	static ca_space_t ca_space;
	static table_t *table;
	static int loop_len, clause_num;
	static void loop_fail_reset(void *o);
	static void mon_spread_scan(void *o);
	static void ca_scan(void *o);
	static void mon_spread_init_phase2(void *o);
	static code2code_t truth_values[] = {
		{CD_ZERO, SAT_TV_FALSE},
		{CD_ONE, SAT_TV_TRUE},
		{-1}
	};
static code2code_t flag2idx[] = {
	{FL_QUIESCENT, IDX_QUIESCENT},
	{FL_ERASE_LOOP, IDX_ERASE_LOOP},
	{FL_COLLISION, IDX_COLLISION},
	{FL_BRANCH_SEQ, IDX_BRANCH_SEQ},
	{FL_GEN_ZERO, IDX_GEN_ZERO},
	{FL_GEN_ONE, IDX_GEN_ONE},
	{FL_MONITOR_ACTIVE, IDX_MONITOR_ACTIVE},
	{FL_MONITOR_ALLERT, IDX_MONITOR_ALLERT},
	{-1}
};
static wind_dir_t cadir2dir(ca_direction_t dir)
{
	code2code_t cd2d[] = {
		{DR_UP, DIR_NO},
		{DR_LEFT, DIR_WE},
		{DR_DOWN, DIR_SO},
		{DR_RIGHT, DIR_EA},
		{-1}
	};

	return code2code(cd2d, dir);
}

static ca_direction_t dir2cadir(wind_dir_t dir)
{
	code2code_t d2cd[] = {
		{DIR_NO, DR_UP},
		{DIR_WE, DR_LEFT},
		{DIR_SO, DR_DOWN},
		{DIR_EA, DR_RIGHT},
		{-1}
	};

	return code2code(d2cd, dir);
}

static ca_direction_t dir_opposite(ca_direction_t dir)
{
	ca_direction_t opp;

	switch (dir)
	{
	case DR_UP:
		opp = DR_DOWN;
		break;
	case DR_LEFT:
		opp = DR_RIGHT;
		break;
	case DR_RIGHT:
		opp = DR_LEFT;
		break;
	case DR_DOWN:
		opp = DR_UP;
		break;
	default:
		opp = DR_UNDEFINED;
		break;
	}

	return opp;
}

static void error_set(void)
{
	signal_set(SIG_ERROR);
}

static void signal_loop(void *o)
{
	signal_set(SIG_LOOP);
}

static ca_t *pointing_neighbour(void *o)
{
	ca_t *ca = (ca_t *)o;
	ca_direction_t opp = dir_opposite(ca->dir);

	return opp == DR_UNDEFINED ? NULL : ca->neighbours[cadir2dir(opp)];
}

static sat_loop_t *loop_remove_active(sat_loop_t *loop)
{
	sat_loop_t **q, *ret;

	for (q = &loop_queue_active; (ret = *q) && *q != loop; q = &(*q)->next);

	*q = (*q)->next;
	event_add_once(signal_loop, NULL);

	return ret;
}

static void loop_del(sat_loop_t *loop)
{
	int i;

	for (i = 0; i < loop_len; i++)
		((ca_t *)loop->ca_list[i])->loop = NULL;
	free(loop->ca_list);
	free(loop);
}

static void loop_new(void *o)
{
	ca_t *tmp = (ca_t *)o, **ca_list;
	sat_loop_t *loop;
	int i;

	if (!(loop = calloc(1, sizeof(sat_loop_t))))
		goto Error;

	if (!(ca_list = calloc(loop_len, sizeof(ca_t *)))) {
		free(loop);
		goto Error;
	}

	loop->ca_list = ca_list;
	for (i = 0; i < loop_len; i++) {
		tmp->loop = loop;
		loop->ca_list[i] = tmp;
		tmp = pointing_neighbour(tmp);
	}
	loop->next = loop_queue_active;
	loop_queue_active = loop;
	return;

Error:
	error_set();
}

static void loop_new_set(ca_t *ca)
{
	event_add(loop_new, ca);
}

static void loop_fail(void *o)
{
	loop_del(loop_remove_active((sat_loop_t *)o));
}

static void loop_fail_set(void *o)
{
	sat_loop_t *loop = (sat_loop_t *)o;
	int i;

	for (i = 0; i < loop_len && loop->ca_list[i]->flag != FL_ERASE_LOOP;
		i++);
	event_add_once(i < loop_len ? loop_fail_reset : loop_fail, o);
}

static void loop_fail_reset(void *o)
{
	event_add_once(loop_fail_set, o);
}

static void loop_success(void *o)
{
	sat_loop_t *loop = (sat_loop_t *)o;

	loop->is_success = 1;
	loop_remove_active(loop);
	loop->next = loop_queue_success;
	loop_queue_success = loop;
}

static void loop_success_set(sat_loop_t *loop)
{
	int i;

	if (loop->is_success)
		return;
	for (i = 0; i < loop_len; i++) {
		if (loop->ca_list[i]->mon.is_active ||
			loop->ca_list[i]->flag == FL_ERASE_LOOP) {
			return;
		}
	}

	event_add_once(loop_success, loop);
}

static int mon_init(monitor_t *mon, int offset, int pos_tbl_sz)
{
	if (!(mon->pos_tbl = calloc(pos_tbl_sz, sizeof(int))))
		return -1;
	mon->pos_tbl_sz = pos_tbl_sz;
	mon->offset = offset;
	mon->is_active = 1;
	return 0;
}

static void mon_uninit(void *o)
{
	monitor_t *mon = (monitor_t *)o;

	if (mon->pos_tbl)
		free(mon->pos_tbl);
	mon->pos_tbl = NULL;
	mon->pos_tbl_sz = 0;
	mon->offset = 0;
	mon->is_active = 0;
}

static void mon_uninit_set(ca_t *ca)
{
	event_add(mon_uninit, &ca->mon);
}

static void mon_deactivate(void *o)
{
	((monitor_t *)o)->is_active = 0;
}

static void mon_deactivate_set(monitor_t *mon)
{
	event_add(mon_deactivate, mon);
}

static ca_t *ca_alloc(int num)
{
	ca_t *ca;
	int i;

	if (!(ca = calloc(num, sizeof(ca_t))))
		return NULL;

	for (i = 0; i < num; i++) {
		ca[i].code = CD_QUIESCENT;
		ca[i].dir = DR_QUIESCENT;
		ca[i].col = CL_QUIESCENT;
		ca[i].flag = FL_QUIESCENT;
		ca[i].id = ID_UNDEFINED;
		ca[i].loop = NULL;
	}

	return ca;
}

static void ca_space_free(ca_t **sp, int hight, int legnth)
{
	int i, j;

	if (!sp)
		return;
	for (i = 0; i < hight; i++) {
		for (j = 0; j < legnth; j++)
			mon_uninit(&sp[i][j].mon);
		free(sp[i]);
	}
	free(sp);
}

static ca_t **ca_space_alloc(int hight, int length)
{
	ca_t **sp;
	int i;

	if (!(sp = calloc(hight, sizeof(ca_t *))))
		return NULL;

	for (i = 0; i < hight && (sp[i] = ca_alloc(length)); i++);

	if (i < hight)
		goto Error;

	for (i = 0; i < hight; i++) {
		int j;

		for (j = 0; j < length; j++) {
			wind_dir_t dir;

			for (dir = DIR_NO; dir <= DIR_NW; dir++) {
				coordinate_euclid_t cell, neighbour;

				cell.n = i;
				cell.m = j;
				coordinate_moore_neighbour(hight, length, &cell,
					dir, &neighbour);
				sp[i][j].neighbours[dir] =
					&sp[neighbour.n][neighbour.m];
			}
		}
	}

	return sp;

Error:
	ca_space_free(sp, i, length);
	return NULL;
}

static int sp_dim_get(int var_num)
{
	return ((2 * var_num) + 1) * ((var_num + 2) / 3) + (6 * var_num) + 2;
}

static void sp_uninit(ca_space_t *s)
{
	ca_space_free(s->sp, s->sp_dim, s->sp_dim);
}

static int is_bound(ca_t *ca)
{
	return ca->dir != DR_QUIESCENT;
}

static void sat_event_loop_clear(void *o)
{
	ca_space_t *s = (ca_space_t *)o;
	int i;

	/* delete all calls to CAs in the event loop */
	for (i = 0; i < s->sp_dim; i++) {
		int j;

		for (j = 0; j < s->sp_dim; j++) {
			ca_t *ca = &s->sp[i][j];

			if (is_bound(ca))
				event_del_all(ca);
		}
	}

	/* delete the call to the printing function */
	event_del_once(s);
}

static void sat_error_handler(void *o)
{
	ca_space_t *s = (ca_space_t *)o;

	sp_uninit(s);
	event_add(s->cb->fail_cb, s->cb->fail_data);
}

static void sat_success_handler(void *o)
{
	ca_space_t *s = (ca_space_t *)o;

	sp_uninit(s);
	event_add(s->cb->success_cb, s->cb->success_data);
}

static int monitor_offset_get(ca_t *ca)
{
	ca_t *nei = pointing_neighbour(ca);

	return nei->mon.offset ? nei->mon.offset + 1 : 1;
}

static int monitor_pos_tbl_sz_get(int offset)
{
	int p = clause_num / loop_len;
	int q = clause_num - p * loop_len;

	return p + (offset <= q ? 1 : 0);
}

static void mon_spread_do(void *o)
{
	ca_t *ca = (ca_t *)o;
	int offset = monitor_offset_get(ca);
	int pos_tbl_sz = monitor_pos_tbl_sz_get(offset);
	ca_t *nei_ahead = ca->neighbours[cadir2dir(ca->dir)];
	ca_t *nei_left =
		ca->neighbours[wind_dir_offset(cadir2dir(ca->dir), -2)];
	ca_t *ca_propogate = is_bound(nei_left) &&
		pointing_neighbour(nei_left) == ca ? nei_left : nei_ahead;

	if (mon_init(&(ca->mon), offset, pos_tbl_sz))
		error_set();
	else
		event_add_once(mon_spread_scan, ca_propogate);
}

static void mon_spread_scan(void *o)
{
	ca_t *ca = (ca_t *)o;
	ca_direction_t sw_dir, sw_dir_bad;

	if (ca->flag == FL_ERASE_LOOP)
		return;

	if (!is_bound(ca) ||
		(!is_bound(ca->neighbours[wind_dir_offset(cadir2dir(ca->dir),
		-2)]) && !is_bound(ca->neighbours[cadir2dir(ca->dir)]))) {
		/* if the cell is not bound or neither the cell ahead of it
		 * and the cell to its left are bound initiate rescanning */
		event_add(mon_spread_init_phase2, o);
		return;
	}

	sw_dir = ca->neighbours[wind_dir_offset(cadir2dir(ca->dir), -3)]->dir;
	sw_dir_bad = dir2cadir(wind_dir_offset(cadir2dir(ca->dir), -2));

	if (ca->mon.pos_tbl || sw_dir == sw_dir_bad ||
		!monitor_pos_tbl_sz_get(monitor_offset_get(ca))) {
		/* do not spread a monitor to the cell if:
		 * - it has already been spread
		 * - the cell is connecting a loop to its replicate
		 * - monitoring of all clauses has already been accounted for */
		return;
	}

	event_add(mon_spread_do, ca);
}

static void mon_spread_init_phase2(void *o)
{
	event_add(mon_spread_scan, o);
}

static void mon_spread_init_phase2_set(void *o)
{
	ca_t *ca = (ca_t *)o;

	event_add(mon_spread_init_phase2, ca->neighbours[cadir2dir(ca->dir)]);
}

static void mon_spread_init_phase1(void *o)
{
	event_add(mon_spread_init_phase2_set, o);
}

static void mon_spread_init_phase1_set(ca_t *ca)
{
	event_add(mon_spread_init_phase1, ca);
}

static int is_mon_danger(monitor_t *mon, int pos, int distance)
{
	return table ? mon->pos_tbl[pos] == (table->record_num - distance) : 0;
}

static int is_mon_fail(monitor_t *mon, int pos)
{
	return is_mon_danger(mon, pos, 1);
}

static int is_mon_allert(monitor_t *mon, int pos)
{
	return is_mon_danger(mon, pos, 2);
}

static ca_monitor_state_t mon_scan(monitor_t *mon, int id, ca_code_t code)
{
	int i;
	ca_monitor_state_t status = MN_NOOP;

	if (!mon->is_active)
		goto Exit;

	for (i = 0; i < mon->pos_tbl_sz; i++) {
		int clause = mon->offset + i*loop_len;

		if (mon->pos_tbl[i] == -1)
			continue;

		if (!table->t[mon->pos_tbl[i]][clause].id &&
			table->t[mon->pos_tbl[i]][clause].tv ==
			SAT_TV_TAUTOLOGY) {
			return MN_FAIL;
		}

		if (table->t[mon->pos_tbl[i]][clause].id != id)
			continue;

		if (table->t[mon->pos_tbl[i]][clause].tv == SAT_TV_PARADOX ||
			table->t[mon->pos_tbl[i]][clause].tv !=
			code2code(truth_values, code)) {
			if (code == CD_ZERO || code == CD_ONE)
				mon->pos_tbl[i] = -1;
			continue;
		}

		if (is_mon_fail(mon, i))
			return MN_FAIL;

		status = is_mon_allert(mon, i) ? MN_ALLERT : MN_ACTIVE;
		mon->pos_tbl[i]++;
	}

	for (i = 0; i < mon->pos_tbl_sz; i++) {
		if (mon->pos_tbl[i] != -1)
			goto Exit;
	}
	return MN_SUCCESS;

Exit:
	return status;
}

static int is_flag_degenerate(ca_flag_t flag)
{
	return flag & (FL_ERASE_LOOP | FL_COLLISION);
}

static int is_neighbour_degenerate(ca_t *ca)
{
	ca_direction_t dir;

	ROTATE(dir, ca, ca->neighbours[cadir2dir(X)]->flag == FL_ERASE_LOOP);
	return dir == DR_UNDEFINED ? 0 : 1;
}

static ca_flag_t is_replicate_arm_retract(ca_t *ca)
{
	ca_direction_t dir;

	/* X is the rotating ca_direction_t */
	ROTATE(dir, ca, ca->neighbours[cadir2dir(X)]->dir == X &&
		ca->neighbours[cadir2dir(X)]->flag == FL_COLLISION);

	return dir;
}

static int is_replication_complete(ca_t *ca)
{
	ca_direction_t dir;

	ROTATE(dir, ca, ca->neighbours[cadir2dir(X)]->code == CD_DETACH ||
		ca->neighbours[cadir2dir(dir_opposite(X))]->code == CD_DETACH);

	return dir == DR_UNDEFINED ? 0 : 1;
}

static int is_arm_extrusion_failure(ca_t *ca)
{
	ca_direction_t dir;

	ROTATE(dir, ca, ca->dir == X && !ca->neighbours[cadir2dir(X)]->dir &&
		ca->neighbours[wind_dir_offset(cadir2dir(X), -2)]->code ==
		CD_FLOW);

	return dir == DR_UNDEFINED ? 0 : 1;
}

/* flag setting functions */
static void flag_set_queiscent(void *o)
{
	((ca_t *)o)->flag = FL_QUIESCENT;
}

static void flag_set_erase_loop(void *o)
{
	((ca_t *)o)->flag = FL_ERASE_LOOP;
}

static void flag_set_collision(void *o)
{
	((ca_t *)o)->flag = FL_COLLISION;
}

static void flag_set_branch_seq(void *o)
{
	((ca_t *)o)->flag = FL_BRANCH_SEQ;
}

static void flag_set_gen_zero(void *o)
{
	((ca_t *)o)->flag = FL_GEN_ZERO;
}

static void flag_set_gen_one(void *o)
{
	((ca_t *)o)->flag = FL_GEN_ONE;
}

static void flag_set_monitor_active(void *o)
{
	((ca_t *)o)->flag = FL_MONITOR_ACTIVE;
}

static void flag_set_monitor_allert(void *o)
{
	((ca_t *)o)->flag = FL_MONITOR_ALLERT;
}

static void flag_set(ca_t *ca, ca_flag_t flag)
{
	event_func_t func;

	switch (flag)
	{
	case FL_QUIESCENT:
		func = flag_set_queiscent;
		break;
	case FL_ERASE_LOOP:
		func = flag_set_erase_loop;
		break;
	case FL_COLLISION:
		func = flag_set_collision;
		break;
	case FL_BRANCH_SEQ:
		func = flag_set_branch_seq;
		break;
	case FL_GEN_ZERO:
		func = flag_set_gen_zero;
		break;
	case FL_GEN_ONE:
		func = flag_set_gen_one;
		break;
	case FL_MONITOR_ACTIVE:
		func = flag_set_monitor_active;
		break;
	case FL_MONITOR_ALLERT:
		func = flag_set_monitor_allert;
		break;
	default:
		return;
	}

	event_add(func, ca);
}

/* flags scanning function */
static void flags_scan(ca_t *ca)
{
	ca_direction_t dir;

	/* if any of the destruction flags are set reset the flag */
	if (is_flag_degenerate(ca->flag)) {
		flag_set(ca, FL_QUIESCENT);
		if (ca->loop)
			loop_fail_set(ca->loop);
		return;
	}

	/* if ca is bound and there is a destruction flag nearby, set the
	 * destruction flag */
	if (ca->dir && is_neighbour_degenerate(ca)) {
		flag_set(ca, FL_ERASE_LOOP);
		return;
	}

	/* if a collision flag nearby copy the retracting flag until reaching a
	 * corner, then set the branch sequence flag */
	dir = is_replicate_arm_retract(ca);
	if (dir != DR_UNDEFINED) {
		wind_dir_t side = wind_dir_offset(cadir2dir(dir), -2);

		if (ca->neighbours[side]->dir == dir)
			flag_set(ca, FL_BRANCH_SEQ);
		else
			flag_set(ca, FL_COLLISION);
		return;
	}

	/* if the replication arm is closing on itself set the flag to mutate a
	 * new one bit */
	if (ca->code && ca->flag & (FL_QUIESCENT | FL_MONITOR_ACTIVE |
		FL_MONITOR_ALLERT) && is_replication_complete(ca)) {
		flag_set(ca, FL_GEN_ZERO);
		return;
	}

	/* arm extrusion failure checking. a failed attempt at new arm
	 * extrusion will result in the FL_BRANCH_SEQ flag being set in the
	 * corner. this allows further attempts at the other directions later */
	if (ca->code == CD_ARM_EXT_END && is_arm_extrusion_failure(ca)) {
		flag_set(ca, FL_BRANCH_SEQ);
		return;
	}

	/* change FL_GEN_ZERO to FL_GEN_ONE after seeing a CD_TURN_LEFT in
	 * order to mutate a new one bit */
	if (ca->flag == FL_GEN_ZERO && ca->code == CD_TURN_LEFT) {
		flag_set(ca, FL_GEN_ONE);
		return;
	}

	/* CD_ARM_EXT_START always clears the flag */
	if (ca->code == CD_ARM_EXT_START) {
		flag_set(ca, FL_QUIESCENT);
		return;
	}

	/* reset FL_GEN_ONE/FL_GEN_ZERO to either FL_QUIESCENT or
	 * FL_BRANCH_SEQ */
	if (ca->flag & (FL_GEN_ZERO | FL_GEN_ONE)) {
		if (pointing_neighbour(ca)->code == CD_FLOW) {
			flag_set(ca, FL_QUIESCENT);
			return;
		}

		if (ca->code == CD_UNEXPLORED_0 ||
			ca->code == CD_UNEXPLORED_1) {
			flag_set(ca, FL_BRANCH_SEQ);
			return;
		}
	}

	if (ca->id) {
		switch (mon_scan(&ca->mon, ca->id, ca->code))
		{
		case MN_SUCCESS:
			mon_deactivate_set(&ca->mon);
			break;
		case MN_ACTIVE:
			flag_set(ca, FL_MONITOR_ACTIVE);
			break;
		case MN_ALLERT:
			flag_set(ca, FL_MONITOR_ALLERT);
			break;
		case MN_FAIL:
			flag_set(ca, FL_ERASE_LOOP);
			break;
		case MN_NOOP:
		/* do nothing */
		default:
			break;
		}
	}
}

/* code set functions */
static void code_set_quiescent(void *o)
{
	((ca_t *)o)->code = CD_QUIESCENT;
	event_add_once(ca_scan, o);
}

static void code_set_flow(void *o)
{
	((ca_t *)o)->code = CD_FLOW;
	event_add_once(ca_scan, o);
}

static void code_set_grow(void *o)
{
	ca_t *ca = (ca_t *)o;
	ca_t *nei_ahead = ca->neighbours[cadir2dir(ca->dir)];

	ca->code = CD_GROW;
	event_add_once(ca_scan, ca);
	event_add_once(ca_scan, nei_ahead);
}

static void code_set_turn_left(void *o)
{
	ca_t *ca = (ca_t *)o;
	ca_t *nei_left = ca->neighbours[wind_dir_offset(cadir2dir(ca->dir),
		-2)];

	ca->code = CD_TURN_LEFT;
	event_add_once(ca_scan, ca);
	event_add_once(ca_scan, nei_left);
}

static void code_set_arm_ext_start(void *o)
{
	ca_t *ca = (ca_t *)o;
	ca_t *nei_ahead = ca->neighbours[cadir2dir(ca->dir)];

	ca->code = CD_ARM_EXT_START;
	event_add_once(ca_scan, ca);
	event_add_once(ca_scan, nei_ahead);
}

static void code_set_arm_ext_end(void *o)
{
	((ca_t *)o)->code = CD_ARM_EXT_END;
	event_add_once(ca_scan, o);
}

static void code_set_detach(void *o)
{
	((ca_t *)o)->code = CD_DETACH;
	event_add_once(ca_scan, o);
}

static void code_set_unexplored_0(void *o)
{
	((ca_t *)o)->code = CD_UNEXPLORED_0;
	event_add_once(ca_scan, o);
}

static void code_set_unexplored_1(void *o)
{
	((ca_t *)o)->code = CD_UNEXPLORED_1;
	event_add_once(ca_scan, o);
}

static void code_set_zero(void *o)
{
	((ca_t *)o)->code = CD_ZERO;
	event_add_once(ca_scan, o);
}

static void code_set_one(void *o)
{
	((ca_t *)o)->code = CD_ONE;
	event_add_once(ca_scan, o);
}

static void code_set_tautology(void *o)
{
	((ca_t *)o)->code = CD_TAUTOLOGY;
	event_add_once(ca_scan, o);
}

static void code_set_paradox(void *o)
{
	((ca_t *)o)->code = CD_PARADOX;
	event_add_once(ca_scan, o);
}

static void code_set(ca_t *ca, ca_code_t code)
{
	event_func_t func;

	switch (code)
	{
	case CD_QUIESCENT:
		func = code_set_quiescent;
		break;
	case CD_FLOW:
		func = code_set_flow;
		break;
	case CD_GROW:
		func = code_set_grow;
		break;
	case CD_TURN_LEFT:
		func = code_set_turn_left;
		break;
	case CD_ARM_EXT_START:
		func = code_set_arm_ext_start;
		break;
	case CD_ARM_EXT_END:
		func = code_set_arm_ext_end;
		break;
	case CD_DETACH:
		func = code_set_detach;
		break;
	case CD_UNEXPLORED_0:
		func = code_set_unexplored_0;
		break;
	case CD_UNEXPLORED_1:
		func = code_set_unexplored_1;
		break;
	case CD_ZERO:
		func = code_set_zero;
		break;
	case CD_ONE:
		func = code_set_one;
		break;
	case CD_TAUTOLOGY:
		func = code_set_tautology;
		break;
	case CD_PARADOX:
		func = code_set_paradox;
		break;
	default:
		return;
	}

	event_add(func, ca);
}

/* direction set functions */
static void dir_set_quiescent(void *o)
{
	((ca_t *)o)->dir = DR_QUIESCENT;
	event_add_once(ca_scan, o);
}

static void dir_set_up(void *o)
{
	((ca_t *)o)->dir = DR_UP;
	event_add_once(ca_scan, o);
}

static void dir_set_down(void *o)
{
	((ca_t *)o)->dir = DR_DOWN;
	event_add_once(ca_scan, o);
}

static void dir_set_left(void *o)
{
	((ca_t *)o)->dir = DR_LEFT;
	event_add_once(ca_scan, o);
}

static void dir_set_right(void *o)
{
	((ca_t *)o)->dir = DR_RIGHT;
}

static void dir_set(ca_t *ca, ca_direction_t dir)
{
	event_func_t func;

	switch (dir)
	{
	case DR_QUIESCENT:
		func = dir_set_quiescent;
		break;
	case DR_UP:
		func = dir_set_up;
		break;
	case DR_DOWN:
		func = dir_set_down;
		break;
	case DR_LEFT:
		func = dir_set_left;
		break;
	case DR_RIGHT:
		func = dir_set_right;
		break;
	default:
		return;
	}

	event_add(func, ca);
}

/* colour set functions */
static void col_set_quiescent(void *o)
{
	((ca_t *)o)->col = CL_QUIESCENT;
	event_add_once(ca_scan, o);
}

static void col_set_white(void *o)
{
	((ca_t *)o)->col = CL_WHITE;
	event_add_once(ca_scan, o);
}

static void col_set_yellow(void *o)
{
	((ca_t *)o)->col = CL_YELLOW;
	event_add_once(ca_scan, o);
}

static void col_set_blue(void *o)
{
	((ca_t *)o)->col = CL_BLUE;
	event_add_once(ca_scan, o);
}

static void col_set_red(void *o)
{
	((ca_t *)o)->col = CL_RED;
	event_add_once(ca_scan, o);
}

static void col_set(ca_t *ca, ca_code_t col)
{
	event_func_t func;

	switch (col)
	{
	case CL_QUIESCENT:
		func = col_set_quiescent;
		break;
	case CL_WHITE:
		func = col_set_white;
		break;
	case CL_YELLOW:
		func = col_set_yellow;
		break;
	case CL_BLUE:
		func = col_set_blue;
		break;
	case CL_RED:
		func = col_set_red;
		break;
	default:
		return;
	}

	event_add(func, ca);
}

static void id_reset(void *o)
{
	((ca_t *)o)->id = ID_UNDEFINED;
	event_add_once(ca_scan, o);
}

static void id_inc(void *o)
{
	((ca_t *)o)->id++;
	event_add_once(ca_scan, o);
}

static int is_code(ca_t *ca)
{
	return ca->code != CD_QUIESCENT;
}

static int is_code_detach(ca_t *ca)
{
	return ca->code == CD_DETACH;
}

static int is_code_arm_ext_start(ca_t *ca)
{
	return ca->code == CD_ARM_EXT_START;
}

static int is_code_unexplored_0(ca_t *ca)
{
	return ca->code == CD_UNEXPLORED_0;
}

static int is_code_unexplored_1(ca_t *ca)
{
	return ca->code == CD_UNEXPLORED_1;
}

static int is_pointing_neighbour_grow(ca_t *ca)
{
	return pointing_neighbour(ca)->code == CD_GROW;
}

static int is_destruct_detach(ca_t *ca)
{
	ca_direction_t dir;

	ROTATE(dir, ca, ca->neighbours[cadir2dir(X)]->flag == FL_GEN_ZERO);
	return dir == DR_UNDEFINED ? 0 : 1;

}

static int is_loop_close(ca_t *ca)
{
	ca_direction_t dir;

	ROTATE(dir, ca, ca->dir == X &&
		ca->neighbours[wind_dir_offset(cadir2dir(X), -3)]->dir ==
		dir2cadir(wind_dir_offset(cadir2dir(X), -2)) &&
		ca->neighbours[wind_dir_offset(cadir2dir(X), -3)]->code !=
		CD_QUIESCENT &&
		ca->neighbours[wind_dir_offset(cadir2dir(X), -1)]->dir ==
		dir2cadir(wind_dir_offset(cadir2dir(X), 2)) &&
		ca->neighbours[wind_dir_offset(cadir2dir(X), -1)]->code !=
		CD_QUIESCENT);
	return dir == DR_UNDEFINED ? 0 : 1;
}

static int is_pointing_neighbour_detach(ca_t *ca)
{
	return pointing_neighbour(ca)->code == CD_DETACH;
}

static int is_gen_arm_extention(ca_t *ca)
{
	return ca->code != CD_FLOW && ca->code != CD_ARM_EXT_START &&
		ca->flag == FL_BRANCH_SEQ && pointing_neighbour(ca)->code ==
		CD_FLOW;
}

static int is_arm_ext_start_corner(ca_t *ca)
{
	ca_direction_t dir;

	ROTATE(dir, ca, ca->dir == X && pointing_neighbour(ca)->code ==
		CD_ARM_EXT_START && cadir2dir(pointing_neighbour(ca)->dir) ==
		wind_dir_offset(cadir2dir(ca->dir), 2) &&
		pointing_neighbour(pointing_neighbour(ca))->code ==
		CD_ARM_EXT_END);

	return dir == DR_UNDEFINED ? 0 : 1;
}

static int is_arm_ext_end_corner(ca_t *ca)
{
	return ca->code == CD_FLOW && pointing_neighbour(ca)->code ==
		CD_ARM_EXT_END;
}

static int is_flag_gen_zero(ca_t *ca)
{
	return ca->flag == FL_GEN_ZERO;
}

static int is_flag_gen_one(ca_t *ca)
{
	return ca->flag == FL_GEN_ONE;
}

static void id_set(ca_t *ca, int id)
{
	if (id == ca->id)
		return;

	event_add(id == ca->id + 1 ? id_inc : id_reset, ca);
}

static void set_scan(void *o)
{
	event_add_once(ca_scan, o);
}

static void bound_rules_scan(ca_t *ca)
{
	ca_t *neighbour = pointing_neighbour(ca);

	/* if any of the destruction flags is set, reset all fields */
	if (is_flag_degenerate(ca->flag)) {
		code_set(ca, CD_QUIESCENT);
		dir_set(ca, DR_QUIESCENT);
		col_set(ca, CL_QUIESCENT);
		id_set(ca, ID_UNDEFINED);
		mon_uninit_set(ca);
		return;
	}

	/* CD_QUIESCENT state changes to CD_FLOW when seeing signal CD_GROW */
	if (!is_code(ca)) {
		if (is_pointing_neighbour_grow(ca))
			code_set(ca, CD_FLOW);
		else
			event_add(set_scan, ca);
		return;
	}

	/* if a CD_DETACH sees a FL_GEN_ZERO nearby it disappears */
	if (is_code_detach(ca) && is_destruct_detach(ca)) {
		code_set(ca, CD_QUIESCENT);
		dir_set(ca, DR_QUIESCENT);
		col_set(ca, CL_QUIESCENT);
		return;
	}

	/* if a closing loop is detected, set the CD_DETACH code */
	if (is_loop_close(ca)) {
		code_set(ca, CD_DETACH);
		id_set(ca, ID_UNDEFINED);
		return;
	}

	/* close the child loop */
	if (is_pointing_neighbour_detach(ca)) {
		ca_direction_t dir =
			dir2cadir(wind_dir_offset(cadir2dir(ca->dir), 2));
		ca_code_t code =
			ca->neighbours[wind_dir_offset(cadir2dir(ca->dir),
			-2)]->code;

		dir_set(ca, dir);
		code_set(ca, code);
		loop_new_set(ca);
		return;
	}

	/* generate an arm extension sequence on seeing a FL_BRANCH_SEQ */
	if (is_gen_arm_extention(ca)) {
		code_set(ca, CD_ARM_EXT_START);
		id_set(ca, ID_UNDEFINED);
		return;
	}

	/* completing the arm extension sequence */
	if (is_code_arm_ext_start(ca)) {
		code_set(ca, CD_ARM_EXT_END);
		return;
	}

	/* do not copy CD_ARM_EXT_START round corners */
	if (is_arm_ext_start_corner(ca)) {
		code_set(ca, CD_FLOW);
		id_set(ca, ID_UNDEFINED);
		return;
	}

	/* do not copy CD_ARM_EXT_END round corners */
	if (is_arm_ext_end_corner(ca)) {
		code_set(ca, CD_FLOW);
		return;
	}

	/* convert CD_UNEXPLORED_0/CD_UNEXPLORED_1 to CD_ZERO/CD_ONE */
	if (is_flag_gen_zero(neighbour)) {
		ca_code_t code;

		if (is_code_unexplored_0(neighbour))
			code = CD_ZERO;
		else if (is_code_unexplored_1(neighbour))
			code = CD_ONE;
		else
			code = neighbour->code;

		code_set(ca, code);
		id_set(ca, neighbour->id);
		return;
	}

	if (is_flag_gen_one(neighbour)) {
		ca_code_t code;

		if (is_code_unexplored_0(neighbour))
			code = CD_ONE;
		else if (is_code_unexplored_1(neighbour))
			code = CD_ZERO;
		else
			code = neighbour->code;

		code_set(ca, code);
		id_set(ca, neighbour->id);
		return;
	}

	if (!ca->mon.is_active && (ca->code == CD_ZERO || ca->code == CD_ONE) &&
		pointing_neighbour(ca)->code == CD_FLOW) {
		loop_success_set(ca->loop);
	}
	code_set(ca, neighbour->code);
	id_set(ca, neighbour->id);
}

static ca_direction_t is_neighbour_grow(ca_t *ca)
{
	ca_direction_t dir;

	ROTATE(dir, ca, ca->neighbours[cadir2dir(X)]->code == CD_GROW &&
		ca->neighbours[cadir2dir(X)]->dir == dir_opposite(X) &&
		cadir2dir(ca->neighbours[wind_dir_offset(cadir2dir(X),
		1)]->dir) != wind_dir_offset(cadir2dir(X), 2) &&
		(ca->neighbours[cadir2dir(X)]->flag == FL_QUIESCENT ||
		ca->neighbours[cadir2dir(X)]->flag == FL_MONITOR_ACTIVE));

	return dir_opposite(dir);
}

static int is_dir_well_defined(ca_direction_t dir)
{
	return dir != DR_QUIESCENT && dir != DR_UNDEFINED ;
}

static int is_collision(ca_t *ca, ca_direction_t dir)
{
	return is_bound(ca->neighbours[cadir2dir(dir)]) &&
		ca->neighbours[cadir2dir(dir)]->col !=
		ca->neighbours[cadir2dir(dir_opposite(dir))]->col;
}

static int is_arm_extend(ca_t *ca)
{
	ca_direction_t dir;

	ROTATE(dir, ca,
		ca->neighbours[cadir2dir(X)]->code == CD_ARM_EXT_START &&
		ca->neighbours[cadir2dir(X)]->dir == dir_opposite(X) &&
		ca->neighbours[cadir2dir(dir_opposite(X))]->dir ==
		DR_QUIESCENT &&
		(ca->neighbours[cadir2dir(X)]->flag == FL_QUIESCENT ||
		ca->neighbours[cadir2dir(X)]->flag == FL_MONITOR_ACTIVE));

	return dir_opposite(dir);
}

static int is_turn_left(ca_t *ca)
{
	ca_direction_t dir;

	ROTATE(dir, ca, ca->neighbours[cadir2dir(X)]->code == CD_TURN_LEFT &&
		cadir2dir(ca->neighbours[cadir2dir(X)]->dir) ==
		wind_dir_offset(cadir2dir(X), -2) &&
		ca->neighbours[wind_dir_offset(cadir2dir(X), -1)]->dir ==
		DR_QUIESCENT &&
		(ca->neighbours[cadir2dir(X)]->flag == FL_QUIESCENT ||
		ca->neighbours[cadir2dir(X)]->flag == FL_MONITOR_ACTIVE));

	return dir_opposite(dir);
}

static void unbound_rules_scan(ca_t *ca)
{
	ca_direction_t dir;
	code2code_t dir2col[] = {
		{DR_UP, CL_WHITE},
		{DR_RIGHT, CL_RED},
		{DR_DOWN, CL_YELLOW},
		{DR_LEFT, CL_BLUE},
		{-1}
	};

	/* CD_QUIESCENT changes to CD_FLOW when there's a CD_GROW nearby */
	if (is_dir_well_defined(dir = is_neighbour_grow(ca))) {
		dir_set(ca, dir);

		if (is_collision(ca, dir)) {
			flag_set(ca, FL_COLLISION);
		}
		else {
			code_set(ca, CD_FLOW);
			col_set(ca, ca->neighbours[
				cadir2dir(dir_opposite(dir))]->col);
		}
		return;
	}

	/* initiate an arm extension */
	if (is_dir_well_defined(dir = is_arm_extend(ca))) {
		dir_set(ca, dir);
		code_set(ca, CD_GROW);
		col_set(ca, code2code(dir2col, dir));
		mon_spread_init_phase1_set(ca);
		return;
	}

	/* turn the flowing direction due to CD_TURN_LEFT */
	if (is_dir_well_defined(dir = is_turn_left(ca))) {
		dir_set(ca, dir);

		if (is_collision(ca, dir)) {
			flag_set(ca, FL_COLLISION);
		}
		else {
			col_set(ca, ca->neighbours[
				cadir2dir(dir_opposite(dir))]->col);
		}
		return;
	}
}

static void ca_scan(void *o)
{
	ca_t *ca = (ca_t *)o;

	flags_scan(ca);
	if (is_bound(ca))
		bound_rules_scan(ca);
	else
		unbound_rules_scan(ca);
}

static ca_t *loop_assignment_get_next(void)
{
	ca_t **ca;

	for (ca = loop_queue_success->ca_list; (*ca)->code != CD_TURN_LEFT;
		ca++);
	return pointing_neighbour(*ca);
}

static int assignments_set(void)
{
	int i;
	sat_loop_t *queue;

	if (!loop_queue_success)
		return 0;

	for (queue = loop_queue_success, i = 0; queue; queue = queue->next,
		i++);
	if (table_assignment_init(table, i))
		return -1;

	i = 0;
	while (loop_queue_success) {
		int id;
		ca_t *ca = loop_assignment_get_next();

		for (id = 1; id <= table->var_num; id++) {
			table_assignment_insert(table, i, id,
				code2code(truth_values, ca->code));
			ca = pointing_neighbour(ca);
		}

		i++;
		queue = loop_queue_success;
		loop_queue_success = loop_queue_success->next;
		loop_del(queue);
	}
	return 0;
}

static void sat_epilogue(void *o)
{
	ca_space_t *s = (ca_space_t *)o;

	if (s->print_field != SAT_PRINT_FIELD_NONE)
		sat_print_uninit();

	event_add(assignments_set() ? sat_error_handler : sat_success_handler,
		o);
}

static void sig_loop_cb(sig_t sig, void *o)
{
	if (loop_queue_active)
		return;
	sat_event_loop_clear(o);
	event_add(sat_epilogue, o);
}

static void sig_error_cb(sig_t s, void *o)
{
	sat_event_loop_clear(o);
	event_add(sat_error_handler, o);
}

static void sp_initial_configuration(void *o)
{
	ca_space_t *s = (ca_space_t *)o;
	int i = 0, vars = table->var_num, id = 1, x_offset, y_offset;

	/* initial loop's bottom left coordinates */
	s->x0 = ((s->sp_dim / (2 * (s->loop_dim + 1))) - 1) * (s->loop_dim + 1);
	s->y0 = s->sp_dim - 1 - s->x0;

	/* set turn left code in bottom left corner and initiate monitor
	 * spreading */
	s->sp[s->y0][s->x0].code = CD_TURN_LEFT;
	s->sp[s->y0][s->x0].dir = DR_DOWN;
	s->sp[s->y0][s->x0].col = CL_RED;
	event_add_once(ca_scan, &s->sp[s->y0][s->x0]);
	event_add_once(mon_spread_scan, &s->sp[s->y0][s->x0]);
	loop_new_set(&s->sp[s->y0][s->x0]);

	/* set grow and loop extension flow codes on the bottom */
	for (i = 1; i < s->loop_dim + 2; i++) {
		x_offset = s->x0 + i;

		s->sp[s->y0][x_offset].code = i < s->loop_dim ?
			CD_GROW : CD_FLOW;
		s->sp[s->y0][x_offset].dir = DR_RIGHT;
		s->sp[s->y0][x_offset].col = CL_RED;
		event_add_once(ca_scan, &s->sp[s->y0][x_offset]);
	}
	event_add_once(mon_spread_scan, &s->sp[s->y0][s->x0 + s->loop_dim + 1]);

	/* set loop unexplored and flow codes to the left */
	for (i = 1; i < s->loop_dim; i++) {
		y_offset = s->y0 - i;

		if (vars) {
			s->sp[y_offset][s->x0].code = CD_UNEXPLORED_0;
			s->sp[y_offset][s->x0].id = id;
			s->sp[y_offset][s->x0].col = CL_RED;
			id++;
			vars--;
		}
		else {
			s->sp[y_offset][s->x0].code = CD_FLOW;
		}
		s->sp[y_offset][s->x0].dir = y_offset ==
			s->y0 - s->loop_dim + 1 ? DR_LEFT : DR_DOWN;
		s->sp[y_offset][s->x0].col = CL_RED;
		event_add_once(ca_scan, &s->sp[y_offset][s->x0]);
	}

	/* set loop unexplored and flow codes on the top */
	y_offset = s->y0 - s->loop_dim + 1;
	for (i = 1; i < s->loop_dim; i++) {
		x_offset = s->x0 + i;

		if (vars) {
			s->sp[y_offset][x_offset].code = CD_UNEXPLORED_0;
			s->sp[y_offset][x_offset].id = id;
			id++;
			vars--;
		}
		else {
			s->sp[y_offset][x_offset].code = CD_FLOW;
		}
		s->sp[y_offset][x_offset].dir = i == s->loop_dim - 1 ?
			DR_UP : DR_LEFT;
		s->sp[y_offset][x_offset].col = CL_RED;
		event_add_once(ca_scan, &s->sp[y_offset][x_offset]);
	}

	/* set loop unexplored and flow codes to the right */
	x_offset = s->x0 + s->loop_dim - 1;
	for (i = 0; i < s->loop_dim - 2; i++) {
		y_offset = s->y0 - s->loop_dim + 2 + i;
		if (vars) {
			s->sp[y_offset][x_offset].code = CD_UNEXPLORED_0;
			s->sp[y_offset][x_offset].id = id;
			id++;
			vars--;
		}
		else {
			s->sp[y_offset][x_offset].code = CD_FLOW;
		}
		s->sp[y_offset][x_offset].dir = DR_UP;
		s->sp[y_offset][x_offset].col = CL_RED;
		event_add_once(ca_scan, &s->sp[y_offset][x_offset]);

		if (vars)
			vars--;
	}
}

static void sat_print_monitor_params_get(ca_space_t *s, int i, int j,
	sat_print_t *printer)
{
	monitor_t mon = s->sp[i][j].mon;
	int is_pulse = loop_queue_active && (s->sp[i][j].code == CD_TURN_LEFT);

	if (mon.is_active) {
		int i, cur = 0, pos = 0;

		for (i = 0; i < mon.pos_tbl_sz; i++) {
			if (mon.pos_tbl[i] <= pos)
				continue;
			pos = mon.pos_tbl[i];
			cur = i;
		}

		printer->representation = ASCII_ONE + pos;
		printer->colour = is_mon_fail(&mon, cur) ? COL_RED : COL_GREEN;
		printer->is_bright = is_pulse ? ATTR_BRIGHT : ATTR_DULL;
	}
	else {
		int idx;

		if (!is_bound(&s->sp[i][j])) {
			idx = CD_QUIESCENT;
			is_pulse = 0;
		}
		else {
			idx = CD_FLOW;
		}

		printer->representation = sat_print_code[idx].representation;
		printer->colour = sat_print_code[idx].colour;
		printer->is_bright =
			(sat_print_code[idx].is_bright + is_pulse) % 2;
	}
}

void sat_print_params_get(void *o, int i, int j, sat_print_t *printer)
{
	ca_space_t *s = (ca_space_t *)o;
	sat_print_t *table;
	int idx, is_pulse = 0;

	switch (s->print_field)
	{
	case SAT_PRINT_FIELD_CODE:
		table = sat_print_code;
		idx = s->sp[i][j].code;
		break;
	case SAT_PRINT_FIELD_DIR:
		table = sat_print_dir;
		idx = s->sp[i][j].dir;
		if (s->sp[i][j].code == CD_TURN_LEFT)
			is_pulse = 1;
		break;
	case SAT_PRINT_FIELD_FLAG:
		table = sat_print_flag;
		if (s->sp[i][j].flag == FL_QUIESCENT) {
			if (is_bound(&s->sp[i][j])) {
				idx = IDX_QUIESCENT_BOUND;
				if (s->sp[i][j].code == CD_TURN_LEFT)
					is_pulse = 1;
			}
			else {
				idx = IDX_QUIESCENT;
			}
		}
		else {
			idx = code2code(flag2idx, s->sp[i][j].flag);
		}
		break;
	case SAT_PRINT_FIELD_COLOUR:
		table = sat_print_colour;
		idx = s->sp[i][j].col;
		if (s->sp[i][j].code == CD_TURN_LEFT)
			is_pulse = 1;
		break;
	case SAT_PRINT_FIELD_MONITOR:
		sat_print_monitor_params_get(s, i, j, printer);
		return;
	default:
		return;
	}

	if (!loop_queue_active)
		is_pulse = 0;
	printer->representation = table[idx].representation;
	printer->colour = table[idx].colour;
	printer->is_bright = (table[idx].is_bright + is_pulse) % 2;
}

void sp_init(ca_space_cb_t *cb, table_t *tbl, sat_print_field_t print_field,
	sat_print_speed_t print_speed)
{
	ca_space.cb = cb;
	ca_space.sp_dim = sp_dim_get(tbl->var_num);
	ca_space.loop_dim = (tbl->var_num + 8) / 3;
	ca_space.print_field = print_field;
	if (!(ca_space.sp = ca_space_alloc(ca_space.sp_dim, ca_space.sp_dim))) {
		event_add(sat_error_handler, &ca_space);
		return;
	}

	table = tbl;
	loop_len = 4 * (ca_space.loop_dim - 1);
	clause_num = table->record_dim - 1;

	signal_register(SIG_LOOP, sig_loop_cb, &ca_space);
	signal_register(SIG_ERROR, sig_error_cb, &ca_space);

	event_add(sp_initial_configuration, &ca_space);
	if (ca_space.print_field != SAT_PRINT_FIELD_NONE)
		sat_print_init(&ca_space, ca_space.sp_dim, print_speed);
}

