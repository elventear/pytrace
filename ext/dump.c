#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include "db.h"
#include "defs.h"
#include "ring.h"
#include "shared_ring.h"
#include "record.pb-c.h"
#include "dump.h"

Ring *ring;
RingReader *reader;
unsigned char *buf;

void dump_init() {
  reader = reader_malloc(ring);
  buf = malloc(MAX_RECORD_SIZE);
  db_init();
}

void dump_process_init() {
  ring = shared_ring_init(1);
  dump_init();
}

void dump_thread_init() {
  ring = ring_init_from_memory((void*) RING_ADDRESS, RB_SIZE);
  dump_init();
}

void print_record(Record *rec) {
  int i;
  for (i = 0; i < rec->depth; i++) {
    printf(" ");
  }
  printf("%f %s %s %d: ", rec->time, rec->module.data, rec->function.data, rec->lineno);
  for (i = 0; i < rec->n_arguments; i++) {
    printf("%s = %s", rec->arguments[i]->name.data, rec->arguments[i]->value.data);
    if (rec->n_arguments - 1 > i) {
      printf(", ");
    }
  }
  printf("\n");
}

int print_records = 0;
int should_stop = 0;
void dump() {
  int size, count=0;
  Record *rec;
  while (1) {
    switch (size = reader_read(reader, buf)) {
    case 0:
      db_commit();
      usleep(100);
      count = 0;
      if (should_stop) {
	db_commit();
	should_stop = 0;
	return;
      }
      break;
    case -1:
      break;
    default:
      count++;
      rec = record__unpack(NULL, size, buf);
      if (print_records) {
	print_record(rec);
      }
      db_handle_record(rec);
      if (COMMIT_INTERVAL < count) {
	count = 0;
	db_commit();
      }
    }
  }
}

int dump_thread_main() {
  dump_thread_init();
  dump();
}

pthread_t dump_thread;
void dump_main_in_thread() {
  should_stop = 0;
  pthread_create(&dump_thread, NULL, dump_thread_main, NULL);
}

void dump_stop() {
  should_stop = 1;
  while (1 == should_stop) {
    usleep(10);
  }
}
