#include "bot/bank.h"
#include "connection.h"

#include <fcntl.h>
#include <unistd.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <stdlib.h>

#include "log.h"

#include "net_rw.h"

void LoadBank(Connection *c) {
	char filepath[1024];
	
	snprintf(filepath, sizeof(filepath),
		"%s/%lu/bank.dat",
		c->bot.data_path,
		c->auth.igg_id
	);
	
	int file = open(filepath, O_RDONLY);
	
	// bank file doesn't exist yet
	if (file == -1) {
		c->bank_data.header.magic   = BANK_MAGIC;
		c->bank_data.header.version = BANK_VERSION;
		c->bank_data.header.count   = 0;
		c->bank_data.capacity = 100;
		
		c->bank_data.record = calloc(c->bank_data.capacity, sizeof(BankRecord));
		
		if (!c->bank_data.record) {
			LOGE("Failed to allocate bank records.\n");
			exit(EXIT_FAILURE);
		}
		
		return;
	}
	
	// Get the size of the file
	struct stat st;
	
	if (fstat(file, &st) == -1) {
		perror("Error getting file size");
		close(file);
		exit(EXIT_FAILURE);
	}
	
	if (st.st_size < sizeof(BankHeader)) {
		LOGE("Bank file is too small.\n");
		// munmap(data, st.st_size);
		close(file);
		return;
	}
	
	size_t expected_size = sizeof(BankHeader) + (size_t)c->bank_data.header.count * (13 + 5 * sizeof(uint32_t));
	
	if ((size_t)st.st_size < expected_size) {
		LOGE("Bank file is truncated.\n");
		// munmap(data, st.st_size);
		close(file);
		return;
	}
	
	// Map the file into memory
	uint8_t *data = mmap(NULL, st.st_size, PROT_READ, MAP_PRIVATE, file, 0);
	
	if (data == MAP_FAILED) {
		perror("Error mapping file to memory");
		close(file);
		exit(EXIT_FAILURE);
	}
	
	size_t offset = 0;
	
	c->bank_data.header.magic   = read_u32(data + offset); offset += 4;
	c->bank_data.header.version = read_u32(data + offset); offset += 4;
	c->bank_data.header.count   = read_u32(data + offset); offset += 4;
	
	if (c->bank_data.header.magic != BANK_MAGIC || 
		c->bank_data.header.version != BANK_VERSION) {
		LOGE("Invalid bank file.\n");
		munmap(data, st.st_size);
		close(file);
		return;
	}
	
	c->bank_data.capacity = (c->bank_data.header.count < 100) ? 100 : c->bank_data.header.count + 100;
	
	c->bank_data.record = calloc(c->bank_data.capacity, sizeof(BankRecord));
	
	if (!c->bank_data.record) {
		LOGE("Failed to allocate bank records.\n");
		munmap(data, st.st_size);
		close(file);
		exit(EXIT_FAILURE);
	}
	
	for (uint32_t i = 0; i < c->bank_data.header.count; i++) {
		read_raw(c->bank_data.record[i].name, data + offset, 13);
		offset += 13;
		
		for (int j = 0; j < 5; j++) {
			c->bank_data.record[i].stock[j] = read_u32(data + offset);
			offset += 4;
		}
	}
	
	// Unmap the file when done
	munmap(data, st.st_size);
	close(file);
}

int create_file(const char *path)
{
    char dir[1024];

    strncpy(dir, path, sizeof(dir));
    dir[sizeof(dir) - 1] = '\0';

    char *p = strrchr(dir, '/');
    if (p != NULL) {
        *p = '\0';

        for (char *s = dir + 1; *s; s++) {
            if (*s == '/') {
                *s = '\0';
                mkdir(dir, 0755);
                *s = '/';
            }
        }

        mkdir(dir, 0755);
    }

    return open(path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
}

void SaveBank(Connection *c)
{
	char filepath[1024];
	
	snprintf(filepath, sizeof(filepath),
		"%s/%lu/bank.dat",
		c->bot.data_path,
		c->auth.igg_id
	);
	
	int file = create_file(filepath);
	
	if (file == -1) {
		return;
	}
	
	size_t size = sizeof(BankHeader) + c->bank_data.header.count * (13 + 5 * 4);
	
	uint8_t *buffer = malloc(size);
	
	if (!buffer) {
		close(file);
		return;
	}
	
	uint32_t offset = 0;
	
	write_u32(buffer + offset, c->bank_data.header.magic);
	offset += 4;
	
	write_u32(buffer + offset, c->bank_data.header.version);
	offset += 4;
	
	write_u32(buffer + offset, c->bank_data.header.count);
	offset += 4;
	
	for (uint32_t i = 0; i < c->bank_data.header.count; i++) {
		write_raw(buffer + offset, c->bank_data.record[i].name, 13);
		offset += 13;
		
		for (int j = 0; j < 5; j++) {
			write_u32(buffer + offset, c->bank_data.record[i].stock[j]);
			offset += 4;
		}
	}
	
	bool ok = (write(file, buffer, offset) == (ssize_t)offset);
	
	free(buffer);
	close(file);
	
	return;
}

BankRecord *BankFindRecord(Connection *c, const char *name)
{
	for (uint32_t i = 0; i < c->bank_data.header.count; i++) {
		if (strcmp(c->bank_data.record[i].name, name) == 0)
			return &c->bank_data.record[i];
	}
	
	return NULL;
}

BankRecord *BankCreateRecord(Connection *c, const char *name)
{
	if (c->bank_data.header.count >= c->bank_data.capacity) {
		LOGE("Bank is full.\n");
		return NULL;
	}
	
	BankRecord *record = &c->bank_data.record[c->bank_data.header.count];
	memset(record, 0, sizeof(*record));
	
	strncpy(record->name, name, sizeof(record->name) - 1);
	record->name[sizeof(record->name) - 1] = '\0';
	
	c->bank_data.header.count++;
	
	return record;
}