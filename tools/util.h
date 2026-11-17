#ifndef UTIL_H
#define UTIL_H

#include <stddef.h>
#include <time.h>

typedef struct kvmap kvmap;
typedef struct strvec strvec;

/** Allocate an empty string map. Returns 0, or -1 on allocation failure. */
int kv_new(kvmap **out);
/** Insert or replace a copied value. Returns 0, or -1 on allocation failure. */
int kv_put(kvmap *m, const char *key, const char *value);
/** Return the value for key, or NULL if the map or the key is missing. */
const char *kv_get(kvmap *m, const char *key);
/** Copy every entry from src into dst. A NULL src is a no-op. Returns 0 or -1. */
int kv_merge(kvmap *dst, kvmap *src);
/** Free the map and its entries. NULL is a no-op. */
void kv_free(kvmap *m);

/** Load a flat JSON object of strings into a new map. Returns 0 or -1. */
int json_load_mem(const char *text, kvmap **out);

/** Replace @@key@@ placeholders from the map. Unknown keys are left intact. */
int subst_apply(const char *in, kvmap *m, char **out);

/** Placeholder replaced by the contents of a file. */
typedef struct inject_item {
	/** Placeholder name, matched as @@key@@ on its own line. */
	const char *key;
	/** File whose contents replace that line. */
	const char *path;
} inject_item;

/** Replace lines that contain an @@key@@ placeholder with the named file. */
int inject_apply(const char *in, const inject_item *items, size_t n, char **out);

/** Format a YYYY-MM-DD date for ca, es or en. Returns 0 or -1. */
int format_date_ymd(const char *lang, const char *ymd, char *out, size_t outsz);
/** UTC year of t, or -1 if the time cannot be converted. */
int year_from_time(time_t t);

/**
 * Build the sblg article prelude and set body_off to the markdown body.
 * Returns 0, or -1 if lang or the date is not usable.
 */
int article_prelude(const char *text, size_t len, const char *source,
	const char *lang, char **prelude, size_t *body_off);

/** Turn sblg tag spans into links under /lang/slug/tag/. */
int linkify(const char *in, const char *lang, const char *slug, char **out);

/** Allocate an empty string vector. */
strvec *strvec_new(void);
/** Append a copy of s. */
void strvec_push(strvec *v, const char *s);
/** Number of entries. A NULL vector has length 0. */
size_t strvec_len(const strvec *v);
/** Pointer to entry i. The pointer is owned by the vector. */
char *strvec_get(const strvec *v, size_t i);
/** Sort entries in byte order. */
void strvec_sort(strvec *v);
/** Free the vector and its entries. NULL is a no-op. */
void strvec_free(strvec *v);
/** Run the vector as an argv and wait. Returns 0 if the command exits 0. */
int strvec_spawn(const strvec *v);
/** Like strvec_spawn, and capture stdout. */
int strvec_capture(const strvec *v, char **out, size_t *outlen);

/**
 * Collect the second tab-separated field of each line.
 * Lines without a tab contribute the whole line. Duplicates are dropped.
 */
int tags_collect(const char *text, strvec **out);

/** Read a file into a NUL-terminated buffer. Returns NULL on error. */
char *read_file(const char *path, size_t *len);
/** Write the whole buffer to a new file. Returns 0 or -1. */
int write_file(const char *path, const void *data, size_t len);
/** Write the whole buffer to fd, retrying EINTR. Returns 0 or -1. */
int write_full(int fd, const void *data, size_t len);
/** Create path and any missing parents. Returns 0 or -1. */
int mkdir_p(const char *path);
/** Copy a file, symlink or directory tree. Returns 0 or -1. */
int copy_tree(const char *src, const char *dst);
/** Delete a file or directory tree. A missing path returns 0. */
int rm_rf(const char *path);

/** fork/exec argv and wait. Returns 0 only when the child exits 0. */
int spawn_wait(char *const argv[]);
/** Like spawn_wait, and capture stdout into a NUL-terminated buffer. */
int spawn_capture(char *const argv[], char **out, size_t *outlen);
/** Split cmdline on spaces, append extra if not NULL, and capture stdout. */
int run_cmd(const char *cmdline, const char *extra, char **out, size_t *outlen);

/** Print a formatted message to stderr and exit with status 1. */
__attribute__((noreturn)) void die(const char *fmt, ...);

/* Flush llvm-cov profile before exec or _exit. No-op without -DCOVERAGE. */
#if defined(__clang__) && defined(COVERAGE)
void __llvm_profile_write_file(void);
#define PROFILE_FLUSH() __llvm_profile_write_file()
#else
#define PROFILE_FLUSH() ((void)0)
#endif

#endif
