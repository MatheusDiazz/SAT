#include "../../include/parser/gcc_parser.h"
#include "../../include/common/utils.h"
#include "../../include/common/logging.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*
 * GCC -fanalyzer writes to stderr in the form:
 *   file.c:line:col: severity: message
 * Continuation lines (spaces, graph edges "|-", "+--") are skipped.
 */

static Severity severity_from_gcc(const char *s)
{
    if (!s)                        return SEV_UNKNOWN;
    if (strcmp(s, "error") == 0)   return SEV_HIGH;
    if (strcmp(s, "warning") == 0) return SEV_MEDIUM;
    if (strcmp(s, "note") == 0)    return SEV_INFO;
    return SEV_UNKNOWN;
}

FindingList *parse_gcc(const char *output_file)
{
    FILE *fp = fopen(output_file, "r");
    if (!fp) {
        LOG_ERROR("Cannot open gcc output: %s", output_file);
        return NULL;
    }

    FindingList *list = finding_list_new();
    if (!list) { fclose(fp); return NULL; }

    char line[4096];

    while (fgets(line, sizeof(line), fp)) {
        /* Skip context/graph lines */
        if (line[0] == ' ' || line[0] == '|' || line[0] == '+' || line[0] == '\n')
            continue;

        /* file:line:col: severity: message */
        char *p = line;

        char *colon1 = strchr(p, ':');
        if (!colon1) continue;

        /* Skip Windows drive letter (e.g. C:\...) */
        if (colon1 == p + 1 && ((*p >= 'A' && *p <= 'Z') || (*p >= 'a' && *p <= 'z')))
            colon1 = strchr(colon1 + 1, ':');
        if (!colon1) continue;

        char *colon2 = strchr(colon1 + 1, ':');
        if (!colon2) continue;
        char *colon3 = strchr(colon2 + 1, ':');
        if (!colon3) continue;
        char *colon4 = strchr(colon3 + 1, ':');
        if (!colon4) continue;

        size_t file_len = (size_t)(colon1 - p);
        char *file = malloc(file_len + 1);
        if (!file) continue;
        memcpy(file, p, file_len);
        file[file_len] = '\0';

        int lineno = atoi(colon1 + 1);
        int colno  = atoi(colon2 + 1);

        size_t sev_len = (size_t)(colon4 - colon3 - 1);
        char sev_buf[32] = {0};
        if (sev_len > 0 && sev_len < sizeof(sev_buf)) {
            memcpy(sev_buf, colon3 + 1, sev_len);
            str_trim(sev_buf);
        }

        Severity sev = severity_from_gcc(sev_buf);
        if (sev == SEV_UNKNOWN) {
            free(file);
            continue;
        }

        char *msg = colon4 + 1;
        while (*msg == ' ') msg++;
        size_t msg_len = strlen(msg);
        while (msg_len > 0 && (msg[msg_len - 1] == '\n' || msg[msg_len - 1] == '\r'))
            msg[--msg_len] = '\0';

        Finding *f = finding_new();
        if (!f) { free(file); continue; }

        f->tool     = TOOL_GCC;
        f->file     = file;
        f->line     = lineno;
        f->column   = colno;
        f->severity = sev;
        f->category = str_dup("gcc-analyzer");
        f->message  = str_dup(msg);

        finding_list_add(list, f);
    }

    fclose(fp);
    LOG_INFO("gcc: parsed %zu findings from %s", list->count, output_file);
    return list;
}
