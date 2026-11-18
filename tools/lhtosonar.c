#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif

#include "util.h"

#include <cjson/cJSON.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define LH_DIR "build/report/lh"
#define CI_RESULT_DESKTOP "build/report/lh/desktop/ci-result.json"
#define CI_RESULT_MOBILE "build/report/lh/mobile/ci-result.json"
#define OUT_PATH "build/report/lh-sonar-issues.json"
#define DEFAULT_FILE "templates/index.html"

struct cat_impact {
	const char *cat;
	const char *quality;
	const char *severity;
};

static const struct cat_impact IMPACTS[] = {
	{ "performance", "RELIABILITY", "INFO" },
	{ "accessibility", "MAINTAINABILITY", "HIGH" },
	{ "best-practices", "SECURITY", "MEDIUM" },
	{ "seo", "MAINTAINABILITY", "MEDIUM" },
};

static const char *PAGE_MARKERS[] = {
	"/educ", "/experi", "/project", "/proyect",
};

static const char *blog_file(const char *path)
{
	if (strstr(path, "/post/"))
		return "templates/post.html";
	if (strstr(path, "/tag/"))
		return "templates/tag.html";
	return "templates/blog.html";
}

static int path_has_any(const char *path, const char *const *markers, size_t n)
{
	size_t i;

	for (i = 0; i < n; i++) {
		if (strstr(path, markers[i]))
			return 1;
	}
	return 0;
}

static const char *file_for_path(const char *path)
{
	if (!path)
		return DEFAULT_FILE;
	if (strstr(path, "/blog/"))
		return blog_file(path);
	/* /ca/, /es/, /en/ index or section pages */
	if (path_has_any(path, PAGE_MARKERS,
	    sizeof PAGE_MARKERS / sizeof PAGE_MARKERS[0]))
		return "templates/page.html";
	return DEFAULT_FILE;
}

static void impact_for_cat(const char *cat, const char **quality, const char **severity)
{
	size_t i;

	*quality = "MAINTAINABILITY";
	*severity = "MEDIUM";
	for (i = 0; i < sizeof IMPACTS / sizeof IMPACTS[0]; i++) {
		if (strcmp(IMPACTS[i].cat, cat) == 0) {
			*quality = IMPACTS[i].quality;
			*severity = IMPACTS[i].severity;
			return;
		}
	}
}

static cJSON *find_rule(cJSON *rules, const char *id)
{
	cJSON *it;

	cJSON_ArrayForEach(it, rules) {
		const cJSON *rid;

		rid = cJSON_GetObjectItemCaseSensitive(it, "id");
		if (cJSON_IsString(rid) && rid->valuestring
		    && strcmp(rid->valuestring, id) == 0)
			return it;
	}
	return NULL;
}

static const char *std_severity(const char *severity)
{
	if (strcmp(severity, "INFO") == 0)
		return "INFO";
	if (strcmp(severity, "HIGH") == 0 || strcmp(severity, "BLOCKER") == 0)
		return "MAJOR";
	return "MINOR";
}

static int json_add_strings(cJSON *obj, const char *const (*fields)[2], size_t n)
{
	size_t i;

	for (i = 0; i < n; i++) {
		if (!cJSON_AddStringToObject(obj, fields[i][0], fields[i][1]))
			return -1;
	}
	return 0;
}

static int add_rule(cJSON *rules, const char *rule_id, const char *cat,
	const char *quality, const char *severity)
{
	cJSON *rule;
	cJSON *impacts;
	cJSON *impact;
	char name[128];
	char desc[256];
	const char *attr;
	const char *std_sev;
	const char *rule_fields[7][2];
	const char *impact_fields[2][2];

	if (find_rule(rules, rule_id))
		return 0;

	rule = cJSON_CreateObject();
	impacts = cJSON_CreateArray();
	impact = cJSON_CreateObject();
	if (!rule || !impacts || !impact) {
		cJSON_Delete(rule);
		cJSON_Delete(impacts);
		cJSON_Delete(impact);
		return -1;
	}
	attr = (strcmp(cat, "performance") == 0) ? "EFFICIENT" : "CONVENTIONAL";
	std_sev = std_severity(severity);
	snprintf(name, sizeof name, "Unlighthouse: %s", cat);
	snprintf(desc, sizeof desc,
		"Category score below 1.0 for %s (Unlighthouse site scan).", cat);

	rule_fields[0][0] = "id";
	rule_fields[0][1] = rule_id;
	rule_fields[1][0] = "name";
	rule_fields[1][1] = name;
	rule_fields[2][0] = "description";
	rule_fields[2][1] = desc;
	rule_fields[3][0] = "engineId";
	rule_fields[3][1] = "lighthouse";
	rule_fields[4][0] = "cleanCodeAttribute";
	rule_fields[4][1] = attr;
	rule_fields[5][0] = "type";
	rule_fields[5][1] = "CODE_SMELL";
	rule_fields[6][0] = "severity";
	rule_fields[6][1] = std_sev;
	impact_fields[0][0] = "softwareQuality";
	impact_fields[0][1] = quality;
	impact_fields[1][0] = "severity";
	impact_fields[1][1] = severity;

	if (json_add_strings(rule, rule_fields, 7) != 0
	    || json_add_strings(impact, impact_fields, 2) != 0) {
		cJSON_Delete(rule);
		cJSON_Delete(impacts);
		cJSON_Delete(impact);
		return -1;
	}
	cJSON_AddItemToArray(impacts, impact);
	cJSON_AddItemToObject(rule, "impacts", impacts);
	cJSON_AddItemToArray(rules, rule);
	return 0;
}

static int add_issue(cJSON *issues, const char *rule_id, const char *msg,
	const char *file_path)
{
	cJSON *issue;
	cJSON *loc;
	cJSON *range;

	issue = cJSON_CreateObject();
	loc = cJSON_CreateObject();
	range = cJSON_CreateObject();
	if (!issue || !loc || !range) {
		cJSON_Delete(issue);
		cJSON_Delete(loc);
		cJSON_Delete(range);
		return -1;
	}
	if (!cJSON_AddStringToObject(issue, "ruleId", rule_id)
	    || !cJSON_AddNumberToObject(issue, "effortMinutes", 10)
	    || !cJSON_AddStringToObject(loc, "message", msg)
	    || !cJSON_AddStringToObject(loc, "filePath", file_path)
	    || !cJSON_AddNumberToObject(range, "startLine", 1)) {
		cJSON_Delete(issue);
		cJSON_Delete(loc);
		cJSON_Delete(range);
		return -1;
	}
	cJSON_AddItemToObject(loc, "textRange", range);
	cJSON_AddItemToObject(issue, "primaryLocation", loc);
	cJSON_AddItemToArray(issues, issue);
	return 0;
}

static int write_payload(cJSON *rules, cJSON *issues)
{
	cJSON *root;
	char *printed;
	size_t len;
	int rc;
	FILE *fp;

	root = cJSON_CreateObject();
	if (!root)
		return -1;
	cJSON_AddItemToObject(root, "rules", rules);
	cJSON_AddItemToObject(root, "issues", issues);
	printed = cJSON_Print(root);
	cJSON_DetachItemFromObject(root, "rules");
	cJSON_DetachItemFromObject(root, "issues");
	cJSON_Delete(root);
	if (!printed)
		return -1;
	len = strlen(printed);
	if (mkdir_p("build/report") != 0) {
		free(printed);
		return -1;
	}
	rc = write_file(OUT_PATH, printed, len);
	free(printed);
	if (rc != 0)
		return rc;
	fp = fopen(OUT_PATH, "a");
	if (fp) {
		fputc('\n', fp);
		fclose(fp);
	}
	return 0;
}

static int write_empty(void)
{
	cJSON *rules;
	cJSON *issues;
	int rc;

	rules = cJSON_CreateArray();
	issues = cJSON_CreateArray();
	if (!rules || !issues) {
		cJSON_Delete(rules);
		cJSON_Delete(issues);
		return -1;
	}
	rc = write_payload(rules, issues);
	cJSON_Delete(rules);
	cJSON_Delete(issues);
	return rc;
}

static int sn_ok(int n, size_t cap)
{
	return n >= 0 && (size_t)n < cap;
}

static int score_failed(const cJSON *cat, const cJSON **score_j)
{
	*score_j = cJSON_GetObjectItemCaseSensitive(cat, "score");
	if (!cJSON_IsNumber(*score_j))
		return 1;
	return (*score_j)->valuedouble >= 1.0;
}

static int process_category(cJSON *cat, const char *path, const char *file_path,
	const char *device, cJSON *rules, cJSON *issues, int *nissues)
{
	const cJSON *score_j;
	const char *cat_id;
	const char *quality;
	const char *severity;
	char rule_id[128];
	char msg[512];
	int n;

	if (!cat->string || strcmp(cat->string, "pwa") == 0)
		return 0;
	cat_id = cat->string;
	if (score_failed(cat, &score_j))
		return 0;

	impact_for_cat(cat_id, &quality, &severity);
	n = snprintf(rule_id, sizeof rule_id, "lh-category-%s", cat_id);
	if (!sn_ok(n, sizeof rule_id))
		return -1;
	if (add_rule(rules, rule_id, cat_id, quality, severity) != 0)
		return -1;
	n = snprintf(msg, sizeof msg,
		"[%s/%s] category score=%g @ %s",
		cat_id, device, score_j->valuedouble, path);
	if (!sn_ok(n, sizeof msg))
		return -1;
	if (add_issue(issues, rule_id, msg, file_path) != 0)
		return -1;
	(*nissues)++;
	return 0;
}

static int process_route(const cJSON *route, cJSON *rules, cJSON *issues,
	int *nissues, const char *device)
{
	const cJSON *path_j;
	const cJSON *cats;
	cJSON *cat;
	const char *path;
	const char *file_path;

	path_j = cJSON_GetObjectItemCaseSensitive(route, "path");
	path = (cJSON_IsString(path_j) && path_j->valuestring)
		? path_j->valuestring : "/";
	file_path = file_for_path(path);
	cats = cJSON_GetObjectItemCaseSensitive(route, "categories");
	if (!cJSON_IsObject(cats))
		return 0;

	for (cat = cats->child; cat; cat = cat->next) {
		if (process_category(cat, path, file_path, device,
		    rules, issues, nissues) != 0)
			return -1;
	}
	return 0;
}

static int process_ci_result(cJSON *root, cJSON *rules, cJSON *issues,
	int *nissues, size_t *nroutes, const char *device)
{
	const cJSON *routes;
	const cJSON *route;

	routes = cJSON_GetObjectItemCaseSensitive(root, "routes");
	if (!cJSON_IsArray(routes))
		return -1;
	*nroutes += (size_t)cJSON_GetArraySize(routes);

	cJSON_ArrayForEach(route, routes) {
		if (process_route(route, rules, issues, nissues, device) != 0)
			return -1;
	}
	return 0;
}

/* Returns 1 if processed, 0 if missing, -1 on error. */
static int load_ci_result(const char *path, const char *device,
	cJSON *rules, cJSON *issues, int *nissues, size_t *nroutes)
{
	char *text;
	cJSON *root;
	int rc;

	text = read_file(path, NULL);
	if (!text) {
		fprintf(stderr, "lhtosonar: missing %s\n", path);
		return 0;
	}
	root = cJSON_Parse(text);
	free(text);
	if (!root) {
		fprintf(stderr, "lhtosonar: parse %s\n", path);
		return -1;
	}
	rc = process_ci_result(root, rules, issues, nissues, nroutes, device);
	cJSON_Delete(root);
	if (rc != 0) {
		fprintf(stderr, "lhtosonar: process %s\n", path);
		return -1;
	}
	return 1;
}

int main(void)
{
	cJSON *rules;
	cJSON *issues;
	int nissues = 0;
	size_t nroutes = 0;
	int nfiles = 0;
	int rc;

	rules = cJSON_CreateArray();
	issues = cJSON_CreateArray();
	if (!rules || !issues)
		die("oom");

	rc = load_ci_result(CI_RESULT_DESKTOP, "desktop", rules, issues,
		&nissues, &nroutes);
	if (rc < 0) {
		cJSON_Delete(rules);
		cJSON_Delete(issues);
		die("lhtosonar: desktop");
	}
	nfiles += rc;

	rc = load_ci_result(CI_RESULT_MOBILE, "mobile", rules, issues,
		&nissues, &nroutes);
	if (rc < 0) {
		cJSON_Delete(rules);
		cJSON_Delete(issues);
		die("lhtosonar: mobile");
	}
	nfiles += rc;

	if (nfiles == 0) {
		cJSON_Delete(rules);
		cJSON_Delete(issues);
		if (mkdir_p(LH_DIR) != 0 || write_empty() != 0)
			die("lhtosonar: write empty %s", OUT_PATH);
		fprintf(stderr,
			"lhtosonar: no ci-result.json; wrote empty %s\n",
			OUT_PATH);
		return 0;
	}

	if (write_payload(rules, issues) != 0) {
		cJSON_Delete(rules);
		cJSON_Delete(issues);
		die("lhtosonar: write %s", OUT_PATH);
	}
	cJSON_Delete(rules);
	cJSON_Delete(issues);
	printf("lhtosonar: %d issue(s) from %zu route(s) -> %s\n",
		nissues, nroutes, OUT_PATH);
	return 0;
}
