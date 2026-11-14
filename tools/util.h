#ifndef UTIL_H
#define UTIL_H

#include <stddef.h>
#include <time.h>

typedef struct kvmap kvmap;
typedef struct strvec strvec;

int kv_new(kvmap **out);
int kv_put(kvmap *m, const char *key, const char *value);
const char *kv_get(kvmap *m, const char *key);
int kv_merge(kvmap *dst, kvmap *src);
void kv_free(kvmap *m);

int json_load_mem(const char *text, kvmap **out);

int subst_apply(const char *in, kvmap *m, char **out);

typedef struct {
	const char *key;
	const char *path;
} inject_item;

int inject_apply(const char *in, const inject_item *items, size_t n, char **out);

int format_date_ymd(const char *lang, const char *ymd, char *out, size_t outsz);
int year_from_time(time_t t);

int article_prelude(const char *text, size_t len, const char *source,
	const char *lang, char **prelude, size_t *body_off);

int linkify(const char *in, const char *lang, const char *slug, char **out);

strvec *strvec_new(void);
void strvec_push(strvec *v, const char *s);
size_t strvec_len(const strvec *v);
char *strvec_get(const strvec *v, size_t i);
void strvec_sort(strvec *v);
void strvec_free(strvec *v);
int strvec_spawn(const strvec *v);
int strvec_capture(const strvec *v, char **out, size_t *outlen);

int tags_collect(const char *text, strvec **out);

char *read_file(const char *path, size_t *len);
int write_file(const char *path, const void *data, size_t len);
int write_full(int fd, const void *data, size_t len);
int mkdir_p(const char *path);
int copy_tree(const char *src, const char *dst);
int rm_rf(const char *path);

int spawn_wait(char *const argv[]);
int spawn_capture(char *const argv[], char **out, size_t *outlen);
int run_cmd(const char *cmdline, const char *extra, char **out, size_t *outlen);

void die(const char *fmt, ...);

#endif
