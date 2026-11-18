#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif

#include "util.h"

#include <cjson/cJSON.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define LH_DIR "build/report/lh"
#define CI_RESULT "build/report/lh/ci-result.json"
#define OUT_PATH "build/report/lh-sonar-issues.json"
#define DEFAULT_FILE "templates/index.html"

struct cat_impact {
	const char *cat;
	const char *quality;
	const char *severity;
};

static const struct cat_impact IMPACTS[] = {
	{ "performance", "RELIABILITY", "MEDIUM" },
	{ "accessibility", "MAINTAINABILITY", "HIGH" },
	{ "best-practices", "SECURITY", "MEDIUM" },
	{ "seo", "MAINTAINABILITY", "MEDIUM" },
};

static const char *file_for_path(const char *path)
{
	if (!path)
		return DEFAULT_FILE;
	if (strstr(path, "/blog/") && strstr(path, "/post/"))
		return "templates/post.html";
	if (strstr(path, "/blog/") && strstr(path, "/tag/"))
		return "templates/tag.html";
	if (strstr(path, "/blog/"))
		return "templates/blog.html";
	/* /ca/, /es/, /en/ index or section pages */
	if (strstr(path, "/educ") || strstr(path, "/experi")
	    || strstr(path, "/project") || strstr(path, "/proyect"))
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
	std_sev = (strcmp(severity, "HIGH") == 0 || strcmp(severity, "BLOCKER") == 0)
		? "MAJOR" : "MINOR";
	snprintf(name, sizeof name, "Unlighthouse: %s", cat);
	snprintf(desc, sizeof desc,
		"Category score below 1.0 for %s (Unlighthouse site scan).", cat);

	if (!cJSON_AddStringToObject(rule, "id", rule_id)
	    || !cJSON_AddStringToObject(rule, "name", name)
	    || !cJSON_AddStringToObject(rule, "description", desc)
	    || !cJSON_AddStringToObject(rule, "engineId", "lighthouse")
	    || !cJSON_AddStringToObject(rule, "cleanCodeAttribute", attr)
	    || !cJSON_AddStringToObject(rule, "type", "CODE_SMELL")
	    || !cJSON_AddStringToObject(rule, "severity", std_sev)
	    || !cJSON_AddStringToObject(impact, "softwareQuality", quality)
	    || !cJSON_AddStringToObject(impact, "severity", severity)) {
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

static int process_ci_result(cJSON *root, cJSON *rules, cJSON *issues, int *nissues)
{
	const cJSON *routes;
	const cJSON *route;

	routes = cJSON_GetObjectItemCaseSensitive(root, "routes");
	if (!cJSON_IsArray(routes))
		return -1;

	cJSON_ArrayForEach(route, routes) {
		const cJSON *path_j;
		const cJSON *cats;
		const cJSON *cat;
		const char *path;
		const char *file_path;

		path_j = cJSON_GetObjectItemCaseSensitive(route, "path");
		path = (cJSON_IsString(path_j) && path_j->valuestring)
			? path_j->valuestring : "/";
		file_path = file_for_path(path);
		cats = cJSON_GetObjectItemCaseSensitive(route, "categories");
		if (!cJSON_IsObject(cats))
			continue;

		for (cat = cats->child; cat; cat = cat->next) {
			const cJSON *score_j;
			const char *cat_id;
			const char *quality;
			const char *severity;
			char rule_id[128];
			char msg[512];
			int n;

			if (!cat->string)
				continue;
			cat_id = cat->string;
			if (strcmp(cat_id, "pwa") == 0)
				continue;
			score_j = cJSON_GetObjectItemCaseSensitive(cat, "score");
			if (!cJSON_IsNumber(score_j) || score_j->valuedouble >= 1.0)
				continue;

			impact_for_cat(cat_id, &quality, &severity);
			n = snprintf(rule_id, sizeof rule_id, "lh-category-%s", cat_id);
			if (n < 0 || (size_t)n >= sizeof rule_id)
				return -1;
			if (add_rule(rules, rule_id, cat_id, quality, severity) != 0)
				return -1;
			n = snprintf(msg, sizeof msg,
				"[%s] category score=%g @ %s",
				cat_id, score_j->valuedouble, path);
			if (n < 0 || (size_t)n >= sizeof msg)
				return -1;
			if (add_issue(issues, rule_id, msg, file_path) != 0)
				return -1;
			(*nissues)++;
		}
	}
	return 0;
}

int main(void)
{
	char *text;
	cJSON *root;
	cJSON *rules;
	cJSON *issues;
	int nissues = 0;
	size_t nroutes = 0;
	const cJSON *routes;

	text = read_file(CI_RESULT, NULL);
	if (!text) {
		if (mkdir_p(LH_DIR) != 0 || write_empty() != 0)
			die("lighthouse_to_sonar: write empty %s", OUT_PATH);
		fprintf(stderr,
			"lighthouse_to_sonar: missing %s; wrote empty %s\n",
			CI_RESULT, OUT_PATH);
		return 0;
	}

	root = cJSON_Parse(text);
	free(text);
	if (!root)
		die("lighthouse_to_sonar: parse %s", CI_RESULT);

	rules = cJSON_CreateArray();
	issues = cJSON_CreateArray();
	if (!rules || !issues)
		die("oom");

	if (process_ci_result(root, rules, issues, &nissues) != 0) {
		cJSON_Delete(root);
		cJSON_Delete(rules);
		cJSON_Delete(issues);
		die("lighthouse_to_sonar: process %s", CI_RESULT);
	}

	routes = cJSON_GetObjectItemCaseSensitive(root, "routes");
	if (cJSON_IsArray(routes))
		nroutes = (size_t)cJSON_GetArraySize(routes);
	cJSON_Delete(root);

	if (write_payload(rules, issues) != 0) {
		cJSON_Delete(rules);
		cJSON_Delete(issues);
		die("lighthouse_to_sonar: write %s", OUT_PATH);
	}
	cJSON_Delete(rules);
	cJSON_Delete(issues);
	printf("lighthouse_to_sonar: %d issue(s) from %zu route(s) -> %s\n",
		nissues, nroutes, OUT_PATH);
	return 0;
}
