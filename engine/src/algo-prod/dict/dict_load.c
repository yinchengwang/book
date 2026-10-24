#include "dict_private.h"

#include <stdio.h>

typedef int32_t (*dict_line_parser_t)(dict_t *dict, char *line);

static char *dict_trim_in_place(char *text)
{
    char *start;
    char *end;

    if (!text) {
        return NULL;
    }

    /* 原地 trim，返回首个非空白字符佝置�?*/
    start = text;
    while (*start != '\0' && isspace((unsigned char)*start)) {
        start += 1;
    }
    if (*start == '\0') {
        return start;
    }

    end = start + strlen(start) - 1;
    while (end >= start && isspace((unsigned char)*end)) {
        *end = '\0';
        end -= 1;
    }
    return start;
}

static int32_t dict_parse_stop_word_line(dict_t *dict, char *line)
{
    char *word;

    if (!dict || !line) {
        return -1;
    }

    word = dict_trim_in_place(line);
    if (*word == '\0' || *word == '#') {
        return 0;
    }

    return dict_add_stop_word(dict, word);
}

static int32_t dict_parse_synonym_line(dict_t *dict, char *line)
{
    char *word;
    char *canonical;
    char *separator;

    if (!dict || !line) {
        return -1;
    }

    word = dict_trim_in_place(line);
    if (*word == '\0' || *word == '#') {
        return 0;
    }

    separator = word;
    while (*separator != '\0' && !isspace((unsigned char)*separator) && *separator != ',' && *separator != '=') {
        separator += 1;
    }
    if (*separator == '\0') {
        return -1;
    }

    *separator = '\0';
    canonical = dict_trim_in_place(separator + 1);
    if (*canonical == '>') {
        canonical = dict_trim_in_place(canonical + 1);
    }
    if (*canonical == '\0') {
        return -1;
    }

    return dict_add_synonym(dict, word, canonical);
}

static int32_t dict_parse_jieba_line(dict_t *dict, char *line)
{
    char *cursor;
    char *word;
    char *freq_text;
    long freq;

    if (!dict || !line) {
        return -1;
    }

    cursor = line;
    while (*cursor == ' ' || *cursor == '\t' || *cursor == '\r' || *cursor == '\n') {
        cursor += 1;
    }
    if (*cursor == '\0' || *cursor == '#') {
        return 0;
    }

    word = cursor;
    while (*cursor != '\0' && *cursor != ' ' && *cursor != '\t' && *cursor != '\r' && *cursor != '\n') {
        cursor += 1;
    }
    if (*cursor == '\0') {
        return dict_add_word(dict, word, 1);
    }

    *cursor = '\0';
    cursor += 1;
    while (*cursor == ' ' || *cursor == '\t') {
        cursor += 1;
    }
    if (*cursor == '\0' || *cursor == '\r' || *cursor == '\n') {
        return dict_add_word(dict, word, 1);
    }

    freq_text = cursor;
    while (*cursor != '\0' && *cursor != ' ' && *cursor != '\t' && *cursor != '\r' && *cursor != '\n') {
        cursor += 1;
    }
    *cursor = '\0';

    freq = strtol(freq_text, NULL, 10);
    if (freq <= 0) {
        freq = 1;
    }
    return dict_add_word(dict, word, (int32_t)freq);
}

static int32_t dict_load_file_lines(dict_t *dict, const char *path, dict_line_parser_t parser)
{
    FILE *file;
    char line[4096];

    if (!dict || !path || !parser) {
        return -1;
    }

    file = fopen(path, "rb");
    if (!file) {
        return -1;
    }

    /* 逝行解枝文件，把具体语义交给 parser�?*/
    while (fgets(line, (int)sizeof(line), file) != NULL) {
        if (parser(dict, line) != 0) {
            fclose(file);
            return -1;
        }
    }

    fclose(file);
    return 0;
}

int32_t dict_load_jieba_buffer(dict_t *dict, const char *buffer)
{
    char *copy;
    char *line;
    char *next;

    if (!dict || !buffer) {
        return -1;
    }

    /* 先夝制一份坯写缓冲区，冝按杢行切分�?*/
    copy = dict_strdup(buffer);
    if (!copy) {
        return -1;
    }

    /* 缓冲区加载路径与文件加载路径共用坌一套行解枝器�?*/
    line = copy;
    while (line && *line != '\0') {
        next = strchr(line, '\n');
        if (next) {
            *next = '\0';
            next += 1;
        }
        if (dict_parse_jieba_line(dict, line) != 0) {
            free(copy);
            return -1;
        }
        line = next;
    }

    free(copy);
    return 0;
}

int32_t dict_load_jieba_file(dict_t *dict, const char *path)
{
    return dict_load_file_lines(dict, path, dict_parse_jieba_line);
}

int32_t dict_load_stop_words_file(dict_t *dict, const char *path)
{
    return dict_load_file_lines(dict, path, dict_parse_stop_word_line);
}

int32_t dict_load_synonyms_file(dict_t *dict, const char *path)
{
    return dict_load_file_lines(dict, path, dict_parse_synonym_line);
}