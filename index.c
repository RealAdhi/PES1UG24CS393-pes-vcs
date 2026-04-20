#include "index.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

extern int object_write(ObjectType type, const void *data, size_t len, ObjectID *id_out);

IndexEntry* index_find(Index *index, const char *path) {
    if (!index || index->count < 0) return NULL;
    for (int i = 0; i < index->count; i++) {
        if (strcmp(index->entries[i].path, path) == 0) return &index->entries[i];
    }
    return NULL;
}

int index_remove(Index *index, const char *path) {
    IndexEntry *e = index_find(index, path);
    if (!e) return -1;
    int idx = e - index->entries;
    int remaining = index->count - idx - 1;
    if (remaining > 0) {
        memmove(&index->entries[idx], &index->entries[idx + 1], remaining * sizeof(IndexEntry));
    }
    index->count--;
    return index_save(index);
}

int index_status(const Index *index) {
    printf("Staged changes:\n");
    if (!index || index->count <= 0) {
        printf("  (nothing to show)\n");
    } else {
        for (int i = 0; i < index->count; i++) {
            printf("  staged:      %s\n", index->entries[i].path);
        }
    }
    return 0;
}

int index_load(Index *index) {
    if (!index) return -1;
    memset(index, 0, sizeof(Index));
    
    FILE *f = fopen(INDEX_FILE, "r");
    if (!f) return 0; // File doesn't exist yet, perfectly normal

    char line[1024];
    while (fgets(line, sizeof(line), f) && index->count < MAX_INDEX_ENTRIES) {
        IndexEntry *e = &index->entries[index->count];
        char hex[HASH_HEX_SIZE + 1];
        unsigned int mode;
        
        // Using %u for size and %lu for mtime to match common 64-bit systems
        if (sscanf(line, "%o %64s %lu %u %[^\n]", 
                   &mode, hex, (unsigned long *)&e->mtime_sec, &e->size, e->path) == 5) {
            e->mode = mode;
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
    if (!index) return -1;
    
    // Allocate memory on the HEAP, not the stack, to avoid Segfaults
    Index *sorted = malloc(sizeof(Index));
    if (!sorted) return -1;
    
    memcpy(sorted, index, sizeof(Index));

    if (sorted->count > 1) {
        qsort(sorted->entries, sorted->count, sizeof(IndexEntry), compare_idx);
    }

    char tmp[1024];
    snprintf(tmp, sizeof(tmp), "%s.tmp", INDEX_FILE);
    
    FILE *f = fopen(tmp, "w");
    if (!f) {
        free(sorted);
        return -1;
    }

    for (int i = 0; i < sorted->count; i++) {
        char hex[HASH_HEX_SIZE + 1];
        hash_to_hex(&sorted->entries[i].hash, hex);
        fprintf(f, "%o %s %lu %u %s\n", 
                sorted->entries[i].mode, 
                hex, 
                (unsigned long)sorted->entries[i].mtime_sec, 
                sorted->entries[i].size, 
                sorted->entries[i].path);
    }

    fflush(f);
    fsync(fileno(f));
    fclose(f);
    
    free(sorted); // Don't forget to free the heap memory!
    return rename(tmp, INDEX_FILE);
}

int index_add(Index *index, const char *path) {
    struct stat st;
    if (lstat(path, &st) != 0) return -1;

    FILE *f = fopen(path, "rb");
    if (!f) return -1;

    void *buf = malloc(st.st_size);
    if (st.st_size > 0 && fread(buf, 1, st.st_size, f) != (size_t)st.st_size) {
        free(buf); fclose(f); return -1;
    }
    fclose(f);

    ObjectID hash;
    if (object_write(OBJ_BLOB, buf, st.st_size, &hash) != 0) {
        free(buf); return -1;
    }
    free(buf);

    IndexEntry *e = index_find(index, path);
    if (!e) {
        if (index->count >= MAX_INDEX_ENTRIES) return -1;
        e = &index->entries[index->count++];
        memset(e->path, 0, sizeof(e->path));
        strncpy(e->path, path, sizeof(e->path) - 1);
    }

    e->mode = (st.st_mode & S_IXUSR) ? 0100755 : 0100644;
    e->hash = hash;
    e->mtime_sec = (uint64_t)st.st_mtime;
    e->size = (uint32_t)st.st_size;

    return index_save(index);
}