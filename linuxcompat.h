
#define SCREEN_XDIM 132
#define SCREEN_YDIM 132

void FbInit(void);
void plot_point(int x, int y, void *context);
void FbSwapBuffers(void);
void FbLine(unsigned char x1, unsigned char y1, unsigned char x2, unsigned char y2);
void FbHorizontalLine(unsigned char x1, unsigned char y1, unsigned char x2, unsigned char y2);
void FbVerticalLine(unsigned char x1, unsigned char y1, unsigned char x2, unsigned char y2);
void FbClear(void);
void restore_original_input_mode(void);
void set_nonblocking_input_mode(void);
int wait_for_keypress(void);
int get_keypress(void);
void FbMove(unsigned char x, unsigned char y);
void FbWriteLine(char *s);
void itoa(char *string, int value, int base);
int abs(int x);
void returnToMenus(void);
void FbColor(int color);

/* This is tentative.  Not really sure how the IR stuff works.
   For now, I will assume I register a callback to receive 32-bit
   packets incoming from IR sensor. */
void register_ir_packet_callback(void (*callback)(unsigned int));
void unregister_ir_packet_callback(void (*callback)(unsigned int));
void setup_ir_sensor(void);
void disable_interrupts(void);
void enable_interrupts(void);
void send_ir_packet(unsigned int packet);
unsigned int get_badge_id(void);

#define BLUE    0
#define GREEN   1
#define RED     2
#define BLACK   3
#define WHITE   4
#define CYAN    5
#define YELLOW  6
#define MAGENTA 7

