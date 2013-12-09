
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <ncurses.h>
#include <time.h>
#include <string.h>
#include <fcntl.h>
#include <glib.h>
#include <glib/gstdio.h>
#include <glib-object.h>
#include <glib-unix.h>
#include <json-glib/json-glib.h>

//
// global variables
//
int rows,columns; // window size

static const gchar *default_config = "\
{\
	\"timezones\": [\
		\"\",\
                \"America/New_York\",\
                \"GMT\",\
                \"Europe/Paris\",\
                \"Asia/Baghdad\",\
                \"Asia/Singapore\",\
                \"Asia/Tokyo\"\
	],\
	\"font\": \"Lat2-VGA8.psf.gz\",\
	\"fontpath\": [\
		\"/usr/share/consolefonts\",\
		\"/home/chicks/consolefonts\"\
	]\
}\
";

struct Font {
	int glyphs, height, width, char_size, bytes_per_row, font_start;
	char data[128*1024]; // the biggest I've seen in the wild is <30k
	unsigned int charmap[512];
};

struct Font myfont;


/********************************
 *        functions
 */

int file_exists (char * fileName) {
	struct stat buf;
	int i = stat ( fileName, &buf );
	/* File found */
	if ( i == 0 ) {
		return 1;
	}
	return 0;
}

const char *byte_to_binary(int x) {
    static char b[9];
    b[0] = '\0';

    for (int z = 128; z > 0; z >>= 1) {
        strcat(b, ((x & z) == z) ? "X" : ".");
    }

    return b;
}

int read_font (const char * filename) {
	char buffer[10];
	gint pipefds[2];
	int compressed = 0;
	size_t got;

	FILE * fh = g_fopen(filename,"r");

	fread(&buffer,2,1,fh);

//	printf("%x %x\n",buffer[0],buffer[1] & 0xff);
	if ((buffer[0] == 0x1f) && ((buffer[1] & 0xff) == 0x8b)) {
		char *base_cmd = "/bin/gzip -dc ";
		compressed++;
//		printf("font %s is compressed\n",filename);
		fclose(fh);

		gchar * cmd = g_strconcat(base_cmd,filename,NULL);
		fh = popen(cmd,"r");
		if (fh == NULL) {
			printf("ERROR: gzip pipe open failed: %s",strerror(errno));
			exit(1);
		}
	} else {
		printf("font %s is NOT compressed\n",filename);
		// TODO: rewind fh
		exit(3);
	}

	got = fread(&myfont.data,1,sizeof(myfont.data),fh);

	printf("font %s is %i bytes uncompressed\n",filename,(int) got);

	if (got == sizeof(myfont.data)) {
		printf("ERROR: font %s is bigger than our buffer can handle, exiting.\n",filename);
		exit(2);
	}

	// determine PSF version
	if (
		((myfont.data[0] & 0xff) == 0x36)
		&& ((myfont.data[1] & 0xff) == 0x04)
	) {
		// v1
		myfont.width = 8;
		myfont.bytes_per_row = 1;
		myfont.font_start=4;
		myfont.height = myfont.char_size = (myfont.data[3] & 0x99);
		int mode = (myfont.data[2] & 0x99);
		if ( mode & 0b001 ) {
			myfont.glyphs=512;
		} else {
			myfont.glyphs=256;
		}
		printf("FONT[0-4]: x%02x x%02x x%02x x%02x\n",myfont.data[0] & 0xff, myfont.data[1] & 0xff, myfont.data[2] & 0xff, myfont.data[3] & 0xff );
		printf("FONT: width=%i height=%i bytes_per_row=%i glyphs=%i\n", myfont.width, myfont.height, myfont.bytes_per_row, myfont.glyphs );
	} else if (
		((myfont.data[0] & 0xff) == 0x72)
		&& ((myfont.data[1] & 0xff) == 0x04)
		&& ((myfont.data[2] & 0xff) == 0x4a)
		&& ((myfont.data[3] & 0xff) == 0x86)
	) {
		// v2
		printf("ERROR: font %s is a v2 PSF which is UNIMPLEMENTED, exiting.\n",filename);
		// TODO: implement!!!
		exit(4);
	} else {
		// wtf?
		printf("FONT[0-4]: x%02x x%02x x%02x x%02x\n",myfont.data[0] & 0xff, myfont.data[1] & 0xff, myfont.data[2] & 0xff, myfont.data[3] & 0xff );
		printf("ERROR: font %s is a not a recognized PSF version, exiting.\n",filename);
		exit(4);
	}

	unsigned int font_pos = myfont.font_start;
	for (int c = 0; c < myfont.glyphs; c++) {
#if 0
		printf("c=%i <<%c>> starts at %i:\n", c, c, font_pos );
		for(int l = 0; l < myfont.height; l++) {
			unsigned char segment = myfont.data[font_pos+l] & 0xff;
			printf("\tl=%i seg %s\n", l, byte_to_binary(segment) );
		}
#endif
		myfont.charmap[c] = font_pos;
		font_pos += myfont.height;
	}

	// clean up fh
	if (compressed) {
		pclose(fh);
	} else {
		fclose(fh);
	}

	return 0;
}

void initializations() {
	JsonParser *parser;
	JsonNode *root;
	JsonObject *jsonObj;
	GError *error;
	JsonArray *fontpath;

	// parse JSON
#if !defined(GLIB_VERSION_2_36) 
	g_type_init(); 
#endif 
	parser = json_parser_new ();

	error = NULL;
	json_parser_load_from_data (parser, default_config, strlen(default_config), &error);
	if (error) {
		endwin();
		g_print ("Unable to parse config: %s\n", error->message);
		g_print("%s\n",default_config);
		g_error_free (error);
		g_object_unref (parser);
		exit(EXIT_FAILURE);
	}

	root = json_parser_get_root (parser);
	jsonObj = json_node_get_object(root);

	// get font
	const gchar *font = json_object_get_string_member(jsonObj,"font");
//	printf("font=%s\n",font);

	// get fontpath
	fontpath = json_object_get_array_member(jsonObj,"fontpath");

	// does font exist?
	for (guint index = 0; index < json_array_get_length(fontpath); index++) {
		const gchar *thispath;
		thispath = json_array_get_string_element(fontpath, index);
//		printf("thispath=%s\n",thispath);
		gchar * check_file = g_strconcat(thispath,"/",font,NULL);
		if ( file_exists(check_file) ) {
			font = check_file;
			printf("found font=%s\n",font);
		}
	}

	// read font in
	read_font(font); // exits on its own errors

	// handy for debugging
	refresh();	// Print it on to the real screen
//	sleep(3);

	// clean up
	g_object_unref (parser);

//	cbreak(); // cbreak so they don't have to hit enter to exit
	getmaxyx(stdscr, rows, columns); // get current window size
	nodelay(stdscr, TRUE); // read characters in non-blocking mode
}

int kbhit () {
	int ch = getch();

	if (ch != ERR) {
		ungetch(ch);
		return(1);
	} else {
		return(0);
	}
}

unsigned int get_segment (unsigned char c, int line) {
	return myfont.data[ myfont.charmap[c] + line ];
}

int big_display (int x, int y, char *string_small) {
	mvprintw(x,y,"%s",string_small);
	x++;

	init_pair(1,COLOR_BLACK,COLOR_BLUE);

	int maxi = columns / (myfont.width + 1);

	for (int line=0; line < myfont.height; line++) {
		for (int i=0; i < strlen(string_small) && i<maxi; i++) {
			unsigned int raw_segment = get_segment(string_small[i],line);
				
			static char b[99];
			b[0] = '\0';

			for (int z = 128; z > 0; z >>= 1) {
				strcat(b, ((raw_segment & z) == z) ? "X" : " ");
			}

			int thisy = y + i * (myfont.width + 1);
			attron(COLOR_PAIR(1));
			mvprintw(x,thisy,"%s",b);
			attroff(COLOR_PAIR(1));
		}
		x++;
	}
}

void display_clock() {
	int centerx,centery;
	char* time_string;
	gchar *gtime_string;
	time_t now;
	size_t width;
	while (1) {
		clear();
		centerx = rows/2;
		centery = columns/2;

		now=time(NULL);
		time_string=ctime(&now);

		GDateTime * date = g_date_time_new_now_local();
		//g_date_strftime((gchar *)&gtime_string,sizeof(gtime_string),"%X %x",date);
		gtime_string = g_date_time_format(date, "%H:%M");
		const gchar *tz = g_date_time_get_timezone_abbreviation(date);
		

		gchar *foo = g_strdup_printf("%s",(char *)&gtime_string);

		width = strlen(gtime_string);
		mvprintw(centerx,centery-(width/2),"%s",gtime_string);
		big_display(centerx-12,3,(char *)gtime_string);
		refresh();	// Print it on to the real screen

/*		if (kbhit()) {
			return;
		}
*/

		getmaxyx(stdscr, rows, columns); // get current window size
		sleep(1);
	}
}

int main() {
	printf("Welcome to Curses Clock\n");	// Print welcome message

	initializations();
	initscr();	// Start curses mode

	// color or death!
	if (has_colors() == FALSE) {
		endwin();
		printf("no colors, no clock!");
		exit(64);
	}
	start_color();

	getmaxyx(stdscr, rows, columns); // get current window size
	printw("\ninitializations complete\n");
	printw("(rows=%i columns=%i)\n", rows, columns);
	refresh();	// Print it on to the real screen
	sleep(1);
	
	display_clock(); // show the clock

	endwin();	// End curses mode

	printf("exiting because the user hit a key\n");

	return 0;
}
