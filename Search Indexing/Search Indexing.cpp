// Search Indexing.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"

// Indexing.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <direct.h>
#include <io.h>
#include <ctype.h>
#include <time.h>
#include <stdbool.h>

#define MAX_PATH 1024
#define MAX_WORD_SIZE 128
#define SIZE_BUF (1<<12)
#define HASH_SIZE 36

/* one instance per file, all fileAttrP use the same fileAttr */
typedef struct fileAttr {
	char *filename;
	size_t mtime;
	size_t count;
} fileAttr;

typedef struct fileAttrP {
	fileAttr *file_attr;
	struct fileAttrP *next;
} fileAttrP;

typedef struct wordAttr {
	char *word;
	struct wordAttr *next;
	fileAttrP *filelist;
} wordAttr;

wordAttr  *ht[HASH_SIZE][HASH_SIZE];

bool is_reg_char(char c) {
	if ((c > 47 && c < 58) || (c > 64 && c < 91) ||
		(c > 96 && c < 123))
		return true;
	return false;
}

int char_index(char c) {
	if (c > 47 && c < 58)
		return c - 48;
	if (c > 64 && c < 91)
		return c - 55;
	if (c > 96 && c < 123)
		return c - 87;

	exit(0);
}

void *malloc_wrapper(size_t size) {
	void *p = malloc(size);
	if (p == NULL) {
		perror("error: ");
		exit(errno);
	}
	return p;
}


void print_filelist(fileAttrP *fp) {
	printf("%s\n", fp->file_attr->filename);
	if (fp->next)
		print_filelist(fp->next);
}

void print_wordlist(wordAttr *wp) {
	printf("wordlist(%d): %s\n", strlen(wp->word), wp->word);
	print_filelist(wp->filelist);

	if (wp->next)
		print_wordlist(wp->next);
}


void print_hash() {

	for (int i = 0; i < HASH_SIZE; i++) {
		for (int j = 0; j < HASH_SIZE; j++) {
			if (ht[i][j] != NULL) {
				print_wordlist(ht[i][j]);
			}
		}
	}
}

wordAttr *locate_word(const char *word) {
	int index1 = char_index(word[0]);
	int index2 = char_index(word[1]);

	wordAttr *word_list = ht[index1][index2];

	while (word_list) {
		if (!strcmp(word, word_list->word))
			return word_list;
		word_list = word_list->next;
	}

	return NULL;
}

fileAttr *create_fileAttr(char *filename) {
	char *name = (char *)malloc_wrapper(sizeof(filename) + 1);
	memcpy(name, filename, strlen(filename) + 1);
	
	fileAttr *f = (fileAttr *)malloc_wrapper(sizeof(fileAttr));
	f->filename = name;
	f->count = 0;
	return f;
}


void free_fileAttr(fileAttr *fa) {
	//assert(f->count == 0);
	free(fa->filename);
	free(fa);
}


fileAttrP *create_fileAttrP(fileAttr *file) {
	fileAttrP *fp = (fileAttrP *)malloc_wrapper(sizeof(fileAttrP));
	fp->file_attr = file;
	file->count++;
	fp->next = NULL;
	return fp;
}

void free_fileAttrP(fileAttrP *fp) {
	fp->file_attr->count--;
	if (fp->file_attr->count == 0)
		free_fileAttr(fp->file_attr);
	free(fp);
}

char *create_word(char *buf, size_t len) {
	char *p;
	p = (char *)malloc_wrapper(len + 1);
	memcpy(p, buf, len);
	p[len] = '\0';
	printf("word (length:%d): %s\n", strlen(p), p);
	return p;
}


wordAttr *create_wordAttr(char *word, fileAttr *file) {
	wordAttr *p = (wordAttr *)malloc_wrapper(sizeof(wordAttr));
	p->word = word;
	p->next = NULL;
	p->filelist = create_fileAttrP(file);
	return p;
}

void free_wordAttr(wordAttr *wp) {
	free(wp->word);
	free(wp);
}



int hash_insert(char *buf, size_t len, fileAttr *file) {
	char *word = create_word(buf, len);
	int index1 = char_index(word[0]);
	int index2 = char_index(word[1]);
	wordAttr *word_list = ht[index1][index2];
	wordAttr *w_p;

	w_p = locate_word(word);
	if (w_p == NULL) {
		w_p = create_wordAttr(word, file);
		if (ht[index1][index2] == NULL)
			ht[index1][index2] = w_p;
		w_p->next = word_list;
		w_p->filelist = create_fileAttrP(file);
		if (word_list == NULL)
			ht[index1][index2] = w_p;
	}
	else if (w_p->filelist->file_attr != file) {
		fileAttrP *fp = create_fileAttrP(file);
		fp->next = w_p->filelist;
		w_p->filelist = fp;
	}

	return 1;
}

void build_index(char *filename) {
	FILE *file;
	char buffer[SIZE_BUF];
	if ( fopen_s(&file, filename, "r")) {
		printf("could not open file1 %s \n", (char *)filename);
		perror("error: ");
		return;
	}

	printf("build index for file %s\n", filename);

	/* file has one file attr, but many file attr pointers*/
	fileAttr *file_attr = create_fileAttr(filename);

	size_t word_begin_index = 0;
	size_t partial_word_len = 0;

	while (!feof(file)) {
		int size = fread(buffer + partial_word_len, 1, SIZE_BUF, file);
		if (size != SIZE_BUF && !feof(file)) {
			perror("error: ");
			exit(errno);
		}

		int i;
		word_begin_index %= SIZE_BUF;
		for (i = 0; i < size; i++) {
			if (!is_reg_char(buffer[i])) {
				int word_len = i - word_begin_index;
				if ((word_len >= 2) && (word_len < MAX_WORD_SIZE)) {
					hash_insert(buffer + word_begin_index, word_len, file_attr);
					print_hash();
				}
				word_begin_index = i + 1;
			}
		}

		if (word_begin_index < size) {
			partial_word_len = i - word_begin_index;
			if (word_begin_index >= partial_word_len) {
				memcpy(buffer, buffer + word_begin_index, partial_word_len);
			}
			else {
				memmove(buffer, buffer + word_begin_index, partial_word_len);
			}
		}
		else partial_word_len = 0;
	}

	fclose(file);
}



void search_index(char *word) {
	wordAttr *p = locate_word(word);
	if (p == NULL)
		return;
	fileAttrP *file_list = p->filelist;
	while (file_list) {
		printf("filename: %s", file_list->file_attr->filename);
		file_list = file_list->next;
	}

}

void init_ht() {
	int i, j;
	for (i = 0; i < HASH_SIZE; i++)
		for (j = 0; j < HASH_SIZE; j++)
			ht[i][j] = NULL;
	return;
}

size_t num_files = 0;

void traverse_dir(char *dir) {
	if (_chdir(dir)) {
		printf("Unable to locate the directory: %s\n", dir);
		return;
	}
	printf("Change to %s\n", dir);

	struct _finddata_t c_file;
	long   hFile;

	hFile = _findfirst("*.*", &c_file);
	long next = hFile;
	while (next) {
		if (strcmp(c_file.name, ".") && strcmp(c_file.name, "..") &&
			(strlen(dir) + strlen(c_file.name) < MAX_PATH)) {

			char new_path[MAX_PATH];
			sprintf(new_path, "%s\\%s", dir, c_file.name);

			if (c_file.attrib & _A_SUBDIR) {
				traverse_dir(new_path);
			}
			else {
				build_index(new_path);
				num_files++;
			}
		}
		next = _findnext(hFile, &c_file) == 0 ? true : false;
	}

	_findclose(hFile);
}


int main()
{
	char path[MAX_PATH];
	printf("please enter the directory (up to 512 characters) to build the index\n");
	scanf("%s", path);

	sprintf(path, "c:\\Users\\xiaoqin\\test");

	init_ht();
	traverse_dir(path);

	char word[MAX_WORD_SIZE];
	while (true) {
		printf("please enter the word to search (the max word size is %d):\n", MAX_WORD_SIZE);
		scanf("%s", word);
		if (memcpy(word, "STOP", 4) == 0)
			exit(0);
		search_index(word);
	}

	return 0;
}




