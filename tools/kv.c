#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif

#include "util.h"

#define STB_DS_IMPLEMENTATION
#include "stb_ds.h"

#include <cjson/cJSON.h>

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct kvmap {
	struct {
		char *key;
		char *value;
	} *tab;
};

__attribute__((noreturn)) void die(const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);
	fputc('\n', stderr);
	exit(1);
}

int kv_new(kvmap **out)
{
	kvmap *m = calloc(1, sizeof *m);

	if (!m)
		return -1;
	sh_new_strdup(m->tab);
	*out = m;
	return 0;
}

int kv_put(kvmap *m, const char *key, const char *value)
{
	char *copy = strdup(value);
	ptrdiff_t i;

	if (!copy)
		return -1;
	i = shgeti(m->tab, key);
	if (i >= 0) {
		free(m->tab[i].value);
		m->tab[i].value = copy;
		return 0;
	}
	shput(m->tab, key, copy);
	return 0;
}

const char *kv_get(kvmap *m, const char *key)
{
	ptrdiff_t i;

	if (!m || !m->tab)
		return NULL;
	i = shgeti(m->tab, key);
	if (i < 0)
		return NULL;
	return m->tab[i].value;
}

int kv_merge(kvmap *dst, kvmap *src)
{
	ptrdiff_t i;

	if (!src)
		return 0;
	for (i = 0; i < shlen(src->tab); i++) {
		if (kv_put(dst, src->tab[i].key, src->tab[i].value) != 0)
			return -1;
	}
	return 0;
}

void kv_free(kvmap *m)
{
	ptrdiff_t i;

	if (!m)
		return;
	for (i = 0; i < shlen(m->tab); i++)
		free(m->tab[i].value);
	shfree(m->tab);
	free(m);
}

static int json_reject(cJSON *root, kvmap *m)
{
	kv_free(m);
	cJSON_Delete(root);
	return -1;
}

int json_load_mem(const char *text, kvmap **out)
{
	cJSON *root;
	cJSON *it;
	kvmap *m = NULL;

	*out = NULL;
	if (!text)
		return -1;
	root = cJSON_Parse(text);
	if (!root || !cJSON_IsObject(root))
		return json_reject(root, NULL);
	if (kv_new(&m) != 0)
		return json_reject(root, NULL);
	for (it = root->child; it; it = it->next) {
		if (!cJSON_IsString(it) || !it->string)
			return json_reject(root, m);
		if (kv_put(m, it->string, it->valuestring) != 0)
			return json_reject(root, m);
	}
	cJSON_Delete(root);
	*out = m;
	return 0;
}
