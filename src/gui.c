#include "gui.h"
#include "uxn.h"
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <stdarg.h>
#include <string.h>

Uxn u;
static Device *devsystem, *devconsole;
extern char *rom;

static const char *op_name[] = {
	"BRK", "LIT", "POP", "DUP", "NIP", "SWP", "OVR", "ROT",
	"EQU", "NEQ", "GTH", "LTH", "JMP", "JCN", "JSR", "STH", 
	"LDZ", "STZ", "LDR", "STR", "LDA", "STA", "DEI", "DEO",
	"ADD", "SUB", "MUL", "DIV", "AND", "ORA", "EOR", "SFT"
};

#define MEM_COLS 16
#define MEM_ROWS 8
#define STACK_COLS 16
#define STACK_ROWS 16

mu_Color RAM_PTR_COLOR = {255, 0, 0, 127};
mu_Color WST_PTR_COLOR = {255, 0, 0, 127};
mu_Color RST_PTR_COLOR = {255, 0, 0, 127};


static char mem[MEM_ROWS][MEM_COLS][3];
int mem_offset = 0x100;
static char stack[STACK_ROWS][STACK_COLS][3];

static char console_buffer[65536];
static int console_updated = 0;

static void console(const char *format, ...) {
  char msg[1024];
  va_list args;
  va_start(args, format);
  vsnprintf(msg, sizeof(msg), format, args);
  va_end(args);
  strcat(console_buffer, msg);
  console_updated = 1;
}

static int
error(char *msg, const char *err)
{
	console("Error %s: %s\n", msg, err);
	return 0;
}

static const char *errors[] = {"underflow", "overflow", "division by zero"};

int uxn_halt(Uxn *u, Uint8 error, char *name, int id) {
  console("Halted: %s %s#%04x, at 0x%04x\n", name, errors[error - 1],
  id, u->ram.ptr);
  u->ram.ptr = 0;
  return 0;
}

static int load(Uxn *u, char *filepath) {
  FILE *f;
  if (!(f = fopen(filepath, "rb")))
    return 0;
  fread(u->ram.dat + PAGE_PROGRAM, sizeof(u->ram.dat) - PAGE_PROGRAM, 1, f);
  console("Loaded %s\n", filepath);
  return 1;
}

static void
system_talk(Device *d, Uint8 b0, Uint8 w)
{
	if(!w) {
		d->dat[0x2] = d->u->wst.ptr;
		d->dat[0x3] = d->u->rst.ptr;
	} else if(b0 == 0xe) {
		Uint8 x, y;
		console("\n\n");
		for(y = 0; y < 0x08; ++y) {
			for(x = 0; x < 0x08; ++x) {
				Uint8 p = y * 0x08 + x;
				console(
					p == d->u->wst.ptr ? "[%02x]" : " %02x ",
					d->u->wst.dat[p]);
			}
			console("\n");
		}
	} else if(b0 == 0xf)
		d->u->ram.ptr = 0x0000;
}

static void
console_talk(Device *d, Uint8 b0, Uint8 w)
{
	if(w && b0 > 0x7)
		write(b0 - 0x7, (char *)&d->dat[b0], 1);
}

static void
file_talk(Device *d, Uint8 b0, Uint8 w)
{
	Uint8 read = b0 == 0xd;
	if(w && (read || b0 == 0xf)) {
		char *name = (char *)&d->mem[mempeek16(d->dat, 0x8)];
		Uint16 result = 0, length = mempeek16(d->dat, 0xa);
		Uint16 offset = mempeek16(d->dat, 0x4);
		Uint16 addr = mempeek16(d->dat, b0 - 1);
		FILE *f = fopen(name, read ? "r" : (offset ? "a" : "w"));
		if(f) {
			console("%s %s %s #%04x, ", read ? "Loading" : "Saving", name, read ? "to" : "from", addr);
			if(fseek(f, offset, SEEK_SET) != -1)
				result = read ? fread(&d->mem[addr], 1, length, f) : fwrite(&d->mem[addr], 1, length, f);
			console("%04x bytes\n", result);
			fclose(f);
		}
		mempoke16(d->dat, 0x2, result);
	}
}

static void
datetime_talk(Device *d, Uint8 b0, Uint8 w)
{
	time_t seconds = time(NULL);
	struct tm *t = localtime(&seconds);
	t->tm_year += 1900;
	mempoke16(d->dat, 0x0, t->tm_year);
	d->dat[0x2] = t->tm_mon;
	d->dat[0x3] = t->tm_mday;
	d->dat[0x4] = t->tm_hour;
	d->dat[0x5] = t->tm_min;
	d->dat[0x6] = t->tm_sec;
	d->dat[0x7] = t->tm_wday;
	mempoke16(d->dat, 0x08, t->tm_yday);
	d->dat[0xa] = t->tm_isdst;
	(void)b0;
	(void)w;
}

static void
nil_talk(Device *d, Uint8 b0, Uint8 w)
{
	(void)d;
	(void)b0;
	(void)w;
}

static void
run(Uxn *u)
{
	if(!uxn_eval(u, PAGE_PROGRAM))
		error("Reset", "Failed");
	else if(mempeek16(devconsole->dat, 0))
		while(read(0, &devconsole->dat[0x2], 1) > 0)
			uxn_eval(u, mempeek16(devconsole->dat, 0));
}

static void uxn_devices()
{
  devsystem = uxn_port(&u, 0x0, "system", system_talk);
	devconsole = uxn_port(&u, 0x1, "console", console_talk);
	uxn_port(&u, 0x2, "empty", nil_talk);
	uxn_port(&u, 0x3, "empty", nil_talk);
	uxn_port(&u, 0x4, "empty", nil_talk);
	uxn_port(&u, 0x5, "empty", nil_talk);
	uxn_port(&u, 0x6, "empty", nil_talk);
	uxn_port(&u, 0x7, "empty", nil_talk);
	uxn_port(&u, 0x8, "empty", nil_talk);
	uxn_port(&u, 0x9, "empty", nil_talk);
	uxn_port(&u, 0xa, "file", file_talk);
	uxn_port(&u, 0xb, "datetime", datetime_talk);
	uxn_port(&u, 0xc, "empty", nil_talk);
	uxn_port(&u, 0xd, "empty", nil_talk);
	uxn_port(&u, 0xe, "empty", nil_talk);
	uxn_port(&u, 0xf, "empty", nil_talk);
}

static void uxn_memory(mu_Context *ctx) {
  mu_Color saved_base_color = ctx->style->colors[MU_COLOR_BASE];
  if (mu_begin_window(ctx, "Memory", mu_rect(0, 0, 400, 260))) {
    mu_layout_row(ctx, 2, (int[]){40, 40}, 0);
    mu_text(ctx, "offset:");
    static char buffer[5];
    snprintf(buffer, sizeof(buffer), "%X", mem_offset);
    if(mu_textbox(ctx, buffer, 5))
      mem_offset = strtol(buffer, NULL, 16);
    mu_text(ctx, "op:");
    static opname_buf[10];
    Uint8 op = u.ram.dat[u.ram.ptr];
    snprintf(opname_buf, sizeof(opname_buf), op_name[u.ram.dat[u.ram.ptr] & 0x1f]);
    if(op & 0x20)
      strcat(opname_buf, "2");
    if(op & 0x80)
      strcat(opname_buf, "k");
    if(op & 0x40)
      strcat(opname_buf, "r");
    mu_textbox(ctx, opname_buf, sizeof(opname_buf));
    mu_layout_row(
        ctx, MEM_COLS,
        (int[]){20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20},
        0);
    for (int y = 0; y < MEM_ROWS; y++) {
      for (int x = 0; x < MEM_COLS; x++) {
        if(mem_offset + y * MEM_COLS + x == u.ram.ptr)
          ctx->style->colors[MU_COLOR_BASE] = RAM_PTR_COLOR;
        else
          ctx->style->colors[MU_COLOR_BASE] = saved_base_color;
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
  mu_Color saved_base_color = ctx->style->colors[MU_COLOR_BASE];
  if (mu_begin_window(ctx, "Stack", mu_rect(405, 0, 400, 260))) {
    for (int y = 0; y < STACK_ROWS; y++) {
      mu_layout_row(ctx, STACK_COLS,
                    (int[]){20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20,
                            20, 20, 20},
                    0);
      for (int x = 0; x < STACK_COLS; x++) {
        if(y * MEM_COLS + x == u.wst.ptr)
          ctx->style->colors[MU_COLOR_BASE] = WST_PTR_COLOR;
        else
          ctx->style->colors[MU_COLOR_BASE] = saved_base_color;
        int id = y * STACK_COLS + x;
        mu_push_id(ctx, &id, sizeof(id));
        mu_textbox(ctx, stack[y][x], sizeof(stack[y][x]));
        mu_pop_id(ctx);
      }
    }
    mu_end_window(ctx);
  }
}

static void controls(mu_Context *ctx) {
  if (mu_begin_window(ctx, "Controls", mu_rect(40, 300, 300, 200))) {
    mu_text(ctx, "Console");
    mu_layout_row(ctx, 1, (int[]){-1}, -25);
    mu_begin_panel(ctx, "Console");
    mu_Container *panel = mu_get_current_container(ctx);
    mu_layout_row(ctx, 1, (int[]){-1}, -1);
    mu_text(ctx, console_buffer);
    mu_end_panel(ctx);
    if (console_updated) {
      panel->scroll.y = panel->content_size.y;
      console_updated = 0;
    }

    mu_layout_row(ctx, 4, (int[]){40, 40, 40, 40}, 20);
    if (mu_button(ctx, "boot")) {
      if (!uxn_boot(&u))
        console("Boot failed");
    }
    if (mu_button(ctx, "load")) {
      if (!load(&u, rom))
        console("Load failed");
    }
    if(mu_button(ctx, "devices"))
      uxn_devices(&u);
    if(mu_button(ctx, "run"))
      run(&u);
    if(mu_button(ctx, "init step"))
      uxn_init_eval(&u, PAGE_PROGRAM);
    if(mu_button(ctx, "step"))
      uxn_step(&u);
    mu_end_window(ctx);
  }
}

void fill_textboxes() {
  for (int y = 0; y < MEM_ROWS; y++)
    for (int x = 0; x < MEM_COLS; x++)
      snprintf(mem[y][x], 3, "%.2X", u.ram.dat[mem_offset + y * MEM_COLS + x]);
  for (int y = 0; y < STACK_COLS; y++)
    for (int x = 0; x < STACK_COLS; x++)
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