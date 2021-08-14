#include <stdio.h>
#include "gui.h"
#include "uxn.h"

Uxn u;
extern char* rom;

#define MEM_COLS 16
#define MEM_ROWS 8
#define STACK_COLS 16
#define STACK_ROWS 16

static const char *errors[] = {"underflow", "overflow", "division by zero"};

static char mem[MEM_ROWS][MEM_COLS][3];
static char stack[STACK_ROWS][STACK_COLS][3];

static char log[65536];

static void write_log(const char *msg) {
  if (log[0]) { strcat(log, "\n"); }
  strcat(log, msg);
}

int
uxn_halt(Uxn *u, Uint8 error, char *name, int id)
{
	//fprintf(stderr, "Halted: %s %s#%04x, at 0x%04x\n", name, errors[error - 1], id, u->ram.ptr);
	u->ram.ptr = 0;
	return 0;
}

static int
load(Uxn *u, char *filepath)
{
	FILE *f;
	if(!(f = fopen(filepath, "rb")))
		return 0;
	fread(u->ram.dat + PAGE_PROGRAM, sizeof(u->ram.dat) - PAGE_PROGRAM, 1, f);
	fprintf(stderr, "Loaded %s\n", filepath);
	return 1;
}

static void uxn_memory(mu_Context *ctx) {
  if (mu_begin_window(ctx, "Memory", mu_rect(40, 40, 300, 200))) {
    for(int y = 0; y < MEM_ROWS; y++) {
      mu_layout_row(ctx, MEM_COLS, (int[]) {20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20}, 0);
      for(int x = 0; x < MEM_COLS; x++) {
        int id = y * MEM_COLS + x;
        mu_push_id(ctx, &id, sizeof(id));
        mu_textbox(ctx, mem[y][x], sizeof(mem[y][x]));
        mu_pop_id(ctx);
      }
    }
    mu_end_window(ctx);
  }
}

static void uxn_stack(mu_Context *ctx) {
  if (mu_begin_window(ctx, "Stack", mu_rect(100, 100, 300, 200))) {
    for(int y = 0; y < STACK_ROWS; y++) {
      mu_layout_row(ctx, STACK_COLS, (int[]) {20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20}, 0);
      for(int x = 0; x < STACK_COLS; x++) {
        int id = y * STACK_COLS + x;
        mu_push_id(ctx, &id, sizeof(id));
        mu_textbox(ctx, stack[y][x], sizeof(stack[y][x])); 
        mu_pop_id(ctx);
      }
    }
    mu_end_window(ctx);
  }
}

static void controls(mu_Context *ctx)
{
   if (mu_begin_window(ctx, "Controls", mu_rect(40, 300, 300, 200))) {
     mu_layout_row(ctx, 1, (int[]) { -1 }, 100);
     mu_text(ctx, log);
     mu_layout_row(ctx, 3, (int[]){40, 40, 40}, 20);
     if(mu_button(ctx, "log"))
      write_log("test log");
     if(mu_button(ctx, "boot")) {
       if(!uxn_boot(&u))
        write_log("Boot failed");
     }

     if(mu_button(ctx, "load")) {
       write_log(rom);
       if(!load(&u, rom))
        write_log("Load failed");
     }
     mu_end_window(ctx);
   }
}

void fill_textboxes()
{
  for(int y = 0; y < MEM_ROWS; y++)
    for(int x = 0; x < MEM_COLS; x++)
      snprintf(mem[y][x], 3, "%.2X", u.ram.dat[0x100 + y * 16 + x]);
  for(int y = 0; y < STACK_COLS; y++)
    for(int x = 0; x < STACK_COLS; x++)
      snprintf(stack[y][x], 3, "%.2X", u.wst.dat[y * STACK_COLS + x]);
}

void process_frame(mu_Context *ctx) {
  mu_begin(ctx);
  fill_textboxes();
  uxn_memory(ctx);
  uxn_stack(ctx);
  controls(ctx);
  mu_end(ctx);
}