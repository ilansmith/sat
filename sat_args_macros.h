#undef ARG_START
#undef ARG_ENTRY
#undef ARG_END

#ifdef _ARG_TABLE_IDX_DECLARE_
#define ARG_START
#define ARG_ENTRY(name, c, icp) extern int IDX_ ## name;
#define ARG_END
#endif

#ifdef _ARG_TABLE_IDX_DEFINE_
#define ARG_START
#define ARG_ENTRY(name, c, icp) int IDX_ ## name;
#define ARG_END
#endif

#ifdef _ARG_TABLE_DECLARE_
#define ARG_START
#define ARG_ENTRY(name, c, icp) {0, 0, 0},
#define ARG_END
#endif

#ifdef _ARG_TABLE_ENTRIES_INIT_
#define ARG_START \
	do { \
		int i = 0;
#define ARG_ENTRY(name, c, icp) \
		IDX_ ## name = i; \
		sat_opts[i].opt = c; \
		sat_opts[i].flag = 1<<i; \
		i++;
#define ARG_END \
	} while (0);

#endif

#ifdef _ARG_TABLE_INCOMPAT_INIT_
#define ARG_START \
	do { \
		int i = 0;
#define ARG_ENTRY(name, c, icp) \
		sat_opts[i].incompat = (icp) & ~sat_opts[i].flag; \
		i++;
#define ARG_END \
	} while (0);
#endif

