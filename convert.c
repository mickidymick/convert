#include <yed/plugin.h>

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
    octal
} n_type;

static enum {
    unsigned_64,
    signed_64,
    unsigned_32,
    signed_32
} d_type;

typedef struct {
    char               word[512];   //the number as a string
    int                word_start;  //col the number starts on
    int                word_len;    //the length of the word
    int                row;         //row the number is on
    int                negative;    //0 not negative 1 negative
    int                number_type; //n_type
    int                data_type;   //d_type
    unsigned long long num_ull;     //stored number u64
    long long          num_ll;      //stored number s64
    unsigned int       num_uint;    //stored number u32
    int                num_int;     //stored number s32

} convert_word;

static char             *num_type_arr[5] = {"integer", "hexadecimal", "binary", "octal", "error"};
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
static int         bin_to_int(char *bin);
static void        draw_popup(void);
static void        start_popup(yed_frame *frame, int start_len, array_t strings);
static void        kill_popup(void);
static char       *printBits(int num);
static yed_buffer *get_or_make_buff(void);
static void        convert_word_at_point(yed_frame *frame, int row, int col);
static int         convert_find_size(void);
static void        init_convert(void);
static void        print_converted_struct(void);
static char       *snprintf_int(char *tmp_buffer);

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

//21345
int convert_find_size(void) {
    LOG_FN_ENTER();
    switch(converted_word.number_type) {
        case integer:
            if(converted_word.word_len < 10) {
                //s32
                converted_word.num_int = strtol(converted_word.word, NULL, 10);
                converted_word.data_type = signed_32;
                print_converted_struct();
                return 1;
            }else if(converted_word.word_len == 10) {
                if((strncmp(converted_word.word, "2147483648", 10) <= 0 && converted_word.negative == 1)
                || (strncmp(converted_word.word, "2147483647", 10) <= 0 && converted_word.negative == 0)) {
                    //s32
                    converted_word.num_int = strtol(converted_word.word, NULL, 10);
                    converted_word.data_type = signed_32;
                    print_converted_struct();
                    return 1;
                }else if(strncmp(converted_word.word, "4294967295", 10) <= 0 && converted_word.negative == 0) {
                    //u32
                    converted_word.num_uint = strtoul(converted_word.word, NULL, 10);
                    converted_word.data_type = unsigned_32;
                    print_converted_struct();
                    return 1;
                }else{
                    //s64
                    yed_cerr("%s", converted_word.word);
                    converted_word.num_ll = strtoll(converted_word.word, NULL, 10);
                    converted_word.data_type = signed_64;
                    print_converted_struct();
                    return 1;
                }
            }else if(converted_word.word_len < 19) {
                //s64
                converted_word.num_ll = strtoll(converted_word.word, NULL, 19);
                converted_word.data_type = signed_64;
                print_converted_struct();
                return 1;
            }else if(converted_word.word_len == 19) {
                if((strncmp(converted_word.word, "9223372036854775808", 19) <= 0 && converted_word.negative == 1)
                || (strncmp(converted_word.word, "9223372036854775807", 19) <= 0 && converted_word.negative == 0)) {
                    //s64
                    converted_word.num_ll = strtoll(converted_word.word, NULL, 19);
                    converted_word.data_type = signed_64;
                    print_converted_struct();
                    return 1;
                }else{
                    //u64
                    converted_word.num_ull = strtoull(converted_word.word, NULL, 19);
                    converted_word.data_type = unsigned_64;
                    print_converted_struct();
                    return 1;
                }
            }else if(converted_word.word_len == 20) {
                if(strncmp(converted_word.word, "18446744073709551615", 20) <= 0 && converted_word.negative == 0) {
                    //u64
                    converted_word.num_ull = strtoull(converted_word.word, NULL, 20);
                    converted_word.data_type = unsigned_64;
                    print_converted_struct();
                    return 1;
                }else{
                    goto overflow;
                }
            }else{
                goto overflow;
            }
            break;

        case hexadecimal:
            break;

        case binary:
            break;

        case octal:
            break;

        default:
            goto no_num;
            break;
    }

    //Overflow
    if(0) {
overflow:;
        yed_cerr("Number to convert is too large.");
        return 0;
    }

    //Not a number
    if(0) {
no_num:;
        yed_cerr("String can not be converted into a number.");
        return 0;
    }

/*     number = strtol(converted_word.word, NULL, 10); */
/*     number = strtol(converted_word.word, NULL, 16); */
/*     number = bin_to_int(converted_word.word); */
/*     number = strtol(converted_word.word, NULL, 8); */

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
    int   number;
    int   number_type;

    yed_frame *frame;

    frame = ys->active_frame;

    if (!frame
    ||  !frame->buffer) {
        return;
    }

    init_convert();
    convert_word_at_point(frame, frame->cursor_line, frame->cursor_col);

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

    if(convert_find_size() == 0) {
        return;
    }

    LOG_FN_ENTER();
    yed_cprint("data_type:%d len:%d\n", converted_word.data_type, converted_word.word_len);
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

    char buffer[512];
    char tmp_buffer[512];

/*  Integer */
    memcpy(tmp_buffer, snprintf_int(tmp_buffer), 512);
/*     snprintf(tmp_buffer, 512, "%d", number); */
    item = strdup(tmp_buffer); array_push(converted_items, item);
    snprintf(buffer, 512, "%11s: %20s", data_type_arr[converted_word.data_type], *((char **)array_item(converted_items, 0)));
    item = strdup(buffer); array_push(popup_items, item);

/*  Hexadecimal     */
    snprintf(tmp_buffer, 512, "0x%x", number);
    item = strdup(tmp_buffer); array_push(converted_items, item);
    snprintf(buffer, 512, "%11s: %20s", num_type_arr[1], *((char **)array_item(converted_items, 1)));
    item = strdup(buffer); array_push(popup_items, item);

/*  Binary */
    item = printBits(number); array_push(converted_items, item);
    snprintf(buffer, 512, "%11s: %20s", num_type_arr[2], *((char **)array_item(converted_items, 2)));
    item = strdup(buffer); array_push(popup_items, item);

/*  Octal */
    snprintf(tmp_buffer, 512, "0%o", number);
    item = strdup(tmp_buffer); array_push(converted_items, item);
    snprintf(buffer, 512, "%11s: %20s", num_type_arr[3], *((char **)array_item(converted_items, 3)));
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
    yed_cprint("\nword:%s\n", converted_word.word);
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
    if(converted_word.data_type == signed_32) {
        snprintf(tmp_buffer, 512, "%d", converted_word.num_int);
    }else if(converted_word.data_type == unsigned_32) {
        snprintf(tmp_buffer, 512, "%u", converted_word.num_uint);
    }else if(converted_word.data_type == signed_64) {
        snprintf(tmp_buffer, 512, "%lld", converted_word.num_ll);
    }else if(converted_word.data_type == unsigned_64) {
        snprintf(tmp_buffer, 512, "%llu", converted_word.num_ull);
    }
    return tmp_buffer;
}

void convert_word_at_point(yed_frame *frame, int row, int col) {
    yed_buffer *buff;
    yed_line   *line;
    int         word_len;
    char        c, *word_start, *ret;
    int         word_column;
    int         neg;

    if (frame == NULL || frame->buffer == NULL) { return; }

    buff = frame->buffer;

    line = yed_buff_get_line(buff, row);
    if (!line) { return; }

    if (col == line->visual_width + 1) { return; }

    c = ((yed_glyph*)yed_line_col_to_glyph(line, col))->c;

    if (isspace(c)) { return; }

    word_len = 0;
    neg = 0;

    if(isalnum(c)) {
        while (col > 1) {
            c = ((yed_glyph*)yed_line_col_to_glyph(line, col - 1))->c;

            if (!isalnum(c)) {
                break;
            }

            col -= 1;
        }
        word_start = array_item(line->chars, yed_line_col_to_idx(line, col));
        word_column = col;
        c = col;
        while (c > 1) {
            c = ((yed_glyph*)yed_line_col_to_glyph(line, col - 1))->c;
            if(c == '-') {
                neg = 1;
/*                 col = c; */
/*                 word_start = array_item(line->chars, yed_line_col_to_idx(line, col)); */
/*                 word_column = col; */
                break;

            }else{
                break;
            }
        }
        c          = ((yed_glyph*)yed_line_col_to_glyph(line, col))->c;
        while (col <= line->visual_width && (isalnum(c))) {
            word_len += -1;
            col      += 1;
            c         = ((yed_glyph*)yed_line_col_to_glyph(line, col))->c;
        }
    }

    memcpy(converted_word.word, word_start, word_len);
    converted_word.word[word_len] = 0;
    converted_word.word_start = word_column;
    converted_word.word_len = word_len;
    converted_word.row = row;
    converted_word.negative = neg;
}

void key_handler(yed_event *event) {
    yed_line *line;
    yed_frame *frame;

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

        LOG_FN_ENTER();
        yed_cprint("HERE:%s col:%d len:%d row:%d", converted_word.word, converted_word.word_start, converted_word.word_len, converted_word.row);
        yed_cprint("selected:%s", *((char **)array_item(converted_items, popup.selection)));
        LOG_EXIT();

        for(int i=converted_word.word_start+converted_word.word_len-1; i>converted_word.word_start-1; i--) {
            yed_delete_from_line(frame->buffer, converted_word.row, i);
        }
        yed_buff_insert_string(frame->buffer, *((char **)array_item(converted_items, popup.selection)), converted_word.row, converted_word.word_start);
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
char *printBits(int num) {
    char ret[34];
    int first = 0;
    int j = 0;

    ret[j] = '0';
    j++;
    ret[j] = 'b';
    j++;

    for(int i=31; i>=0; i--) {
        if(((num & (1<<i))>>i) == 1) {
            ret[j] = '1';
            first = 1;
            j++;
        }else if(first){
            ret[j] = '0';
            j++;
        }
    }
    ret[j] = '\0';
    return strdup(ret);
}

/* check_int: checks if cstring is an int
 * params: char *number to be checked
 * returns: int 0 if not hex
 */
int check_int(char *number) {
    int i = 0;
    int at_least_one = 0;

/*     if(converted_word.negative) { */
/*         i+=1; */
/*     } */

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

int bin_to_int(char *bin) {
    int num = 0;
    int i = 0;

    bin = bin+2;

    while(bin[i] != '\0') {
        if(bin[i] == '1') {
            num += pow(2, strlen(bin)-i-1);
        }
        i++;
    }
    return num;
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