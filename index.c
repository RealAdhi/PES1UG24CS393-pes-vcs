#include "index.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <dirent.h>

// ─── PROVIDED ────────────────────────────────────────────────────────────────

IndexEntry* index_find(Index *index, const char *path) {
    for (int i = 0; i < index->count; i++) {
        if (strcmp(index->entries[i].path, path) == 0) return &index->entries[i];
    }
    return NULL;
}

int index_remove(Index *index, const char *path) {
    for (int i = 0; i < index->count; i++) {
        if (strcmp(index->entries[i].path, path) == 0) {
            int remaining = index->count - i - 1;
            if (remaining > 0) memmove(&index->entries[i], &index->entries[i + 1], remaining * sizeof(IndexEntry));
            index->count--;
            return index_save(index);
        }
    }
    return -1;
}

int index_status(const Index *index) {
    printf("Staged changes:\n");
    for (int i = 0; i < index->count; i++) printf("  staged:      %s\n", index->entries[i].path);
    if (index->count == 0) printf("  (nothing to show)\n");
    return 0;
}

// ─── IMPLEMENTATION ──────────────────────────────────────────────────────────

int index_load(Index *index) {
    index->count = 0;
    FILE *f = fopen(INDEX_FILE, "r");
    if (!f) return 0;
    char line[1024];
    while (fgets(line, sizeof(line), f)) {
        if (index->count >= MAX_INDEX_ENTRIES) break;
        IndexEntry *e = &index->entries[index->count];
        char hex[HASH_HEX_SIZE + 1];
        if (sscanf(line, "%o %64s %lu %u %[^\n]", &e->mode, hex, &e->mtime_sec, &e->size, e->path) == 5) {
            hex_to_hash(hex, &e->hash);
            index->count++;
        }
    }
    fclose(f);
    return 0;
}

static int compare_idx(const void *a, const void *b) {
    return strcmp(((const IndexEntry *)a)->path, ((const IndexEntry *)b)->path);
}

int index_save(const Index *index) {
    Index sorted = *index;
    qsort(sorted.entries, sorted.count, sizeof(IndexEntry), compare_idx);
    char tmp[512];
    snprintf(tmp, 512, "%s.tmp", INDEX_FILE);
    FILE *f = fopen(tmp, "w");
    if (!f) return -1;
    for (int i = 0; i < sorted.count; i++) {
        char hex[HASH_HEX_SIZE + 1];
        hash_to_hex(&sorted.entries[i].hash, hex);
        fprintf(f, "%o %s %lu %u %s\n", sorted.entries[i].mode, hex, sorted.entries[i].mtime_sec, sorted.entries[i].size, sorted.entries[i].path);
    }
    fflush(f);
    fsync(fileno(f));
    fclose(f);
    return rename(tmp, INDEX_FILE);
}

int index_add(Index *index, const char *path) {
    struct stat st;
    if (lstat(path, &st) != 0) return -1;
    FILE *f = fopen(path, "rb");
    if (!f) return -1;
    uint8_t *buf = malloc(st.st_size);
    if (st.st_size > 0) fread(buf, 1, st.st_size, f);
    fclose(f);
    ObjectID hash;
    object_write(OBJ_BLOB, buf, st.st_size, &hash);
    free(buf);
    IndexEntry *e = index_find(index, path);
    if (!e) e = &index->entries[index->count++];
    strcpy(e->path, path);
    e->mode = (st.st_mode & S_IXUSR) ? 0100755 : 0100644;
    e->hash = hash;
    e->mtime_sec = st.st_mtime;
    e->size = st.st_size;
    return index_save(index);
}