#include "../../include/parser/coverity_parser.h"
#include "../../include/common/utils.h"
#include "../../include/common/logging.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*
 * Coverity cov-analyze text output (from cov-format-errors --text-output)
 * produces blocks like:
 *
 *   Error: CHECKER_NAME (checker/category)
 *   /path/to/file.c:42:
 *   [some context lines ...]
 *
 * We identify the start of each finding by a line beginning with "Error: "
 * then read the following file:line line.
 *
 * Severity mapping: Coverity labels all findings as "Error" in text mode;
 * we map them to SEV_HIGH unless the checker name contains "advisory" (LOW).
 */

static Severity severity_from_coverity(const char *checker)
{
    if (!checker) return SEV_HIGH;
    /* Coverity advisory checkers are lower priority */
    if (strstr(checker, "ADVISORY") || strstr(checker, "advisory"))
        return SEV_LOW;
    return SEV_HIGH;
}

FindingList *parse_coverity(const char *output_file)
{
    FILE *fp = fopen(output_file, "r");
    if (!fp) {
        LOG_ERROR("Cannot open coverity output: %s", output_file);
        return NULL;
    }

    FindingList *list = finding_list_new();
    if (!list) { fclose(fp); return NULL; }

    char line[4096];
    Finding *current = NULL;

    while (fgets(line, sizeof(line), fp)) {
        /* Strip trailing newline */
        size_t len = strlen(line);
        while (len > 0 && (line[len - 1] == '\n' || line[len - 1] == '\r'))
            line[--len] = '\0';

        if (str_starts_with(line, "Error: ")) {
            /* Commit any previous incomplete finding */
            if (current) {
                finding_list_add(list, current);
                current = NULL;
            }

            current = finding_new();
            if (!current) continue;

            current->tool = TOOL_COVERITY;

            /* "Error: CHECKER_NAME (optional description)" */
            const char *rest = line + 7;  /* skip "Error: " */
            const char *paren = strchr(rest, '(');
            char checker[128] = {0};
            if (paren && paren > rest) {
                size_t clen = (size_t)(paren - rest);
                if (clen >= sizeof(checker)) clen = sizeof(checker) - 1;
                memcpy(checker, rest, clen);
                str_trim(checker);
            } else {
                strncpy(checker, rest, sizeof(checker) - 1);
                str_trim(checker);
            }

            current->category = str_dup(checker);
            current->severity = severity_from_coverity(checker);

            /* Extract the description inside parentheses as the message */
            if (paren) {
                const char *msg_start = paren + 1;
                const char *msg_end   = strrchr(msg_start, ')');
                if (msg_end && msg_end > msg_start) {
                    size_t mlen = (size_t)(msg_end - msg_start);
                    char *msg = malloc(mlen + 1);
                    if (msg) {
                        memcpy(msg, msg_start, mlen);
                        msg[mlen] = '\0';
                        current->message = msg;
                    }
                }
            }
            if (!current->message)
                current->message = str_dup(checker);

        } else if (current && !current->file && len > 0) {
            /* First non-empty line after "Error:" should be file:line: */
            char *colon = strchr(line, ':');
            if (colon) {
                size_t flen = (size_t)(colon - line);
                current->file = malloc(flen + 1);
                if (current->file) {
                    memcpy(current->file, line, flen);
                    current->file[flen] = '\0';
                }
                current->line = atoi(colon + 1);
            }
        }
    }

    if (current)
        finding_list_add(list, current);

    fclose(fp);
    LOG_INFO("coverity: parsed %zu findings from %s", list->count, output_file);
    return list;
}
