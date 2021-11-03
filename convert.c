#include <yed/plugin.h>
#include <inttypes.h>

/*
Signed 32 MAX
Int   2147483647
Hex   0x7FFFFFFF
BIN   0b1111111111111111111111111111111
OCT   017777777777
2COMP 0b01111111111111111111111111111111

Signed 32 MIN
INT  -2147483648
HEX  -0x80000000
BIN  -0b10000000000000000000000000000000
OCT  -020000000000
2COMP 0b10000000000000000000000000000000

Unsigned 32 MAX
INT   4294967295
HEX   0xFFFFFFFF
BIN   0b11111111111111111111111111111111
OCT   037777777777
2COMP 0b0000000000000000000000000000000011111111111111111111111111111111

Signed 64 MAX
INT   9223372036854775807
HEX   0x7FFFFFFFFFFFFFFF
BIN   0b111111111111111111111111111111111111111111111111111111111111111
OCT   0777777777777777777777
2COMP 0b0111111111111111111111111111111111111111111111111111111111111111

Signed 64 MIN
INT  -9223372036854775808
HEX  -0x8000000000000000
BIN  -0b1000000000000000000000000000000000000000000000000000000000000000
OCT  -01000000000000000000000
2COMP 0b1000000000000000000000000000000000000000000000000000000000000000

Unsigned 64 MAX
INT   18446744073709551615
HEX   0xFFFFFFFFFFFFFFFF
BIN   0b1111111111111111111111111111111111111111111111111111111111111111
OCT   01777777777777777777777
2COMP Overflow

Uppercase
loWERCASE
camelCase_oopsTHINGS_GETmessySometimes
*/

#define TWOSCOMP 0

typedef struct {
   yed_frame *frame;
   array_t    strings;
   array_t    dds;
   int        start_len;
   int        is_up;
   int        row;
   int        selection;
   int        size;
   int        cursor_row;
   int        cursor_col;
} convert_popup_t;

static enum {
    integer,
    hexadecimal,
    binary,
    octal,
    twoscomp
} n_type;

static enum {
    unsigned_64,
    signed_64,
    unsigned_32,
    signed_32
} d_type;

typedef struct {
    char     word[512];   //the number as a string
    int      word_start;  //col the number starts on
    int      word_len;    //the length of the word
    int      row;         //row the number is on
    int      negative;    //0 not negative 1 negative
    int      number_type; //n_type
    int      data_type;   //d_type
    int      is_word;     //if its a number or a word
    uint64_t num_ull;     //stored number u64
    int64_t  num_ll;      //stored number s64
    uint32_t num_uint;    //stored number u32
    int32_t  num_int;     //stored number s32
} convert_word;

static char             *word_type_arr[6] = {"uppercase", "lowercase", "snakecase", "camelcase", "error"};
static char             *num_type_arr[6] = {"integer", "hexadecimal", "binary", "octal", "2scomp", "error"};
static char             *data_type_arr[5] = {"unsigned_64", "signed_64", "unsigned_32", "signed_32", "error"};
static convert_popup_t   popup;
static array_t           popup_items;
static array_t           converted_items;
static yed_event_handler h_key;
static int               popup_is_up;
static convert_word      converted_word;

/* Internal Functions*/
static int         check_int(char *number);
static int         check_hex(char *number);
static int         check_bin(char *number);
static int         check_oct(char *number);
static int         isbdigit(char c);
static int         isodigit(char c);
static void        bin_to_int(char *bin);
static void        draw_popup(void);
static void        start_popup(yed_frame *frame, int start_len, array_t strings);
static void        kill_popup(void);
static char       *print_bits();
static char       *print_twos_bits();
static yed_buffer *get_or_make_buff(void);
static int         convert_word_at_point(yed_frame *frame, int row, int col);
static int         convert_word_at_point_2(yed_frame *frame, int row, int col);
static int         convert_find_size(void);
static void        init_convert(void);
static void        print_converted_struct(void);
static char       *snprintf_int(char *tmp_buffer);
static char       *snprintf_hex(char *tmp_buffer);
static char       *snprintf_oct(char *tmp_buffer);

/* Event Handlers */
static void key_handler(yed_event *event);

/* Global Functions */
void convert_number(int nargs, char** args);

int yed_plugin_boot(yed_plugin *self) {
    YED_PLUG_VERSION_CHECK();

    popup_items = array_make(char *);
    converted_items = array_make(char *);

    h_key.kind = EVENT_KEY_PRESSED;
    h_key.fn   = key_handler;
    yed_plugin_add_event_handler(self, h_key);

    yed_plugin_set_command(self, "convert-number", convert_number);

    return 0;
}

int convert_find_size(void) {
    char tmp_buff[512];
    LOG_FN_ENTER();
    switch(converted_word.number_type) {
        case integer:
            if(converted_word.word_len < 10) {
                //s32
                converted_word.num_int = strtol(converted_word.word, NULL, 10);
                converted_word.data_type = signed_32;
                return 1;
            }else if(converted_word.word_len == 10) {
                if((strncmp(converted_word.word, "2147483648", 10) <= 0 && converted_word.negative == 1)
                || (strncmp(converted_word.word, "2147483647", 10) <= 0 && converted_word.negative == 0)) {
                    //s32
                    converted_word.num_int = strtol(converted_word.word, NULL, 10);
                    converted_word.data_type = signed_32;
                    return 1;
                }else if(strncmp(converted_word.word, "4294967295", 10) <= 0 && converted_word.negative == 0) {
                    //u32
                    converted_word.num_uint = strtoul(converted_word.word, NULL, 10);
                    converted_word.data_type = unsigned_32;
                    return 1;
                }else{
                    //s64
                    yed_cerr("%s", converted_word.word);
                    converted_word.num_ll = strtoll(converted_word.word, NULL, 10);
                    converted_word.data_type = signed_64;
                    return 1;
                }
            }else if(converted_word.word_len < 19) {
                //s64
                converted_word.num_ll = strtoll(converted_word.word, NULL, 10);
                converted_word.data_type = signed_64;
                return 1;
            }else if(converted_word.word_len == 19) {
                if((strncmp(converted_word.word, "9223372036854775808", 19) <= 0 && converted_word.negative == 1)
                || (strncmp(converted_word.word, "9223372036854775807", 19) <= 0 && converted_word.negative == 0)) {
                    //s64
                    if(strncmp(converted_word.word, "9223372036854775808", 19) <= 0 && converted_word.negative == 1) {
                        snprintf(tmp_buff, 512, "-%s", converted_word.word);
                        converted_word.num_ll = strtoll(tmp_buff, NULL, 10);
                    }else{
                        converted_word.num_ll = strtoll(converted_word.word, NULL, 10);
                    }
                    converted_word.data_type = signed_64;
                    return 1;
                }else{
                    //u64
                    converted_word.num_ull = strtoull(converted_word.word, NULL, 10);
                    converted_word.data_type = unsigned_64;
                    return 1;
                }
            }else if(converted_word.word_len == 20) {
                if(strncmp(converted_word.word, "18446744073709551615", 20) <= 0 && converted_word.negative == 0) {
                    //u64
                    converted_word.num_ull = strtoull(converted_word.word, NULL, 10);
                    converted_word.data_type = unsigned_64;
                    return 1;
                }else{
                    goto overflow;
                }
            }else{
                goto overflow;
            }
            break;

        case hexadecimal:
            if(converted_word.word_len < 10) {
                //s32
                converted_word.num_int = strtol(converted_word.word, NULL, 16);
                converted_word.data_type = signed_32;
                return 1;
            }else if(converted_word.word_len == 10) {
                if((strncmp(converted_word.word, "0x80000000", 10) <= 0 && converted_word.negative == 1)
                || (strncmp(converted_word.word, "0x7FFFFFFF", 10) <= 0 && converted_word.negative == 0)) {
                    //s32
                    converted_word.num_int = strtol(converted_word.word, NULL, 16);
                    converted_word.data_type = signed_32;
                    return 1;
                }else if(strncmp(converted_word.word, "0xFFFFFFFF", 10) <= 0 && converted_word.negative == 0) {
                    //u32
                    converted_word.num_uint = strtoul(converted_word.word, NULL, 16);
                    converted_word.data_type = unsigned_32;
                    return 1;
                }else{
                    //s64
                    yed_cerr("%s", converted_word.word);
                    converted_word.num_ll = strtoll(converted_word.word, NULL, 16);
                    converted_word.data_type = signed_64;
                    return 1;
                }
            }else if(converted_word.word_len < 18) {
                //s64
                converted_word.num_ll = strtoll(converted_word.word, NULL, 16);
                converted_word.data_type = signed_64;
                return 1;
            }else if(converted_word.word_len == 18) {
                if((strncmp(converted_word.word, "0x8000000000000000", 18) <= 0 && converted_word.negative == 1)
                || (strncmp(converted_word.word, "0x7FFFFFFFFFFFFFFF", 18) <= 0 && converted_word.negative == 0)) {
                    //s64
                    if(strncmp(converted_word.word, "0x8000000000000000", 18) <= 0 && converted_word.negative == 1) {
                        snprintf(tmp_buff, 512, "-%s", converted_word.word);
                        converted_word.num_ll = strtoll(tmp_buff, NULL, 16);
                    }else{
                        converted_word.num_ll = strtoll(converted_word.word, NULL, 16);
                    }
                    converted_word.data_type = signed_64;
                    return 1;
                }else if(strncmp(converted_word.word, "0xFFFFFFFFFFFFFFFF", 18) <= 0 && converted_word.negative == 0) {
                    //u64
                    converted_word.num_ull = strtoull(converted_word.word, NULL, 16);
                    converted_word.data_type = unsigned_64;
                    return 1;
                }
            }else{
                goto overflow;
            }
            break;

        case binary:
            if(converted_word.word_len <= 33) {
                //s32
                converted_word.data_type = signed_32;
                bin_to_int(converted_word.word);
                return 1;
            }else if(converted_word.word_len == 34) {
                if(strncmp(converted_word.word, "10000000000000000000000000000000", 34) <= 0 && converted_word.negative == 1) {
                    //s32
                    converted_word.data_type = signed_32;
                    bin_to_int(converted_word.word);
                    return 1;
                }else {
                    //u32
                    converted_word.data_type = unsigned_32;
                    bin_to_int(converted_word.word);
                    return 1;
                }
            }else if(converted_word.word_len <= 65) {
                //s64
                converted_word.data_type = signed_64;
                bin_to_int(converted_word.word);
                return 1;
            }else if(converted_word.word_len == 66) {
                if(strncmp(converted_word.word, "1000000000000000000000000000000000000000000000000000000000000000", 64) <= 0 && converted_word.negative == 1) {
                    //s64
                    converted_word.data_type = signed_64;
                    bin_to_int(converted_word.word);
                    return 1;
                }else{
                    //u64
                    converted_word.data_type = unsigned_64;
                    bin_to_int(converted_word.word);
                    return 1;
                }
            }else{
                goto overflow;
            }
            break;

        case octal:
            if(converted_word.word_len < 12) {
                //s32
                converted_word.num_int = strtol(converted_word.word, NULL, 8);
                converted_word.data_type = signed_32;
                return 1;
            }else if(converted_word.word_len == 12) {
                if((strncmp(converted_word.word, "020000000000", 12) <= 0 && converted_word.negative == 1)
                || (strncmp(converted_word.word, "017777777777", 12) <= 0 && converted_word.negative == 0)) {
                    //s32
                    converted_word.num_int = strtol(converted_word.word, NULL, 8);
                    converted_word.data_type = signed_32;
                    return 1;
                }else if(strncmp(converted_word.word, "037777777777", 12) <= 0 && converted_word.negative == 0) {
                    //u32
                    converted_word.num_uint = strtoul(converted_word.word, NULL, 8);
                    converted_word.data_type = unsigned_32;
                    return 1;
                }else{
                    //s64
                    yed_cerr("%s", converted_word.word);
                    converted_word.num_ll = strtoll(converted_word.word, NULL, 8);
                    converted_word.data_type = signed_64;
                    return 1;
                }
            }else if(converted_word.word_len < 22) {
                //s64
                converted_word.num_ll = strtoll(converted_word.word, NULL, 8);
                converted_word.data_type = signed_64;
                return 1;
            }else if(converted_word.word_len == 22) {
                if(strncmp(converted_word.word, "0777777777777777777777", 22) <= 0 && converted_word.negative == 0) {
                    converted_word.num_ll = strtoll(converted_word.word, NULL, 8);
                    converted_word.data_type = signed_64;
                    return 1;
                }else if(strncmp(converted_word.word, "01777777777777777777777", 23) <= 0 && converted_word.negative == 0) {
                    //u64
                    converted_word.num_ull = strtoull(converted_word.word, NULL, 8);
                    converted_word.data_type = unsigned_64;
                    return 1;
                }
            }else if(converted_word.word_len == 23) {
                if(strncmp(converted_word.word, "01000000000000000000000", 23) <= 0 && converted_word.negative == 1) {
                    //s64
                    if(strncmp(converted_word.word, "01000000000000000000000", 23) <= 0 && converted_word.negative == 1) {
                        snprintf(tmp_buff, 512, "-%s", converted_word.word);
                        converted_word.num_ll = strtoll(tmp_buff, NULL, 8);
                    }else{
                        converted_word.num_ll = strtoll(converted_word.word, NULL, 8);
                    }
                    converted_word.data_type = signed_64;
                    return 1;
                }else if(strncmp(converted_word.word, "01777777777777777777777", 23) <= 0 && converted_word.negative == 0) {
                    //u64
                    converted_word.num_ull = strtoull(converted_word.word, NULL, 8);
                    converted_word.data_type = unsigned_64;
                    return 1;
                }
            }else{
                goto overflow;
            }
            break;

        default:
            goto no_num;
            break;
    }

    //Overflow
    if(0) {
overflow:;
/*         yed_cerr("Number to convert is too large."); */
        return -1;
    }

    //Not a number
    if(0) {
no_num:;
/*         yed_cerr("String can not be converted into a number."); */
        return 0;
    }

    LOG_EXIT();
    return 1;
}

void init_convert(void) {
    memset(&converted_word.word[0], 0, 512);
    converted_word.word_start =  -1;
    converted_word.word_len =    -1;
    converted_word.row =         -1;
    converted_word.negative =    -1;
    converted_word.number_type = -1;
    converted_word.data_type =   -1;
    converted_word.num_ull =      0;
    converted_word.num_ll =       0;
    converted_word.num_uint =     0;
    converted_word.num_int =      0;
    converted_word.number_type =  5;
}

void convert_number(int nargs, char** args) {
    char *item;
    char  buffer[512];
    char  tmp_buffer[512];
    int   number;
    int   number_type;
    int   word_break;
    int   uppercase_last;
    int   extra;
    int   con;
    yed_frame *frame;

    frame = ys->active_frame;

    if (!frame
    ||  !frame->buffer) {
        return;
    }

    init_convert();
    if(convert_word_at_point(frame, frame->cursor_line, frame->cursor_col) == 0) {
        return;
    }

    if(popup.is_up) {
        yed_cerr("Convert popup is already up!");
        return;
    }

    if(check_int(converted_word.word)) {
        converted_word.number_type = integer;
    }

    if(check_hex(converted_word.word)) {
        converted_word.number_type = hexadecimal;
    }

    if(check_bin(converted_word.word)) {
        converted_word.number_type = binary;
    }

    if(check_oct(converted_word.word)) {
        converted_word.number_type = octal;
    }
    con = convert_find_size();
    if( con == 1) {
        LOG_FN_ENTER();
        if(converted_word.data_type == signed_32) {
            yed_cprint("%d converted to int32_t from %s.", converted_word.num_int, num_type_arr[converted_word.number_type]);
        }else if(converted_word.data_type == unsigned_32) {
            yed_cprint("%u converted to uint32_t from %s.", converted_word.num_uint, num_type_arr[converted_word.number_type]);
        }else if(converted_word.data_type == signed_64) {
            yed_cprint("%lld converted to int64_t from %s.", converted_word.num_ll, num_type_arr[converted_word.number_type]);
        }else if(converted_word.data_type == unsigned_64) {
            yed_cprint("%llu converted to uint64_t from %s.", converted_word.num_ull, num_type_arr[converted_word.number_type]);
        }
        LOG_EXIT();

        while(array_len(popup_items) > 0) {
            array_pop(popup_items);
        }

        while(array_len(converted_items) > 0) {
            array_pop(converted_items);
        }

    /*  Integer */
        memcpy(tmp_buffer, snprintf_int(tmp_buffer), 512);
        item = strdup(tmp_buffer); array_push(converted_items, item);
        snprintf(buffer, 512, "%11s: %-20s", data_type_arr[converted_word.data_type], *((char **)array_item(converted_items, 0)));
        item = strdup(buffer); array_push(popup_items, item);

    /*  Hexadecimal     */
        memcpy(tmp_buffer, snprintf_hex(tmp_buffer), 512);
        item = strdup(tmp_buffer); array_push(converted_items, item);
        snprintf(buffer, 512, "%11s: %-20s", num_type_arr[1], *((char **)array_item(converted_items, 1)));
        item = strdup(buffer); array_push(popup_items, item);

    /*  Binary */
        item = print_bits(); array_push(converted_items, item);
        snprintf(buffer, 512, "%11s: %-20s", num_type_arr[2], *((char **)array_item(converted_items, 2)));
        item = strdup(buffer); array_push(popup_items, item);

    /*  Octal */
        memcpy(tmp_buffer, snprintf_oct(tmp_buffer), 512);
        item = strdup(tmp_buffer); array_push(converted_items, item);
        snprintf(buffer, 512, "%11s: %-20s", num_type_arr[3], *((char **)array_item(converted_items, 3)));
        item = strdup(buffer); array_push(popup_items, item);

    /*  Twos Compliment */
#if TWOSCOMP
        item = print_twos_bits(); array_push(converted_items, item);
        snprintf(buffer, 512, "%11s: %-20s", num_type_arr[4], *((char **)array_item(converted_items, 4)));
        item = strdup(buffer); array_push(popup_items, item);
#endif
        popup.size = array_len(popup_items);

        if (ys->active_frame->cur_y + popup.size >= ys->active_frame->top + ys->active_frame->height) {
            popup.row = ys->active_frame->cur_y - popup.size - 1;
        } else {
            popup.row = ys->active_frame->cur_y;
        }
        popup.cursor_col = ys->active_frame->cursor_col;

        start_popup(frame, array_len(popup_items), popup_items);
        popup.is_up = 1;
        return;
    }else if(con == -1){
        yed_cerr("Overflow!");
        return;
    }else if(convert_word_at_point_2(frame, frame->cursor_line, frame->cursor_col) == 1) {
        while(array_len(popup_items) > 0) {
            array_pop(popup_items);
        }

        while(array_len(converted_items) > 0) {
            array_pop(converted_items);
        }

    /*  Uppercase */
        memcpy(tmp_buffer, converted_word.word, 512);
        for(int i=0; i<converted_word.word_len; i++) {
            tmp_buffer[i] = toupper(tmp_buffer[i]);
        }
        item = strdup(tmp_buffer); array_push(converted_items, item);
        snprintf(buffer, 512, "%11s: %-20s", word_type_arr[0], *((char **)array_item(converted_items, 0)));
        item = strdup(buffer); array_push(popup_items, item);

    /*  Lowercase     */
        memcpy(tmp_buffer, converted_word.word, 512);
        for(int i=0; i<converted_word.word_len; i++) {
            tmp_buffer[i] = tolower(tmp_buffer[i]);
        }
        item = strdup(tmp_buffer); array_push(converted_items, item);
        snprintf(buffer, 512, "%11s: %-20s", word_type_arr[1], *((char **)array_item(converted_items, 1)));
        item = strdup(buffer); array_push(popup_items, item);

    /*  Snakecase */
        word_break = 0;
        uppercase_last = 0;
        extra = 0;
        memcpy(tmp_buffer, converted_word.word, 512);
        for(int i=0; i<converted_word.word_len+extra; i++) {
            if(tmp_buffer[i] >= 'A' && tmp_buffer[i] <= 'Z') {
                if(uppercase_last == 1) {
                    tmp_buffer[i] = tolower(tmp_buffer[i]);
                }else if(i == 0) {
                    tmp_buffer[i] = tolower(tmp_buffer[i]);
                }else{
                    tmp_buffer[i] = tolower(tmp_buffer[i]);
                    for(int j=converted_word.word_len+extra+2; j>i; j--) {
                        tmp_buffer[j] = tmp_buffer[j-1];
                    }
                    tmp_buffer[i] = '_';
                    extra++;
                    i++;
                }
                uppercase_last = 1;
            }else if(tmp_buffer[i] >= 'a' && tmp_buffer[i] <= 'z') {
                uppercase_last = 0;
            }
        }
        item = strdup(tmp_buffer); array_push(converted_items, item);
        snprintf(buffer, 512, "%11s: %-20s", word_type_arr[2], *((char **)array_item(converted_items, 2)));
        item = strdup(buffer); array_push(popup_items, item);

    /*  Camelcase */
        word_break = 0;
        uppercase_last = 0;
        extra = 0;
        memcpy(tmp_buffer, converted_word.word, 512);
        for(int i=0; i<converted_word.word_len-extra; i++) {
            if(tmp_buffer[i] >= 'A' && tmp_buffer[i] <= 'Z') {
                if(uppercase_last == 1) {
                    tmp_buffer[i] = tolower(tmp_buffer[i]);
                }else if(i == 0) {
                    tmp_buffer[i] = tolower(tmp_buffer[i]);
                }
                uppercase_last = 1;
            }else if(tmp_buffer[i] >= 'a' && tmp_buffer[i] <= 'z') {
                uppercase_last = 0;
            }else if(tmp_buffer[i] == '_') {
                for(int j=i; j<converted_word.word_len-extra; j++) {
                    tmp_buffer[j] = tmp_buffer[j+1];
                }
                tmp_buffer[i] = toupper(tmp_buffer[i]);
            }
        }
        item = strdup(tmp_buffer); array_push(converted_items, item);
        snprintf(buffer, 512, "%11s: %-20s", word_type_arr[3], *((char **)array_item(converted_items, 3)));
        item = strdup(buffer); array_push(popup_items, item);

        popup.size = array_len(popup_items);

        if (ys->active_frame->cur_y + popup.size >= ys->active_frame->top + ys->active_frame->height) {
            popup.row = ys->active_frame->cur_y - popup.size - 1;
        } else {
            popup.row = ys->active_frame->cur_y;
        }
        popup.cursor_col = ys->active_frame->cursor_col;

        start_popup(frame, array_len(popup_items), popup_items);
        popup.is_up = 1;
        return;
    }else{
        return;
    }
}

yed_buffer *get_or_make_buff(void) {
    yed_buffer *buff;

    buff = yed_get_buffer("*find-file-list");

    if (buff == NULL) {
        buff = yed_create_buffer("*find-file-list");
        buff->flags |= BUFF_RD_ONLY | BUFF_SPECIAL;
    }

    return buff;
}

void print_converted_struct(void) {
    LOG_FN_ENTER();
    yed_cprint("\n\nword:%s\n", converted_word.word);
    yed_cprint("word_start:%d\n", converted_word.word_start);
    yed_cprint("word_len:%d\n", converted_word.word_len);
    yed_cprint("row:%d\n", converted_word.row);
    yed_cprint("negative:%d\n", converted_word.negative);
    yed_cprint("number_type:%d\n", converted_word.number_type);
    yed_cprint("data_type:%d\n", converted_word.data_type);
    yed_cprint("num_ull:%llu\n", converted_word.num_ull);
    yed_cprint("num_ll:%lld\n", converted_word.num_ll);
    yed_cprint("num_uint:%u\n", converted_word.num_uint);
    yed_cprint("num_int:%d", converted_word.num_int);
    LOG_EXIT();
}

char *snprintf_int(char *tmp_buffer) {
    char buff[512];
    if(converted_word.data_type == signed_32) {
        snprintf(tmp_buffer, 512, "%d", converted_word.num_int);
    }else if(converted_word.data_type == unsigned_32) {
        snprintf(tmp_buffer, 512, "%u", converted_word.num_uint);
    }else if(converted_word.data_type == signed_64) {
        snprintf(tmp_buffer, 512, "%lld", converted_word.num_ll);
    }else if(converted_word.data_type == unsigned_64) {
        snprintf(tmp_buffer, 512, "%llu", converted_word.num_ull);
    }

    if(converted_word.negative == 1) {
        snprintf(buff, 512, "%s", tmp_buffer);
        if(buff[0] != '-') {
            snprintf(tmp_buffer, 512, "-%s", buff);
        }
    }
    return tmp_buffer;
}

char *snprintf_hex(char *tmp_buffer) {
    char buff[512];
    if(converted_word.data_type == signed_32) {
        if(converted_word.negative == 1) {
            snprintf(tmp_buffer, 512, "-0x%X", converted_word.num_int);
        }else{
            snprintf(tmp_buffer, 512, "0x%X", converted_word.num_int);
        }
    }else if(converted_word.data_type == unsigned_32) {
        snprintf(tmp_buffer, 512, "0x%X", converted_word.num_uint);
    }else if(converted_word.data_type == signed_64) {
        if(converted_word.negative == 1) {
            snprintf(tmp_buffer, 512, "-0x%" PRIX64 "", converted_word.num_ll);
        }else{
            snprintf(tmp_buffer, 512, "0x%" PRIX64 "", converted_word.num_ll);
        }
    }else if(converted_word.data_type == unsigned_64) {
        snprintf(tmp_buffer, 512, "0x%" PRIX64 "", converted_word.num_ull);
    }

    return tmp_buffer;
}

char *snprintf_oct(char *tmp_buffer) {
    char buff[512];
    if(converted_word.data_type == signed_32) {
        if(converted_word.negative == 1) {
            snprintf(tmp_buffer, 512, "-0%o", converted_word.num_int);
        }else{
            snprintf(tmp_buffer, 512, "0%o", converted_word.num_int);
        }
    }else if(converted_word.data_type == unsigned_32) {
        snprintf(tmp_buffer, 512, "0%o", converted_word.num_uint);
    }else if(converted_word.data_type == signed_64) {
        if(converted_word.negative == 1) {
            snprintf(tmp_buffer, 512, "-0%" PRIo64  "", converted_word.num_ll);
        }else{
            snprintf(tmp_buffer, 512, "0%" PRIo64 "", converted_word.num_ll);
        }
    }else if(converted_word.data_type == unsigned_64) {
        snprintf(tmp_buffer, 512, "0%" PRIo64  "", converted_word.num_ull);
    }

    return tmp_buffer;
}

int convert_word_at_point(yed_frame *frame, int row, int col) {
    yed_buffer *buff;
    yed_line   *line;
    int         word_len;
    char        c, *word_start, *ret;
    int         word_column;
    int         neg;

    if (frame == NULL || frame->buffer == NULL) { return 0; }

    buff = frame->buffer;

    line = yed_buff_get_line(buff, row);
    if (!line) { return 0; }

    if (col == line->visual_width + 1) { return 0; }

    c = ((yed_glyph*)yed_line_col_to_glyph(line, col))->c;

    if (isspace(c)) { return 0; }

    word_len = 1;
    neg = 0;

    if(isalnum(c)) {
        while (col > 1) {
            c = ((yed_glyph*)yed_line_col_to_glyph(line, col - 1))->c;
            if (!isalnum(c)) {
                break;
            }

            col -= 1;
        }
        if(col-1 >= 1) {
            c = ((yed_glyph*)yed_line_col_to_glyph(line, col - 1))->c;
            if(c == '-') {
                neg = 1;
            }
        }

        word_start  = array_item(line->chars, yed_line_col_to_idx(line, col));
        word_column = col;
        col        += 1;
        c           = ((yed_glyph*)yed_line_col_to_glyph(line, col))->c;
        while (col <= line->visual_width && (isalnum(c))) {
            word_len += 1;
            col      += 1;
            c         = ((yed_glyph*)yed_line_col_to_glyph(line, col))->c;
        }
    }else{
        return 0;
    }

    memcpy(converted_word.word, word_start, word_len);
    converted_word.word[word_len] = 0;
    converted_word.word_start = word_column;
    converted_word.word_len = word_len;
    converted_word.row = row;
    converted_word.negative = neg;
    converted_word.is_word = 0;
    return 1;
}

int convert_word_at_point_2(yed_frame *frame, int row, int col) {
    yed_buffer *buff;
    yed_line   *line;
    int         word_len;
    char        c, *word_start, *ret;
    int         word_column;

    if (frame == NULL || frame->buffer == NULL) { return 0; }

    buff = frame->buffer;

    line = yed_buff_get_line(buff, row);
    if (!line) { return 0; }

    if (col == line->visual_width + 1) { return 0; }

    c = ((yed_glyph*)yed_line_col_to_glyph(line, col))->c;

    if (isspace(c)) { return 0; }

    word_len = 0;

    if (isalnum(c) || c == '_') {
        while (col > 1) {
            c = ((yed_glyph*)yed_line_col_to_glyph(line, col - 1))->c;

            if (!isalnum(c) && c != '_') {
                break;
            }

            col -= 1;
        }
        word_start = array_item(line->chars, yed_line_col_to_idx(line, col));
        word_column = col;
        c = ((yed_glyph*)yed_line_col_to_glyph(line, col))->c;
        while (col <= line->visual_width
        &&    (isalnum(c) || c == '_')) {

            word_len += 1;
            col      += 1;
            c         = ((yed_glyph*)yed_line_col_to_glyph(line, col))->c;
        }
    } else {
        while (col > 1) {
            c = ((yed_glyph*)yed_line_col_to_glyph(line, col - 1))->c;

            if (isalnum(c) || c == '_' || isspace(c)) {
                break;
            }

            col -= 1;
        }
        word_start  = array_item(line->chars, yed_line_col_to_idx(line, col));
        word_column = col;
        c           = ((yed_glyph*)yed_line_col_to_glyph(line, col))->c;
        while (col <= line->visual_width
        &&    (!isalnum(c) && c != '_' && !isspace(c))) {

            word_len += 1;
            col      += 1;
            c         = ((yed_glyph*)yed_line_col_to_glyph(line, col))->c;
        }
    }

    memcpy(converted_word.word, word_start, word_len);
    converted_word.word[word_len] = 0;
    converted_word.word_start = word_column;
    converted_word.word_len = word_len;
    converted_word.row = row;
    converted_word.negative = -1;
    converted_word.is_word = 1;
    print_converted_struct();
    return 1;
}

void key_handler(yed_event *event) {
    yed_line  *line;
    yed_frame *frame;
    int        word_len;
    int        word_start;

    frame = ys->active_frame;

    if (frame == NULL || frame->buffer == NULL) { return; }

    if (ys->interactive_command != NULL) { return; }

    if(!popup.is_up) {return;}

    if (event->key == ESC) {
        kill_popup();
        popup.is_up = 0;
    }else if(event->key == ENTER) {
        kill_popup();
        popup.is_up = 0;
/*      add replace stuff here */
        if(converted_word.word == NULL) { return;}

        if(converted_word.is_word == 0) {
            word_len   = converted_word.word_len;
            word_start = converted_word.word_start;
            if(converted_word.negative == 1) {
                word_len++;
                word_start--;
            }

            for(int i=word_start+word_len-1; i>word_start-1; i--) {
                yed_delete_from_line(frame->buffer, converted_word.row, i);
            }
            yed_buff_insert_string(frame->buffer, *((char **)array_item(converted_items, popup.selection)), converted_word.row, word_start);
        }else {
            word_len   = converted_word.word_len;
            word_start = converted_word.word_start;
            for(int i=word_start+word_len-1; i>word_start-1; i--) {
                yed_delete_from_line(frame->buffer, converted_word.row, i);
            }
            yed_buff_insert_string(frame->buffer, *((char **)array_item(converted_items, popup.selection)), converted_word.row, word_start);
        }
    }else if(event->key == ARROW_UP) {
        event->cancel = 1;
        if(popup.selection > 0) {
            popup.selection -= 1;
        }else{
            popup.selection = popup.size-1;
        }
        draw_popup();
    }else if(event->key == ARROW_DOWN) {
        event->cancel = 1;
        if(popup.selection < popup.size-1) {
            popup.selection += 1;
        }else {
            popup.selection = 0;
        }
        draw_popup();
    }
}

// Assumes little endian
char *print_bits() {
    char ret[67];
    int first = 0;
    int j = 0;

    if(converted_word.negative == 1) {
        ret[j] = '-';
        j++;
    }
    ret[j] = '0';
    j++;
    ret[j] = 'b';
    j++;

    if(converted_word.data_type == signed_32) {
        for(int i=31; i>=0; i--) {
            if(((converted_word.num_int & ((int64_t)1<<i))>>i) == 1) {
                ret[j] = '1';
                first = 1;
                j++;
            }else if(first){
                ret[j] = '0';
                j++;
            }
        }
    }else if(converted_word.data_type == unsigned_32) {
        for(int i=31; i>=0; i--) {
            if(((converted_word.num_uint & ((uint32_t)1<<i))>>i) == 1) {
                ret[j] = '1';
                first = 1;
                j++;
            }else if(first){
                ret[j] = '0';
                j++;
            }
        }
    }else if(converted_word.data_type == signed_64) {
        for(int i=63; i>=0; i--) {
            if(((converted_word.num_ll & ((uint64_t)1<<i))>>i) == 1) {
                ret[j] = '1';
                first = 1;
                j++;
            }else if(first){
                ret[j] = '0';
                j++;
            }
        }
    }else if(converted_word.data_type == unsigned_64) {
        for(int i=63; i>=0; i--) {
            if(((converted_word.num_ull & ((uint64_t)1<<i))>>i) == 1) {
                ret[j] = '1';
                first = 1;
                j++;
            }else if(first){
                ret[j] = '0';
                j++;
            }
        }
    }

    ret[j] = '\0';
    return strdup(ret);
}

// Assumes little endian
char *print_twos_bits() {
    char ret[67];
    int  first   = 0;
    int  j       = 0;
    int  add_one = 1;
    int  changed = 1;
    int  carry   = 0;

    if(converted_word.negative == 1) {
        ret[j] = '-';
        j++;
    }
    ret[j] = '0';
    j++;
    ret[j] = 'b';
    j++;
    if(converted_word.data_type == signed_32) {
        for(int i=31; i>=0; i--) {
            if(((converted_word.num_int & ((int64_t)1<<i))>>i) == 1 && first) {
                ret[j] = '0';
                j++;
            }else {
                ret[j] = '1';
                first = 1;
                j++;
            }
        }

        for(int i=j; i>2; i--) {
/*             yed_cerr("num:%d char:%c  ", i, ret[i]); */
            if(changed == 1) {
                if(ret[i] == '0') {
                    ret[i] = '1';
                    changed = 0;
                }else if(ret[i] == '1'){
                    ret[i] = '0';
                    changed = 1;
                    if(i == 3) {
                        carry = 1;
                    }
                }
            }
/*             yed_cerr("num:%d char:%c\n", i, ret[i]); */
        }
/*         if(carry == 1) { */
/*             ret[j] == '1'; */
/*             j++; */
/*         } */
        ret[j] = '\0';

    }else if(converted_word.data_type == unsigned_32) {
        for(int i=31; i>=0; i--) {
            if(((converted_word.num_uint & ((uint32_t)1<<i))>>i) == 1) {
                ret[j] = '0';
                first = 1;
                j++;
            }else if(first){
                ret[j] = '1';
                j++;
            }
        }
        ret[j] = '\0';
    }else if(converted_word.data_type == signed_64) {
        for(int i=63; i>=0; i--) {
            if(((converted_word.num_ll & ((uint64_t)1<<i))>>i) == 1) {
                ret[j] = '0';
                first = 1;
                j++;
            }else if(first){
                ret[j] = '1';
                j++;
            }
        }
        ret[j] = '\0';
    }else if(converted_word.data_type == unsigned_64) {
        for(int i=63; i>=0; i--) {
            if(((converted_word.num_ull & ((uint64_t)1<<i))>>i) == 1) {
                ret[j] = '0';
                first = 1;
                j++;
            }else if(first){
                ret[j] = '1';
                j++;
            }
        }
        ret[j] = '\0';
    }

    return strdup(ret);
}

/* check_int: checks if cstring is an int
 * params: char *number to be checked
 * returns: int 0 if not hex
 */
int check_int(char *number) {
    int i = 0;
    int at_least_one = 0;

    while(number[i] != '\0') {
        if(isdigit(number[i]) == 0) return 0;
        at_least_one = 1;
        i++;
    }
    if(!at_least_one) return 0;
    return 1;
}

/* check_hex: checks if cstring is hex
 * params: char *number to be checked
 * returns: int -1 if not hex
 */
int check_hex(char *number) {
    int i = 2;
    int at_least_one = 0;

    if(number[0] != '0') return 0;
    if(number[1] != 'x' && number[1] != 'X') return 0;

    while(number[i] != '\0') {
        if(isxdigit(number[i]) == 0) return 0;
        at_least_one = 1;
        i++;
    }
    if(!at_least_one) return 0;
    return 1;
}

/* check_bin: checks if cstring is a binary number
 * params: char *number to be checked
 * returns: int 0 if not binary
 */
int check_bin(char *number) {
    int i = 2;
    int at_least_one = 0;

    if(number[0] != '0') return 0;
    if(number[1] != 'b') return 0;

    while(number[i] != '\0') {
        if(isbdigit(number[i]) == 0) return 0;
        at_least_one = 1;
        i++;
    }
    if(!at_least_one) return 0;
    return 1;
}

/* check_oct: checks if cstring is an octal number
 * params: char *number to be checked
 * returns: int 0 if not octal
 */
int check_oct(char *number) {
    int i = 1;
    int at_least_one = 0;

    if(number[0] != '0') return 0;

    while(number[i] != '\0') {
        if(isodigit(number[i]) == 0) return 0;
        at_least_one = 1;
        i++;
    }
    if(!at_least_one) return 0;
    return 1;
}
int isbdigit(char c) {
    int ret = 0;
    switch(c) {
        case '0':
            ret = 1;
            break;
        case '1':
            ret = 1;
            break;
        default :
            break;
    }
    return ret;
}

int isodigit(char c) {
    int ret = 0;
    switch(c) {
        case '0':
            ret = 1;
            break;
        case '1':
            ret = 1;
            break;
        case '2':
            ret = 1;
            break;
        case '3':
            ret = 1;
            break;
        case '4':
            ret = 1;
            break;
        case '5':
            ret = 1;
            break;
        case '6':
            ret = 1;
            break;
        case '7':
            ret = 1;
            break;
        default :
            break;
    }
    return ret;
}

void bin_to_int(char *bin) {
    uint64_t i = 0;
    uint64_t p;
    bin = bin+2;

    if(converted_word.data_type == signed_32) {
        converted_word.num_int = 0;
        while(bin[i] != '\0') {
            if(bin[i] == '1') {
                p = pow(2, strlen(bin)-i-1);
                converted_word.num_int += p;
            }
            i++;
        }
    }else if(converted_word.data_type == unsigned_32) {
        converted_word.num_uint = 0;
        while(bin[i] != '\0') {
            if(bin[i] == '1') {
                p = pow(2, strlen(bin)-i-1);
                converted_word.num_uint += p;
            }
            i++;
        }
    }else if(converted_word.data_type == signed_64) {
        converted_word.num_ll = 0;
        while(bin[i] != '\0') {
            if(bin[i] == '1') {
                p = pow(2, strlen(bin)-i-1);
                converted_word.num_ll += p;
            }
            i++;
        }
    }else if(converted_word.data_type == unsigned_64) {
        converted_word.num_ull = 0;
        while(bin[i] != '\0') {
            if(bin[i] == '1') {
                p = pow(2, strlen(bin)-i-1);
                converted_word.num_ull += p;
            }
            i++;
        }
    }
}

static void kill_popup(void) {
    yed_direct_draw_t **dd;

    if (!popup.is_up) { return; }

    free_string_array(popup.strings);

    array_traverse(popup.dds, dd) {
        yed_kill_direct_draw(*dd);
    }

    array_free(popup.dds);

    popup.frame = NULL;

    popup.is_up = 0;
}

static void draw_popup(void) {
    yed_direct_draw_t **dd_it;
    yed_attrs           active;
    yed_attrs           assoc;
    yed_attrs           merged;
    yed_attrs           merged_inv;
    char              **it;
    int                 max_width;
    int                 has_left_space;
    int                 i;
    char                buff[512];
    yed_direct_draw_t  *dd;

    array_traverse(popup.dds, dd_it) {
        yed_kill_direct_draw(*dd_it);
    }
    array_free(popup.dds);

    popup.dds = array_make(yed_direct_draw_t*);

    active = yed_active_style_get_active();
    assoc  = yed_active_style_get_associate();
    merged = active;
    yed_combine_attrs(&merged, &assoc);
    merged_inv = merged;
    merged_inv.flags ^= ATTR_INVERSE;

    max_width = 0;
    array_traverse(popup.strings, it) {
        max_width = MAX(max_width, strlen(*it));
    }

    i              = 1;
    has_left_space = popup.frame->cur_x > popup.frame->left;

    array_traverse(popup.strings, it) {
        snprintf(buff, sizeof(buff), "%s%*s ", has_left_space ? " " : "", -max_width, *it);
        dd = yed_direct_draw(popup.row + i,
                             popup.frame->left + popup.cursor_col - 1 - has_left_space,
                             i == popup.selection + 1 ? merged_inv : merged,
                             buff);
        array_push(popup.dds, dd);
        i += 1;
    }
}

static void start_popup(yed_frame *frame, int start_len, array_t strings) {
    kill_popup();

    popup.frame     = frame;
    popup.strings   = copy_string_array(strings);
    popup.dds       = array_make(yed_direct_draw_t*);
    popup.start_len = start_len;
    popup.selection = 0;

    draw_popup();

    popup.is_up = 1;
}
