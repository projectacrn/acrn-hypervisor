#define SBI_EXT_CONSOLE_PUTCHAR 0x01
#define SBI_EXT_CONSOLE_GETCHAR 0x02

#define SBI_FID_CONSOLE_PUTCHAR 0x0
#define SBI_FID_CONSOLE_GETCHAR 0x0

extern long sbi_call(long eid, long fid, long arg0, long arg1, long arg2, long arg3);

static void sbi_putchar(int ch) {
    sbi_call(SBI_EXT_CONSOLE_PUTCHAR, SBI_FID_CONSOLE_PUTCHAR, ch, 0, 0, 0);
}

static void sbi_puts(const char *str) {
    while (*str) {
        sbi_putchar(*str);
        str++;
    }
}

void print_hello_world(void) {
    sbi_puts("Hello World!\n");
}

