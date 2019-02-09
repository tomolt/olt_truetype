#include "mapping.h"

typedef long mapping_handle_t;

#if defined(_WIN32)

#define WIN32_LEAN_AND_MEAN 1
#include <windows.h>

int olt_INTERN_unmap_file(void *addr, mapping_handle_t mapping)
{
	BOOL ret = TRUE;
	ret &= addr == NULL ? FALSE : UnmapViewOfFile(addr);
	ret &= mapping == INVALID_HANDLE_VALUE ? FALSE : CloseHandle(mapping);
	return ret ? 0 : -1;
}

int olt_INTERN_map_file(char const *filename, void **addr, mapping_handle_t *mapping)
{
	*addr = NULL;
	*mapping = INVALID_HANDLE_VALUE;

	HANDLE descr = CreateFile(filename, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
	if (descr == INVALID_HANDLE_VALUE) goto fail;

	DWORD high, low = GetFileSize(descr, &high);
	if (low == INVALID_FILE_SIZE) goto fail;

	*mapping = CreateFileMapping(descr, NULL, PAGE_READONLY, high, low, NULL);
	if (*mapping == NULL) goto fail;

	*addr = MapViewOfFile(*mapping, FILE_MAP_READ, 0, 0, 0);
	if (*addr == NULL) goto fail;

	BOOL close_ret = CloseHandle(descr);
	if (!close_ret) goto fail;

	return 0;

fail:
	// don't care about further return values - we're already failing.
	if (descr != INVALID_HANDLE_VALUE)
		CloseHandle(descr);
	olt_INTERN_unmap_file(str);

	return -1;
}

#elif defined(__unix__)

#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <limits.h>

int olt_INTERN_unmap_file(void *addr, mapping_handle_t mapping)
{
	return addr == MAP_FAILED ? -1 : munmap(addr, mapping);
}

int olt_INTERN_map_file(char const *filename, void **addr, mapping_handle_t *mapping)
{
	int ret;

	*addr = MAP_FAILED;

	int descr = open(filename, O_RDONLY);
	if (descr < 0) goto fail;

	struct stat stat;
	ret = fstat(descr, &stat);
	if (ret != 0) goto fail;
	if (stat.st_size > LONG_MAX) goto fail;
	*mapping = stat.st_size;

	*addr = mmap(NULL, stat.st_size, PROT_READ, MAP_PRIVATE, descr, 0);
	if (*addr == MAP_FAILED) goto fail;

	ret = close(descr);
	if (ret != 0) goto fail;

	return 0;

fail:
	// don't care about further return values - we're already failing.
	if (descr >= 0)
		close(descr);
	olt_INTERN_unmap_file(*addr, *mapping);

	return -1;
}

#else
#error Platform not supported.
#endif
