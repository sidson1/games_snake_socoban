#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <termios.h>
#include <time.h>
#include <assert.h>
#include <signal.h>


#define WIDTH 80
#define HEIGHT 25

typedef enum {
	UP, DOWN, LEFT, RIGHT
} direction;


typedef struct DirectedPoint {
	int row, col;
	direction dir;
} DirectedPoint;


typedef struct Snake {
	DirectedPoint head; 
	DirectedPoint tail;
	direction new_dir;
	unsigned long long length; 
	long long lengthbuf; 
} Snake;


enum { width = WIDTH - 2,
       height = HEIGHT - 3 };

enum { KEY_ARROWU = 0x415b1b,
       KEY_ARROWD = 0x425b1b,
       KEY_ARROWR = 0x435b1b,
       KEY_ARROWL = 0x445b1b };

enum { BILLION = 1000000000 }; 
enum { FOOD_RARITY = 512, 
       LVLUP_LENGTH = 50 }; 

sig_atomic_t run = 1; 

struct termios saved_attributes;

unsigned char playfield[height][width];
int food_cnt = 0;
int level = 1;

void reset_input_mode(void){
	printf("\x1b[?25h");
	tcsetattr(STDIN_FILENO, TCSANOW, &saved_attributes);
	system("clear");
}


void sighandler(int sig){
	run = 0;
	signal(sig, sighandler);
}


void set_terminal_mode(void){
	struct termios tattr;

	setbuf(stdout, NULL);
	
	if (!isatty(STDIN_FILENO)){
		fprintf(stderr, "Not a terminal.\n");
		exit(EXIT_FAILURE);
	}

	
	tcgetattr(STDIN_FILENO, &tattr);
	tattr.c_lflag &= ~(ICANON|ECHO); 
	tattr.c_cc[VMIN] = 0;
	tattr.c_cc[VTIME] = 0;
	tcsetattr(STDIN_FILENO, TCSAFLUSH, &tattr);

	printf("\x1b[?25l");
	system("clear");
	atexit(reset_input_mode);
}


int turn_up(Snake *snake){
	DirectedPoint *head = &(snake->head);
	char bodyseg;
	int success = 1;

	switch(head->dir){
	case LEFT:
		bodyseg = '\\';
		break;
	case RIGHT:
		bodyseg = '/';
		break;
	case DOWN:
		success = 0; 
	default:
		goto ret;
	}

	snake->new_dir = UP;
	playfield[head->row][head->col] = bodyseg;

  ret:
	return success;
}

int turn_down(Snake *snake){
	DirectedPoint *head = &(snake->head);
	char bodyseg;
	int success = 1;

	switch(head->dir){
	case LEFT:
		bodyseg = '/';
		break;
	case RIGHT:
		bodyseg = '\\';
		break;
	case UP:
		success = 0; 
	default:
		goto ret;
	}

	snake->new_dir = DOWN;
	playfield[head->row][head->col] = bodyseg;

  ret:
	return success;
}

int turn_left(Snake *snake){
	DirectedPoint *head = &(snake->head);
	char bodyseg;
	int success = 1;

	switch(head->dir){
	case UP:
		bodyseg = '\\';
		break;
	case DOWN:
		bodyseg = '/';
		break;
	case RIGHT:
		success = 0; 
	default:
		goto ret;
	}

	snake->new_dir = LEFT;
	playfield[head->row][head->col] = bodyseg;

  ret:
	return success;
}

int turn_right(Snake *snake){
	DirectedPoint *head = &(snake->head);
	char bodyseg;
	int success = 1;

	switch(head->dir){
	case UP:
		bodyseg = '/';
		break;
	case DOWN:
		bodyseg = '\\';
		break;
	case LEFT:
		success = 0; 
	default:
		goto ret;
	}

	snake->new_dir = RIGHT;
	playfield[head->row][head->col] = bodyseg;

  ret:
	return success;
}


int process_key(Snake *snake){
	int c = 0;
	int snake_moved = 0;

	if(read(STDIN_FILENO, &c, 3)){
		switch(c){
		case KEY_ARROWU:
			snake_moved = turn_up(snake);
			break;
		case KEY_ARROWD:
			snake_moved = turn_down(snake);
			break;
		case KEY_ARROWL:
			snake_moved = turn_left(snake);
			break;
		case KEY_ARROWR:
			snake_moved = turn_right(snake);
			break;
		}
	}

	
	return snake_moved;
}


void init_playfield(void){
	for (int j = 0; j < height; j++){
		for(int i = 0; i < width; playfield[j][i++] = ' ');
	}
}


void init_snake(Snake *snake){
	playfield[height / 2][width / 2] = '<';

	snake->head.row = height / 2;
	snake->head.col = width / 2;
	snake->head.dir = RIGHT;

	snake->tail.row = height / 2;
	snake->tail.col = width / 2;
	snake->tail.dir = RIGHT;

	snake->new_dir = RIGHT;

	snake->length = 1;
	snake->lengthbuf = 4; 
}


void redraw_all(Snake *snake){
	printf("\x1b[H"); 

	for(int i = width + 2; i; i--){ 
		putchar('#');
	}
	putchar('\n');

	for(int j = 0; j < height; j++){ 
		putchar('#'); 
		for(int i = 0; i < width; i++){
			putchar(playfield[j][i]);
		}
		putchar('#'); 
		putchar('\n');
	}

	for(int i = width + 2; i; i--){ 
		putchar('#');
	}
	putchar('\n');

	printf("Length: %5lld\tLevel: %3d", snake->length, level);
}


void redraw_animation(Snake *snake){
	
	printf("\x1b[%d;%dH", snake->head.row + 2, snake->head.col + 2);

	if(snake->head.dir == UP || 
	   snake->head.dir == DOWN){
		putchar('|');
	} else {
		putchar('-');
	}

	
	printf("\x1b[%d;%dH", snake->tail.row + 2, snake->tail.col + 2);

	if(playfield[snake->tail.row][snake->tail.col] != '-' &&
	   playfield[snake->tail.row][snake->tail.col] != '|'){
		if(snake->tail.dir == UP ||
		   snake->tail.dir == DOWN){
			putchar('|');
		} else {
			putchar('-');
		}
	}
}


void move_snake(Snake *snake){
	DirectedPoint *head = &(snake->head);
	DirectedPoint *tail = &(snake->tail);
	unsigned char headchar;

	
	if(head->dir == snake->new_dir){
		if(head->dir == UP || head->dir == DOWN){
			playfield[head->row][head->col] = '|';
		} else {
			playfield[head->row][head->col] = '-';
		}
	} else{
		head->dir = snake->new_dir;
	}

	
	switch(head->dir){
	case UP:
		head->row--;
		headchar = 'V';
		break;
	case DOWN:
		head->row++;
		headchar = '^';
		break;
	case LEFT:
		head->col--;
		headchar = '>';
		break;
	case RIGHT:
		head->col++;
		headchar = '<';
		break;
	default:
		assert(0); 
	}

	
	if(head->col < 0 ||
	   head->col >= width ||
	   head->row < 0 ||
	   head->row >= height){
		reset_input_mode();
		system("clear");
		printf("You hit a wall. Game over!\n");
		printf("Press any key to exit\n");
		getchar();
		exit(0);
	}

	
	if(playfield[head->row][head->col] == '@'){
		snake->lengthbuf++;
		food_cnt--;
	}
	
	else if(playfield[head->row][head->col] != ' '){
		reset_input_mode();
		system("clear");
		printf("You bit yourself. Game over!\n");
		printf("Press any key to exit\n");
		getchar();
		exit(0);
	}

	
	playfield[head->row][head->col] = headchar;

	
	if(snake->lengthbuf){
		snake->lengthbuf--;
		snake->length++;
	} else {
		playfield[tail->row][tail->col] = ' ';

		switch(tail->dir){
		case UP:
			tail->row--;
			break;
		case DOWN:
			tail->row++;
			break;
		case LEFT:
			tail->col--;
			break;
		case RIGHT:
			tail->col++;
			break;
		default:
			assert(0); 
		}

		
		switch(playfield[tail->row][tail->col]){
		case '-': 
		case '|': 
			break;
		case '/': 
			switch(tail->dir){ 
			case UP:
				tail->dir = RIGHT;
				break;
			case DOWN:
				tail->dir = LEFT;
				break;
			case LEFT:
				tail->dir = DOWN;
				break;
			case RIGHT:
				tail->dir = UP;
				break;
			default:
				assert(0); 
			}
			break;
		case '\\': 
			switch(tail->dir){ 
			case UP:
				tail->dir = LEFT;
				break;
			case DOWN:
				tail->dir = RIGHT;
				break;
			case LEFT:
				tail->dir = UP;
				break;
			case RIGHT:
				tail->dir = DOWN;
				break;
			default:
				assert(0); 
			}
			break;
		default:
			assert(0); 
		}
	}
}


void gen_food(){
	
	static const int max_food = (width * height) / FOOD_RARITY + 1;
	int row, col;

	if(food_cnt <= max_food){
		row = rand() % height;
		col = rand() % width;
		if(playfield[row][col] == ' '){ 
			playfield[row][col] = '@'; 
			food_cnt++;
		} else {
			gen_food(); 
		}
	}
}

void level_up(Snake *snake){
	static int upped_already = 0;

	if(snake->length % LVLUP_LENGTH == 0){
		if(!upped_already){
			level++;
			upped_already = 1;
		}
	} else {
		upped_already = 0;
	}
}

int main(void){
	set_terminal_mode();
	signal(SIGINT, sighandler);
	srand(time(NULL));

	
	struct timespec time_start, time_now;
	long long time_delta;

	char animation_frame = 1;

	Snake snake0;
	int snake_moved = 0;

	init_playfield();
	init_snake(&snake0);

	redraw_all(&snake0);

	clock_gettime(CLOCK_REALTIME, &time_start);

	while(run){
		snake_moved = process_key(&snake0);

		
		clock_gettime(CLOCK_REALTIME, &time_now);
		time_delta = ((time_now.tv_sec - time_start.tv_sec) * BILLION +
		              (time_now.tv_nsec - time_start.tv_nsec));
		if(snake_moved ||
		   time_delta > BILLION / (level + 1) / 5){
			time_start = time_now;
			if(animation_frame){
				redraw_animation(&snake0);
				animation_frame = 0;
			} else {
				snake_moved = 0;
				gen_food();
				level_up(&snake0);
				move_snake(&snake0);
				redraw_all(&snake0);
				animation_frame = 1;
			}
		}
	}

	return EXIT_SUCCESS;
}
