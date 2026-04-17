#include "../../include/parser/flawfinder_parser.h"
#include "../../include/common/utils.h"
#include "../../include/common/logging.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*
 * Flawfinder --dataonly --csv produces:
 *   File,Line,Column,Level,Category,Name,Warning
 * where Level is 0-5 (0=lowest risk, 5=highest).
 * The first line is a header row and is skipped.
 *
 * Fields may be quoted; commas inside quoted fields are allowed.
 */

/* Copy the next CSV field from *src into buf (max buf_size).
 * Advances *src past the trailing comma (or to end of string).
 * Returns length written, or -1 on error.
 */
static int csv_next_field(const char **src, char *buf, size_t buf_size)
{
    const char *p = *src;
    size_t len = 0;

    if (*p == '"') {
        p++;
        while (*p && !(*p == '"' && *(p + 1) != '"')) {
            if (*p == '"' && *(p + 1) == '"') { p++; }  /* escaped quote */
            if (len + 1 < buf_size) buf[len++] = *p;
            p++;
        }
        if (*p == '"') p++;
    } else {
        while (*p && *p != ',' && *p != '\n' && *p != '\r') {
            if (len + 1 < buf_size) buf[len++] = *p;
            p++;
        }
    }

    buf[len] = '\0';
    if (*p == ',') p++;
    *src = p;
    return (int)len;
}

static Severity severity_from_flawfinder(int level)
{
    if (level <= 0) return SEV_INFO;
    if (level == 1) return SEV_LOW;
    if (level == 2) return SEV_LOW;
    if (level == 3) return SEV_MEDIUM;
    if (level == 4) return SEV_HIGH;
    return SEV_CRITICAL;   /* 5 */
}

FindingList *parse_flawfinder(const char *output_file)
{
    FILE *fp = fopen(output_file, "r");
    if (!fp) {
        LOG_ERROR("Cannot open flawfinder output: %s", output_file);
        return NULL;
    }

    FindingList *list = finding_list_new();
    if (!list) { fclose(fp); return NULL; }

    char line[4096];
    int first = 1;

    while (fgets(line, sizeof(line), fp)) {
        /* Skip header row */
        if (first) { first = 0; continue; }

        /* Skip blank lines */
        if (line[0] == '\n' || line[0] == '\r') continue;

        const char *p = line;
        char file[512], lineno_s[32], col_s[32], level_s[8], category[128], name[128], warning[1024];

        if (csv_next_field(&p, file,     sizeof(file))     < 0) continue;
        if (csv_next_field(&p, lineno_s, sizeof(lineno_s)) < 0) continue;
        if (csv_next_field(&p, col_s,    sizeof(col_s))    < 0) continue;
        if (csv_next_field(&p, level_s,  sizeof(level_s))  < 0) continue;
        if (csv_next_field(&p, category, sizeof(category)) < 0) continue;
        if (csv_next_field(&p, name,     sizeof(name))     < 0) continue;
        csv_next_field(&p, warning, sizeof(warning));

        Finding *f = finding_new();
        if (!f) continue;

        f->tool     = TOOL_FLAWFINDER;
        f->file     = str_dup(file);
        f->line     = atoi(lineno_s);
        f->column   = atoi(col_s);
        f->severity = severity_from_flawfinder(atoi(level_s));
        f->category = str_dup(name[0] ? name : category);
        f->message  = str_dup(warning);

        finding_list_add(list, f);
    }

    fclose(fp);
    LOG_INFO("flawfinder: parsed %zu findings from %s", list->count, output_file);
    return list;
}
