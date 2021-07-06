#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <time.h>
#include <string.h>

#include <signal.h>

#define FREE_PAGE '-' // '-' => asc(45)
#define ID_POOL_START '0'

typedef struct MSegment {
	struct MSegment* next; // 8
	uint32_t size; // 4
	char id; // 1
	uint8_t free; // 1
} MSegment;

uint32_t MEMSIZE;
uint32_t MEMUSED = 0;
uint32_t MAX_PAGES;
char next_id = ID_POOL_START;
MSegment* head_segment;

void abort_handle(int sig);
void zahtjev();
void oslobodi();
void garbage();
void show_memory();
uint32_t _find_min_free(MSegment** minseg, uint32_t siz);
void merge_segment(MSegment* prev, MSegment* curr);
void _debug_segments();


int main(int argc, char* argv[])
{
	if (argc < 2) return -1;
	MEMSIZE = atoi(argv[1]);

	if (MEMSIZE < 2) {
		printf("> That makes no sense... I'm heading tf out, make it at least 2 dude\n");
		return -1;
	}

	srand(time(NULL));

	MAX_PAGES = (int)(0.10f * MEMSIZE);

	char MEM[MEMSIZE+1];
	memset(&MEM, FREE_PAGE, MEMSIZE);
	MEM[MEMSIZE] = '\0';

	head_segment = malloc(sizeof(MSegment));
	head_segment->next = NULL;
	head_segment->size = MEMSIZE;
	head_segment->id = FREE_PAGE;
	head_segment->free = 1;

	#ifdef _WIN32
		signal(SIGINT, abort_handle);
	#else
		sigset(SIGINT, abort_handle);
	#endif

	char cmd;
	while (1) {
		printf("\n> Command: (Z/O/G) ");
		scanf(" %c", &cmd);

		switch (cmd) {
			case 'Z':
			case 'z':
				zahtjev();
				break;

			case 'O':
			case 'o':
				oslobodi();
				break;

			case 'G':
			case 'g':
				garbage();
				break;

			case 'D':
			case 'd':
				_debug_segments();
				break;

			default:
				printf("> Enter a valid command\n");
				break;
		}
		show_memory();
	}
	return 0;
}

void abort_handle(int sig)
{
	uint32_t cnt = 0;
	MSegment* curr, *next = head_segment;
	do {
		curr = next;
		next = next->next;
		free(curr);
		cnt++;

	} while (next != NULL);

	printf("\n> Freed %d segments (%d bytes)\n", cnt, cnt*sizeof(MSegment));
	exit(0);
}

void show_memory()
{
	char memory[MEMSIZE+1];
	memory[MEMSIZE] = '\0';

	MSegment* curr, *next = head_segment;
	uint32_t offset = 0;
	while (1) {
		curr = next;
		next = next->next;

		memset(memory + offset, curr->id, curr->size);

		if (next == NULL)
			break;
		
		offset += curr->size;
	}

	printf("\n%s %d/%d (%.2f%%)\n", memory, MEMUSED, MEMSIZE, (float)MEMUSED / MEMSIZE * 100);
}

void _debug_segments()
{
	uint32_t cnt = 0;
	MSegment* ptr = head_segment;
	while (ptr != NULL) {
		printf("(%p) %p, %d, %c, %d\n", ptr,
										ptr->next,
										ptr->size,
										ptr->id,
										ptr->free);
		cnt++;
		ptr = ptr->next;
	}
	printf("=> %d segments.\n", cnt);
}

uint32_t _find_min_free(MSegment** minseg, uint32_t siz)
{
	MSegment* curr, *next = head_segment;
	*minseg = NULL;
	uint32_t offset = 0;
	while (1) {
		curr = next;
		next = next->next;

		if (curr->free && curr->size >= siz) {
			if ((*minseg) == NULL || curr->size < (*minseg)->size)
				*minseg = curr;
		}

		if (next == NULL)
			break;

		offset += curr->size;
	}
	return offset;
}

void zahtjev()
{
	// Rand new size in pages
	uint32_t siz = rand() % MAX_PAGES + 1;
	printf("Novi zahtjev %c za %d spremnickih mjesta\n", next_id, siz);

	MSegment* minseg = NULL;
	uint32_t offset = _find_min_free(&minseg, siz);

	if (!minseg) {
		garbage();
		offset = _find_min_free(&minseg, siz);

		if (!minseg) {
			printf("> Cannot allocate %d pages\n", siz);
			return;
		}
	}
	minseg->id = next_id;
	minseg->free = 0;

	// If it's smaller, shrink original segment
	// Otherwise they are equal size
	if (siz < minseg->size) {
		MSegment* freeseg = malloc(sizeof(MSegment));
		freeseg->next = minseg->next;
		freeseg->size = minseg->size - siz;
		freeseg->id = FREE_PAGE;
		freeseg->free = 1;

		minseg->size = siz;
		minseg->next = freeseg;
	}

	next_id++;
	MEMUSED += siz;
}

void merge_segment(MSegment* prev, MSegment* curr)
{
	if (curr == NULL)
		return;

	MSegment* next = curr->next;

	// Check right
	if (next != NULL && next->free) {
		// Append right segment to curr
		curr->size += next->size;
		
		curr->next = next->next;

		free(next);
		next = curr->next;
	}

	// Check left
	if (prev != NULL && prev->free) {
		prev->size += curr->size;
		
		prev->next = next;
		
		free(curr);
		if (next != NULL)
			curr = prev;
		else
			curr = NULL;
	}
}

void oslobodi()
{
	char zah;
	printf("Koji zahtjev treba osloboditi?\n");
	scanf(" %c", &zah);
	printf("Oslobada se zahtjev %c\n", zah);

	MSegment* prev = NULL, *curr = head_segment;
	do {
		if (curr->id != zah || curr->free) // Allow using same id character
			continue; 					   // used to indicate free segments

		MEMUSED -= curr->size;
		
		curr->id = FREE_PAGE;
		curr->free = 1;
		merge_segment(prev, curr);
		break;

	} while ((prev = curr, curr = curr->next) != NULL);
}

void garbage()
{ // There are at least 2 slots (3 with \0)
	while (1) {
	// Find minimal (empty) offset (min + startfrom)
		MSegment* ffree_prev = NULL, *curr = head_segment;
		MSegment* first_free = NULL;
		uint32_t free_offset = 0;

		while (1) {
			if (curr->free) {
				first_free = curr;
				break;
			}

			if (curr->next == NULL) // Needed if none are free
				break;

			free_offset += curr->size;
			ffree_prev = curr;
			curr = curr->next;
		}
		if (!first_free || !first_free->next)
			break; // return

	// Find first following full offset
		curr = first_free->next;
		MSegment* first_full = NULL;
		uint32_t full_offset = free_offset + first_free->size;

		while (1) {
			if (!curr->free) {
				first_full = curr;
				break;
			}

			if (curr->next == NULL)
				break;

			full_offset += curr->size;
			curr = curr->next;
		}
		if (!first_full)
			break; //return

	// Swap segments
		// Empty segment is right before the occupied one
		MSegment* tmp_prev, *tmp_next;

		// Update head_segment if needed !
		if (first_free == head_segment)
			head_segment = first_full;

		first_free->next = first_full->next;
		first_full->next = first_free;
		if (ffree_prev != NULL)
			ffree_prev->next = first_full;

// Concat free space
		merge_segment(NULL, first_free);
	}
}
