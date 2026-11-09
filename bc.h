#ifndef BC_H
#define BC_H

#define _GNU_SOURCE
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/file.h>
#include <sys/mman.h>
#include <sys/stat.h>

typedef struct {
	int         fd;
	size_t      size;
	size_t      len;
	size_t      max;
	const void* map;
} bc;

static int bc_init(bc* db, const char* path, size_t size, size_t max) {
	if (size == 0 || size > 512) return -1;

	struct stat st;
	db->fd = open(path, O_RDWR | O_CREAT, 0664);
	if (db->fd < 0) return -1;
	if (flock(db->fd, LOCK_EX | LOCK_NB) != 0) goto fail;
	if (fstat(db->fd, &st) != 0) goto fail;

	size_t whole = (size_t)st.st_size - (size_t)st.st_size % size;
	if (whole > max) goto fail;
	if ((size_t)st.st_size != whole && ftruncate(db->fd, whole) != 0) goto fail;
	if (lseek(db->fd, 0, SEEK_END) < 0) goto fail;

	db->map = mmap(NULL, max, PROT_READ, MAP_SHARED, db->fd, 0);
	if (db->map == MAP_FAILED) goto fail;

	db->size = size;
	db->len  = whole / size;
	db->max  = max;
	return 0;

fail:
	close(db->fd);
	return -1;
}

static int bc_fini(bc* db) {
	int rc = fdatasync(db->fd);
	munmap((void*)db->map, db->max);
	close(db->fd);
	return rc;
}

static size_t bc_len (const bc* db) {
	return db->len;
}

static size_t bc_size(const bc* db) {
	return db->len * db->size;
}

static int bc_get (const bc* db, size_t idx, void* data) {
	if (idx >= db->len) return -1;

	memcpy(data, (const char*)db->map + idx * db->size, db->size);
	return 0;
}

static int bc_ref (const bc* db, size_t idx, const void** ref) {
	if (idx >= db->len) return -1;

	*ref = (const char*)db->map + idx * db->size;
	return 0;
}

static int bc_add (bc* db, size_t* idx, const void* data) {
	if ((db->len + 1) * db->size > db->max) return -1;
	if (write(db->fd, data, db->size) != (ssize_t)db->size) {
		ftruncate(db->fd, db->len * db->size);
		lseek(db->fd, 0, SEEK_END);
		return -1;
	}

	if (idx) *idx = db->len;
	db->len++;
	return 0;
}

#endif
