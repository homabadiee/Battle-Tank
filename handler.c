#include "main.h"
#include "stm32f3xx_it.h"
#include <stdbool.h>

typedef struct
{
	uint8_t i;
	uint8_t j;
} position;

typedef struct
{
    uint16_t frequency;
    uint16_t duration;
} Tone;

extern const Tone fire[3];
extern RTC_HandleTypeDef hrtc;
extern RTC_DateTypeDef myDate;
extern RTC_TimeTypeDef myTime;
extern UART_HandleTypeDef huart4;

extern volatile uint16_t volume;
extern bool sound;
extern char player1[50];
extern char player2[50];
extern bool _chance_box;
char start_game_time[8];
char end_game_time[8];

int game_positions[4][20];
int state = 0;
int main_option = 1;
bool _clear = false;
bool _option = false;
bool _start_game = false;
bool active_buzzer = false;

int chance_box_content;


GPIO_TypeDef *const Row_ports[] = {GPIOC, GPIOC, GPIOC, GPIOC};
const uint16_t Row_pins[] = {GPIO_PIN_0, GPIO_PIN_1, GPIO_PIN_2, GPIO_PIN_3};
GPIO_TypeDef *const Column_ports[] = {GPIOA, GPIOA, GPIOA, GPIOA};
const uint16_t Column_pins[] = {GPIO_PIN_0, GPIO_PIN_1, GPIO_PIN_2, GPIO_PIN_3};
volatile uint32_t last_gpio_exti;
enum game_components {TANK_UP = 0, TANK_DOWN = 1, TANK_RIGHT = 2, TANK_LEFT = 3, MISSILE = 4, OBSTACLE = 5, HEART = 6, CHANCE_BOX = 7, WALL = 255, BURST = 42};

uint8_t blinking_state;
int total_health = 3;
int total_missiles = 5;
int total_obstecles = 6;
int health_p1 = 3;
int health_p2 = 3;
int missile_p1 = 5;
int missile_p2 = 5;
position tank_p1;
position tank_p2;
position missile_p1_;
position missile_p2_;
position burst;
position chance_box_;

bool _missile_p1 = false;
bool _missile_p2 = false;
bool deleted_with_missile = false;
bool _burst = false;
bool change_dir_p1 = false;
bool change_dir_p2 = false;
bool move_p1 = false;
bool move_p2 = false;

int missile_p1_direction;
int missile_p2_direction;
int reward_p1 = 0;
int reward_p2 = 0;
int winner;
int missile_speed_p1 = 2;
int missile_speed_p2 = 2;
int coeff_missile_p1 = 1;
int coeff_missile_p2 = 1;

char winner_reward[8];




/************* SEVEN SEGMENT ************/
typedef struct
{
    GPIO_TypeDef *port;
    uint16_t pin;
} pin_type;

typedef struct
{
    pin_type digit_activators[4];
    pin_type BCD_input[4];
    uint32_t digits[4];
    uint32_t number;
} seven_segment_type;


seven_segment_type seven_segment =
{
		.digit_activators={{.port=GPIOD, .pin=GPIO_PIN_4},
						   {.port=GPIOD, .pin=GPIO_PIN_5},
						   {.port=GPIOD, .pin=GPIO_PIN_6},
						   {.port=GPIOD, .pin=GPIO_PIN_7}},
        .BCD_input={{.port=GPIOD, .pin=GPIO_PIN_0},
                    {.port=GPIOD, .pin=GPIO_PIN_1},
                    {.port=GPIOD, .pin=GPIO_PIN_2},
                    {.port=GPIOD, .pin=GPIO_PIN_3}},
        .digits={0, 0, 0, 0},
        .number = 0
};


void seven_segment_display_decimal(uint32_t n)
{
    if (n < 10)
    {
        HAL_GPIO_WritePin(seven_segment.BCD_input[0].port, seven_segment.BCD_input[0].pin,
                          (n & 1) ? GPIO_PIN_SET : GPIO_PIN_RESET);
        HAL_GPIO_WritePin(seven_segment.BCD_input[1].port, seven_segment.BCD_input[1].pin,
                          (n & 2) ? GPIO_PIN_SET : GPIO_PIN_RESET);
        HAL_GPIO_WritePin(seven_segment.BCD_input[2].port, seven_segment.BCD_input[2].pin,
                          (n & 4) ? GPIO_PIN_SET : GPIO_PIN_RESET);
        HAL_GPIO_WritePin(seven_segment.BCD_input[3].port, seven_segment.BCD_input[3].pin,
                          (n & 8) ? GPIO_PIN_SET : GPIO_PIN_RESET);
    }
}

void seven_segment_deactivate_digits(void)
{
    for (int i = 0; i < 4; ++i)
    {
        HAL_GPIO_WritePin(seven_segment.digit_activators[i].port, seven_segment.digit_activators[i].pin,
                          GPIO_PIN_SET);
    }
}

void seven_segment_activate_digit(uint32_t d)
{
    if (d < 4)
    {
        HAL_GPIO_WritePin(seven_segment.digit_activators[d].port, seven_segment.digit_activators[d].pin,
                          GPIO_PIN_RESET);
    }
}

void seven_segment_set_num(uint32_t n)
{
    if (n < 10000)
    {
        seven_segment.number = n;
        for (uint32_t i = 0; i < 4; ++i)
        {
            seven_segment.digits[3 - i] = n % 10;
            n /= 10;
        }
    }
}

void digit_blinking_seven_segment_refresh(void)
{

	static uint32_t state = 0;
	static uint32_t last_time = 0;
	static uint32_t blink_duration = 0;
	static bool blink = false;

	seven_segment_display_decimal(seven_segment.digits[state]);
	seven_segment_deactivate_digits();
	if (state == blinking_state)
	{
		HAL_GPIO_WritePin(
				seven_segment.digit_activators[blinking_state].port,
				seven_segment.digit_activators[blinking_state].pin, blink);
		if (HAL_GetTick() - blink_duration > 500)
		{

			blink = !blink;
			blink_duration = HAL_GetTick();
		}

		seven_segment_display_decimal(seven_segment.digits[state]);

	} else
	{
		seven_segment_activate_digit(state);
	}
	state = (state + 1) % 4;
	last_time = HAL_GetTick();

}

void two_digit_blinking_seven_segment_refresh(void)
{
	static uint32_t state = 0;
	static uint32_t last_time = 0;
	static uint32_t blink_duration = 0;
	static bool blink = false;

	seven_segment_display_decimal(seven_segment.digits[state]);
	seven_segment_deactivate_digits();
	if (state == 0 || state == 2)
	{
		if(state == 0)
		{
			HAL_GPIO_WritePin(
							seven_segment.digit_activators[0].port,
							seven_segment.digit_activators[0].pin, blink);
		}

		if(state == 2)
		{
			HAL_GPIO_WritePin(
							seven_segment.digit_activators[2].port,
							seven_segment.digit_activators[2].pin, blink);
		}

		if (HAL_GetTick() - blink_duration > 500)
		{

			blink = !blink;
			blink_duration = HAL_GetTick();
		}

		seven_segment_display_decimal(seven_segment.digits[state]);

	}
	else
	{
		seven_segment_activate_digit(state);
	}

	state = (state + 1) % 4;
	last_time = HAL_GetTick();

}

void seven_segment_refresh(void)
{
    static uint32_t state = 0;
    static uint32_t last_time = 0;
    if (HAL_GetTick() - last_time > 5)
    {
        seven_segment_deactivate_digits();
        seven_segment_activate_digit(state);
        seven_segment_display_decimal(seven_segment.digits[state]);
        state = (state + 1) % 4;
        last_time = HAL_GetTick();
    }
}

void programLoop()
{
	if(state == 2 && health_p1 <= 1 && health_p2 <= 1)
	{
		two_digit_blinking_seven_segment_refresh();
	}
	else if(state == 2 && health_p1 <= 1)
	{
		blinking_state = 0;
		digit_blinking_seven_segment_refresh();
	}
	else if(state == 2 && health_p2 <= 1)
	{
		blinking_state = 2;
		digit_blinking_seven_segment_refresh();
	}
	else
	{
	    seven_segment_refresh();
	}

}

/************ LCD ************/
typedef unsigned char byte;

byte tank_up[8] =
{
  0x00,
  0x0E,
  0x0E,
  0x1F,
  0x1F,
  0x1F,
  0x00,
  0x00
};

byte tank_down[8] =
{
  0x00,
  0x00,
  0x1F,
  0x1F,
  0x1F,
  0x0E,
  0x0E,
  0x00
};

byte tank_right[8] =
{
  0x00,
  0x00,
  0x1C,
  0x1F,
  0x1F,
  0x1F,
  0x1C,
  0x00
};

byte tank_left[8] =
{
  0x00,
  0x00,
  0x07,
  0x1F,
  0x1F,
  0x1F,
  0x07,
  0x00
};

byte missile[8] =
{
  0x00,
  0x00,
  0x00,
  0x03,
  0x07,
  0x0E,
  0x1C,
  0x08
};

byte obstacle[8] =
{
  0x1F,
  0x0E,
  0x0E,
  0x0E,
  0x0E,
  0x0E,
  0x0E,
  0x1F
};

byte wall[8] =
{
  0x1F,
  0x1F,
  0x1F,
  0x1F,
  0x1F,
  0x1F,
  0x1F,
  0x1F
};

byte heart[8] =
{
  0x00,
  0x0A,
  0x1F,
  0x1F,
  0x0E,
  0x04,
  0x00,
  0x00
};

byte chance_box[8] =
{
  0x1F,
  0x11,
  0x15,
  0x1D,
  0x1B,
  0x1B,
  0x1F,
  0x1B
};

void create_char()
{
	createChar(0, tank_up);
	createChar(1, tank_down);
	createChar(2, tank_right);
	createChar(3, tank_left);
	createChar(4, missile);
	createChar(5, obstacle);
	createChar(6, heart);
	createChar(7, chance_box);
}


void delete_board_chance_box()
{
	setCursor(chance_box_.j, chance_box_.i);
	print(" ");
}

void delete_chance_box()
{
	_chance_box = false;
	game_positions[chance_box_.i][chance_box_.j] = -1;

}

void handle_chance_box(int player)
{
	switch(chance_box_content)
	{
	case 0:

		if(player == 1)
		{
			if(missile_p1 == 8)
			{
				missile_p1 = 9;
			}
			else if(missile_p1 < 9)
			{
				missile_p1 += 2;
			}

		}
		else if(player == 2)
		{
			if(missile_p2 == 8)
			{
				missile_p2 = 9;
			}
			else if(missile_p2 < 9)
			{
				missile_p2 += 2;
			}

		}
		break;

	case 1:
		if(player == 1)
		{
			if(health_p1 == 8)
			{
				health_p1 = 9;
			}
			else if(health_p1 < 9)
			{
				health_p1 += 2;
			}

		}
		else if(player == 2)
		{
			if(health_p2 == 8)
			{
				health_p2 = 9;
			}
			else if(health_p2 < 9)
			{
				health_p2 += 2;
			}

		}
		break;

	case 2:
		if(player == 1)
		{
			coeff_missile_p1 = 3;

		}
		else if(player == 2)
		{
			coeff_missile_p2 = 3;

		}
		break;

	case 3:
		if(player == 1)
		{
			missile_speed_p1 = 2;

		}
		else if(player == 2)
		{
			missile_speed_p2 = 2;

		}

		break;
	}
}

bool add_chance_box_icon()
{
	int i = rand() % 4;
	int j = rand() % 20;
	if(game_positions[i][j] == -1)
	{
		chance_box_.i = i;
		chance_box_.j = j;
		chance_box_content = rand() % 4;
		game_positions[i][j] = CHANCE_BOX;
		setCursor(j, i);
		write(CHANCE_BOX);
		return true;
	}
	else
	{
		return false;
	}
}

bool add_health_icon()
{
	int i = rand() % 4;
	int j = rand() % 20;
	if(game_positions[i][j] == -1)
	{
		game_positions[i][j] = HEART;
		setCursor(j, i);
		write(HEART);
		return true;
	}
	else
	{
		return false;
	}
}

bool add_missile_icon()
{
	int i = rand() % 4;
	int j = rand() % 20;
	if(game_positions[i][j] == -1)
	{
		game_positions[i][j] = MISSILE;
		setCursor(j, i);
		write(MISSILE);
		return true;
	}
	else
	{
		return false;
	}
}

bool add_obstcle_icon()
{
	int i = rand() % 3;
	int j = (rand() % 17) + 2;
	if(game_positions[i][j] == -1)
	{
		game_positions[i][j] = OBSTACLE;
		setCursor(j, i);
		write(OBSTACLE);
		return true;

	}
	else
	{
		return false;
	}
}

void initial_game()
{
	memset(game_positions, -1, 4 * 20 * sizeof(int));
	initialize_game_positions();
	int health_cnt = total_health;
	int missiles_cnt = total_missiles;
	int obstecle_cnt = total_obstecles;

	for(int i = 0; i < 4; i++)
	{
		for(int j = 0; j < 20; j++)
		{
			if(game_positions[i][j] != -1)
			{
				setCursor(j,i);
				write(game_positions[i][j]);
			}
		}
	}

	while(obstecle_cnt > 0)
	{
		if(add_obstcle_icon())
		{
			obstecle_cnt--;
		}
	}

	while(health_cnt > 0)
	{
		if(add_health_icon())
		{
			health_cnt--;
		}
	}

	while(missiles_cnt > 0)
	{
		if(add_missile_icon())
		{
			missiles_cnt--;
		}
	}


}

void clear_cursor()
{
	setCursor(0,0);
	print(" ");
	setCursor(0,1);
	print(" ");
	setCursor(0,2);
	print(" ");
}

void clear_LCD()
{
	if(_clear)
	{
		clear();
		_clear = false;
	}
	else if(_option)
	{
		clear_cursor();
		_option = false;
	}

}

void move_tank(int player, int i, int j) // check if running missile is coming
{

	switch(game_positions[i][j])
	{
	case TANK_UP:
		if(i != 0) // out of range
		{
			if(game_positions[i - 1][j] == CHANCE_BOX)
			{
				handle_chance_box(player);
				delete_chance_box();
			}
			else if(game_positions[i - 1][j] == HEART)
			{
				setCursor(j, i); // remove tank
				print(" ");
				setCursor(j, i - 1); // remove heart
				print(" ");
				setCursor(j, i - 1);
				write(game_positions[i][j]);
				game_positions[i - 1][j] = game_positions[i][j];
				game_positions[i][j] = -1;

				if(player == 1)
				{
					health_p1 = (health_p1 == 9) ? 9 : health_p1 + 1;
					tank_p1.i--;

				}
				else if(player == 2)
				{
					health_p2 = (health_p2 == 9) ? 9 : health_p2 + 1;
					tank_p2.i--;
				}
			}
			else if(game_positions[i - 1][j] == MISSILE)
			{
				setCursor(j, i); // remove tank
				print(" ");
				setCursor(j, i - 1); // remove missile
				print(" ");
				setCursor(j, i - 1);
				write(game_positions[i][j]);
				game_positions[i - 1][j] = game_positions[i][j];
				game_positions[i][j] = -1;

				if(player == 1)
				{
					missile_p1 = (missile_p1 == 9) ? 9 : missile_p1 + 1;
					tank_p1.i--;

				}
				else if(player == 2)
				{
					missile_p2 = (missile_p2 == 9) ? 9 : missile_p2 + 1;
					tank_p2.i--;
				}
			}

			else if(game_positions[i - 1][j] == -1)
			{
				if( (player == 1 && _missile_p2 && (i - 1) == missile_p2_.i && j == missile_p2_.j) ||
						(player == 2 && _missile_p1 && (i - 1) == missile_p1_.i && j == missile_p1_.j) )
				{

				}
				else
				{
					setCursor(j, i); // remove tank
					print(" ");
					setCursor(j, i - 1);
					write(game_positions[i][j]);
					game_positions[i - 1][j] = game_positions[i][j];
					game_positions[i][j] = -1;

					if(player == 1)
					{
						tank_p1.i--;

					}
					else if(player == 2)
					{
						tank_p2.i--;
					}
				}

			}


		}

		break;

	case TANK_RIGHT:
		if(j != 19)
		{
			if(game_positions[i][j + 1] == CHANCE_BOX)
			{
				handle_chance_box(player);
				delete_chance_box();

			}
			else if(game_positions[i][j + 1] == HEART)
			{
				setCursor(j, i); // remove tank
				print(" ");
				setCursor(j + 1, i); // remove heart
				print(" ");
				setCursor(j + 1, i);
				write(game_positions[i][j]);
				game_positions[i][j + 1] = game_positions[i][j];
				game_positions[i][j] = -1;

				if(player == 1)
				{
					health_p1 = (health_p1 == 9) ? 9 : health_p1 + 1;
					tank_p1.j++;

				}
				else if(player == 2)
				{
					health_p2 = (health_p2 == 9) ? 9 : health_p2 + 1;
					tank_p2.j++;
				}
			}
			else if(game_positions[i][j + 1] == MISSILE)
			{
				setCursor(j, i); // remove tank
				print(" ");
				setCursor(j + 1, i); // remove missile
				print(" ");
				setCursor(j + 1, i);
				write(game_positions[i][j]);
				game_positions[i][j + 1] = game_positions[i][j];
				game_positions[i][j] = -1;

				if(player == 1)
				{
					missile_p1 = (missile_p1 == 9) ? 9 : missile_p1 + 1;
					tank_p1.j++;

				}
				else if(player == 2)
				{
					missile_p2 = (missile_p2 == 9) ? 9 : missile_p2 + 1;
					tank_p2.j++;
				}
			}

			else if(game_positions[i][j + 1] == -1)
			{
				if( (player == 1 && _missile_p2 && (j + 1) == missile_p2_.j && i == missile_p2_.i) ||
										(player == 2 && _missile_p1 && (j + 1) == missile_p1_.j && i == missile_p1_.i) )
				{

				}
				else
				{
					setCursor(j, i); // remove tank
					print(" ");
					setCursor(j + 1, i);
					write(game_positions[i][j]);
					game_positions[i][j + 1] = game_positions[i][j];
					game_positions[i][j] = -1;

					if(player == 1)
					{
						tank_p1.j++;

					}
					else if(player == 2)
					{
						tank_p2.j++;
					}
				}

			}

		}

		break;

	case TANK_DOWN:
		if(i != 3)
		{
			if(game_positions[i + 1][j] == CHANCE_BOX)
			{
				handle_chance_box(player);
				delete_chance_box();

			}
			else if(game_positions[i + 1][j] == HEART)
			{
				setCursor(j, i); // remove tank
				print(" ");
				setCursor(j, i + 1); // remove heart
				print(" ");
				setCursor(j, i + 1);
				write(game_positions[i][j]);
				game_positions[i + 1][j] = game_positions[i][j];
				game_positions[i][j] = -1;

				if(player == 1)
				{
					health_p1 = (health_p1 == 9) ? 9 : health_p1 + 1;
					tank_p1.i++;

				}
				else if(player == 2)
				{
					health_p2 = (health_p2 == 9) ? 9 : health_p2 + 1;
					tank_p2.i++;
				}
			}
			else if(game_positions[i + 1][j] == MISSILE)
			{
				setCursor(j, i); // remove tank
				print(" ");
				setCursor(j, i + 1); // remove missile
				print(" ");
				setCursor(j, i + 1);
				write(game_positions[i][j]);
				game_positions[i + 1][j] = game_positions[i][j];
				game_positions[i][j] = -1;

				if(player == 1)
				{
					missile_p1 = (missile_p1 == 9) ? 9 : missile_p1 + 1;
					tank_p1.i++;

				}
				else if(player == 2)
				{
					missile_p2 = (missile_p2 == 9) ? 9 : missile_p2 + 1;
					tank_p2.i++;
				}
			}

			else if(game_positions[i + 1][j] == -1)
			{
				if( (player == 1 && _missile_p2 && (i + 1) == missile_p2_.i && j == missile_p2_.j) ||
										(player == 2 && _missile_p1 && (i + 1) == missile_p1_.i && j == missile_p1_.j) )
				{

				}
				else
				{
					setCursor(j, i); // remove tank
					print(" ");
					setCursor(j, i + 1);
					write(game_positions[i][j]);
					game_positions[i + 1][j] = game_positions[i][j];
					game_positions[i][j] = -1;

					if(player == 1)
					{
						tank_p1.i++;

					}
					else if(player == 2)
					{
						tank_p2.i++;
					}
				}

			}

		}

		break;

	case TANK_LEFT:
		if(j != 0)
		{
			if(game_positions[i][j - 1] == CHANCE_BOX)
			{
				handle_chance_box(player);
				delete_chance_box();

			}
			if(game_positions[i][j - 1] == HEART)
			{
				setCursor(j, i); // remove tank
				print(" ");
				setCursor(j - 1, i); // remove heart
				print(" ");
				setCursor(j - 1, i);
				write(game_positions[i][j]);
				game_positions[i][j - 1] = game_positions[i][j];
				game_positions[i][j] = -1;

				if(player == 1)
				{
					health_p1 = (health_p1 == 9) ? 9 : health_p1 + 1;
					tank_p1.j--;

				}
				else if(player == 2)
				{
					health_p2 = (health_p2 == 9) ? 9 : health_p2 + 1;
					tank_p2.j--;
				}
			}
			else if(game_positions[i][j - 1] == MISSILE)
			{
				setCursor(j, i); // remove tank
				print(" ");
				setCursor(j - 1, i); // remove missile
				print(" ");
				setCursor(j - 1, i);
				write(game_positions[i][j]);
				game_positions[i][j - 1] = game_positions[i][j];
				game_positions[i][j] = -1;

				if(player == 1)
				{
					missile_p1 = (missile_p1 == 9) ? 9 : missile_p1 + 1;
					tank_p1.j--;

				}
				else if(player == 2)
				{
					missile_p2 = (missile_p2 == 9) ? 9 : missile_p2 + 1;
					tank_p2.j--;
				}
			}

			else if(game_positions[i][j - 1] == -1)
			{
				if( (player == 1 && _missile_p2 && (j - 1) == missile_p2_.j && i == missile_p2_.i) ||
														(player == 2 && _missile_p1 && (j - 1) == missile_p1_.j && i == missile_p1_.i) )
				{

				}
				else
				{
					setCursor(j, i); // remove tank
					print(" ");
					setCursor(j - 1, i);
					write(game_positions[i][j]);
					game_positions[i][j - 1] = game_positions[i][j];
					game_positions[i][j] = -1;

					if(player == 1)
					{
						tank_p1.j--;

					}
					else if(player == 2)
					{
						tank_p2.j--;
					}
				}
			}
		}

		break;

	}
	update_players_info();

}

void QQ(int player, int i, int j)
{
	int direction = (player == 1) ? missile_p1_direction : missile_p2_direction;

	switch(direction)
	{
	case TANK_UP:

		if(player == 1)
		{

			if(i != tank_p1.i)
			{
				setCursor(j, i); // remove missile
				print(" ");
			}
			missile_p1_.i = i - 1;
		}
		else if(player == 2)
		{
			if(i != tank_p2.i)
			{
				setCursor(j, i); // remove missile
				print(" ");
			}
			missile_p2_.i = i - 1;
		}
		if(player == 1 && _missile_p2 && i == missile_p2_.i && j == missile_p2_.j) // hit missiles
		{
			setCursor(j, i);
			write(BURST);
			burst.i = i;
			burst.j = j;
			_burst = true;
			_missile_p1 = false;
			_missile_p2 = false;
		}
		else if(player == 2 && _missile_p1 && i == missile_p1_.i && j == missile_p1_.j) // hit missiles
		{
			setCursor(j, i);
			write(BURST);
			burst.i = i;
			burst.j = j;
			_burst = true;
			_missile_p1 = false;
			_missile_p2 = false;
		}
		else if(game_positions[i - 1][j] == -1) // next cell is empty
		{
			setCursor(j, i - 1);
			write(MISSILE);
		}
		else if(game_positions[i - 1][j] == WALL)
		{
			if(player == 1)
			{
				_missile_p1 = false;
			}
			else if(player == 2)
			{
				_missile_p2 = false;
			}
		}
		else if(game_positions[i - 1][j] == OBSTACLE)
		{
			game_positions[i - 1][j] = -1;
			setCursor(j, i - 1); // remove obstacle
			print(" ");
			if(player == 1)
			{
				_missile_p1 = false;
			}
			else if(player == 2)
			{
				_missile_p2 = false;
			}
		}

		else if(player == 1 && (i - 1) == tank_p2.i &&  j == tank_p2.j) // hit the tank
		{
			if(sound)
			{
				PWM_Change_Tone(220, volume);
				active_buzzer = true;
			}
			reward_p1++;
			health_p2 -= coeff_missile_p1;
			health_p2 = (health_p2 < 0) ? 0 : health_p2;
			update_players_info();
			if(health_p2 <= 0)
			{
				clear();
				state = 5;
				winner = 1;
				end_game();
				send_history();
			}
			_missile_p1 = false;
		}
		else if(player == 2 && (i - 1) == tank_p1.i &&  j == tank_p1.j)
		{
			if(sound)
			{
				PWM_Change_Tone(220, volume);
				active_buzzer = true;

			}
			reward_p2++;
			health_p1 -= coeff_missile_p2;
			health_p1 = (health_p2 < 0) ? 0 : health_p1;
			update_players_info();
			if(health_p1 == 0)
			{
				clear();
				state = 5;
				winner = 2;
				end_game();
				send_history();
			}
			_missile_p2 = false;
		}
		else if(i == 0) // out of range
		{
			if(player == 1)
			{
				_missile_p1 = false;
			}
			else if(player == 2)
			{
				_missile_p2 = false;
			}
		}
		else if(game_positions[i - 1][j] == HEART || game_positions[i - 1][j] == MISSILE)
		{
			deleted_with_missile = true;
			setCursor(j, i - 1);
			write(MISSILE);

		}


		if(deleted_with_missile && (game_positions[i][j] == HEART || game_positions[i][j] == MISSILE ))
		{
			deleted_with_missile = false;
			setCursor(j, i);
			write(game_positions[i][j]);
		}
		break;



	case TANK_DOWN:

		if(player == 1)
		{

			if(i != tank_p1.i)
			{
				setCursor(j, i); // remove missile
				print(" ");
			}
			missile_p1_.i = i + 1;
		}
		else if(player == 2)
		{
			if(i != tank_p2.i)
			{
				setCursor(j, i); // remove missile
				print(" ");
			}
			missile_p2_.i = i + 1;
		}
		if(player == 1 && _missile_p2 && i == missile_p2_.i && j == missile_p2_.j)
		{
			setCursor(j, i); // remove missile
			print(" ");
			setCursor(j, i);
			write(BURST);
			_burst = true;
			burst.i = i;
			burst.j = j;
			_missile_p1 = false;
			_missile_p2 = false;
		}
		else if(player == 2 && _missile_p1 && i == missile_p1_.i && j == missile_p1_.j)
		{
			setCursor(j, i); // remove missile
			print(" ");
			setCursor(j, i);
			write(BURST);
			_burst = true;
			burst.i = i;
			burst.j = j;
			_missile_p1 = false;
			_missile_p2 = false;
		}

		else if(game_positions[i + 1][j] == -1) // next cell is empty
		{
			setCursor(j, i + 1);
			write(MISSILE);
		}
		else if(game_positions[i + 1][j] == WALL)
		{
			if(player == 1)
			{
				_missile_p1 = false;
			}
			else if(player == 2)
			{
				_missile_p2 = false;
			}
		}
		else if(game_positions[i + 1][j] == OBSTACLE)
		{
			game_positions[i + 1][j] = -1;
			setCursor(j, i + 1); // remove obstacle
			print(" ");
			if(player == 1)
			{
				_missile_p1 = false;
			}
			else if(player == 2)
			{
				_missile_p2 = false;
			}
		}

		else if(player == 1 && (i + 1) == tank_p2.i &&  j == tank_p2.j)
		{
			if(sound)
			{
				PWM_Change_Tone(220, volume);
				active_buzzer = true;
			}
			reward_p1++;
			health_p2 -= coeff_missile_p1;
			health_p2 = (health_p2 < 0) ? 0 : health_p2;
			update_players_info();
			if(health_p2 == 0)
			{
				clear();
				state = 5;
				winner = 1;
				end_game();
				send_history();
			}
			_missile_p1 = false;
		}
		else if(player == 2 && (i + 1) == tank_p1.i &&  j == tank_p1.j)
		{
			if(sound)
			{
				PWM_Change_Tone(220, volume);
				active_buzzer = true;
			}
			reward_p2++;
			health_p1 -= coeff_missile_p2;
			health_p1 = (health_p2 < 0) ? 0 : health_p1;
			update_players_info();
			if(health_p1 == 0)
			{
				clear();
				state = 5;
				winner = 2;
				end_game();
				send_history();
			}
			_missile_p2 = false;
		}
		else if(i == 3) // out of range
		{

			if(player == 1)
			{
				_missile_p1 = false;
			}
			else if(player == 2)
			{
				_missile_p2 = false;
			}

		}
		else if(game_positions[i + 1][j] == HEART || game_positions[i + 1][j] == MISSILE)
		{
			deleted_with_missile = true;
			setCursor(j, i + 1); // remove heart or missile
			print(" ");
			setCursor(j, i + 1);
			write(MISSILE);

		}


		if(deleted_with_missile && (game_positions[i][j] == HEART || game_positions[i][j] == MISSILE ))
		{
			deleted_with_missile = false;
			setCursor(j, i);
			write(game_positions[i][j]);
		}

		break;

	case TANK_LEFT:

		if(player == 1)
		{

			if(j != tank_p1.j)
			{
				setCursor(j, i); // remove missile
				print(" ");
			}
			missile_p1_.j = j - 1;
		}
		else if(player == 2)
		{
			if(j != tank_p2.j)
			{
				setCursor(j, i); // remove missile
				print(" ");
			}
			missile_p2_.j = j - 1;
		}
		if(player == 1 && _missile_p2 && i == missile_p2_.i && j == missile_p2_.j)
		{

			setCursor(j, i); // remove missile
			print(" ");
			setCursor(j, i);
			write(BURST);
			_burst = true;
			burst.i = i;
			burst.j = j;
			_missile_p1 = false;
			_missile_p2 = false;
		}
		else if(player == 2 && _missile_p1 && i == missile_p1_.i && j == missile_p1_.j)
		{
			setCursor(j, i); // remove missile
			print(" ");
			setCursor(j, i);
			write(BURST);
			_burst = true;
			burst.i = i;
			burst.j = j;
			_missile_p1 = false;
			_missile_p2 = false;
		}

		else if(game_positions[i][j - 1] == -1) // next cell is empty
		{
			setCursor(j - 1, i);
			write(MISSILE);
		}
		else if(game_positions[i][j - 1] == WALL)
		{
			if(player == 1)
			{
				_missile_p1 = false;
			}
			else if(player == 2)
			{
				_missile_p2 = false;
			}
		}
		else if(game_positions[i][j - 1] == OBSTACLE)
		{
			game_positions[i][j - 1] = -1;
			setCursor(j - 1, i); // remove obstacle
			print(" ");
			if(player == 1)
			{
				_missile_p1 = false;
			}
			else if(player == 2)
			{
				_missile_p2 = false;
			}
		}

		else if(player == 1 && i == tank_p2.i &&  (j - 1) == tank_p2.j)
		{
			if(sound)
			{
				PWM_Change_Tone(220, volume);
				active_buzzer = true;
			}
			reward_p1++;
			health_p2 -= coeff_missile_p1;
			health_p2 = (health_p2 < 0) ? 0 : health_p2;
			update_players_info();
			if(health_p2 == 0)
			{
				clear();
				state = 5;
				winner = 1;
				end_game();
				send_history();
			}
			_missile_p1 = false;
		}
		else if(player == 2 && i == tank_p1.i &&  (j - 1) == tank_p1.j)
		{
			if(sound)
			{
				PWM_Change_Tone(220, volume);
				active_buzzer = true;
			}
			reward_p2++;
			health_p1 -= coeff_missile_p2;
			health_p1 = (health_p2 < 0) ? 0 : health_p1;
			update_players_info();
			if(health_p1 == 0)
			{
				clear();
				state = 5;
				winner = 2;
				end_game();
				send_history();
			}
			_missile_p2 = false;
		}
		else if(j == 0) // out of range
		{
			if(player == 1)
			{
				_missile_p1 = false;
			}
			else if(player == 2)
			{
				_missile_p2 = false;
			}
		}
		else if(game_positions[i][j - 1] == HEART || game_positions[i][j - 1] == MISSILE)
		{
			deleted_with_missile = true;
			setCursor(j - 1, i); // remove heart or missile
			print(" ");
			setCursor(j - 1, i);
			write(MISSILE);

		}
		if(deleted_with_missile && (game_positions[i][j] == HEART || game_positions[i][j] == MISSILE ))
		{
			deleted_with_missile = false;
			setCursor(j, i);
			write(game_positions[i][j]);
		}

		break;

	case TANK_RIGHT:

		if(player == 1)
		{

			if(j != tank_p1.j)
			{
				setCursor(j, i); // remove missile
				print(" ");
			}
			missile_p1_.j = j + 1;
		}
		else if(player == 2)
		{
			if(j != tank_p2.j)
			{
				setCursor(j, i); // remove missile
				print(" ");
			}
			missile_p2_.j = j + 1;
		}
		if(player == 1 && _missile_p2 && i == missile_p2_.i && j == missile_p2_.j)
		{
			setCursor(j, i); // remove missile
			print(" ");
			setCursor(j, i);
			write(BURST);
			_burst = true;
			burst.i = i;
			burst.j = j;
			_missile_p1 = false;
			_missile_p2 = false;
		}
		else if(player == 2 && _missile_p1 && i == missile_p1_.i && j == missile_p1_.j)
		{
			setCursor(j, i); // remove missile
			print(" ");
			setCursor(j, i);
			write(BURST);
			_burst = true;
			burst.i = i;
			burst.j = j;
			_missile_p1 = false;
			_missile_p2 = false;
		}
		else if(game_positions[i][j + 1] == -1) // next cell is empty
		{
			setCursor(j + 1, i);
			write(MISSILE);
		}
		else if(game_positions[i][j + 1] == WALL)
		{
			if(player == 1)
			{
				_missile_p1 = false;
			}
			else if(player == 2)
			{
				_missile_p2 = false;
			}
		}
		else if(game_positions[i][j + 1] == OBSTACLE)
		{
			game_positions[i][j + 1] = -1;
			setCursor(j + 1, i); // remove obstacle
			print(" ");
			if(player == 1)
			{
				_missile_p1 = false;
			}
			else if(player == 2)
			{
				_missile_p2 = false;
			}
		}
		else if(player == 1 && i == tank_p2.i &&  (j + 1) == tank_p2.j)
		{
			if(sound)
			{
				PWM_Change_Tone(220, volume);
				active_buzzer = true;
			}
			reward_p1++;
			health_p2 -= coeff_missile_p1;
			health_p2 = (health_p2 < 0) ? 0 : health_p2;
			update_players_info();
			if(health_p2 == 0)
			{
				clear();
				state = 5;
				winner = 1;
				end_game();
				send_history();
			}
			_missile_p1 = false;
		}
		else if(player == 2 && i == tank_p1.i &&  (j + 1) == tank_p1.j)
		{
			if(sound)
			{
				PWM_Change_Tone(220, volume);
				active_buzzer = true;
			}
			reward_p2++;
			health_p1 -= coeff_missile_p2;
			health_p1 = (health_p2 < 0) ? 0 : health_p1;
			update_players_info();
			if(health_p1 == 0)
			{
				clear();
				state = 5;
				winner = 2;
				end_game();
				send_history();
			}
			_missile_p2 = false;
		}
		else if(j == 19) // out of range
		{
			if(player == 1)
			{
				_missile_p1 = false;
			}
			else if(player == 2)
			{
				_missile_p2 = false;
			}

		}
		else if(game_positions[i][j + 1] == HEART || game_positions[i][j + 1] == MISSILE)
		{
			deleted_with_missile = true;
			setCursor(j + 1, i); // remove heart or missile
			print(" ");
			setCursor(j + 1, i);
			write(MISSILE);
		}


		if(deleted_with_missile && (game_positions[i][j] == HEART || game_positions[i][j] == MISSILE ))
		{
			deleted_with_missile = false;
			setCursor(j, i);
			write(game_positions[i][j]);
		}

		break;
	}

}

void monitor_LCD()
{
	switch(state)
	{
	case 0:
		setCursor(1,0);
		write(TANK_RIGHT);
		setCursor(1,3);
		write(TANK_UP);
		setCursor(18,3);
		write(TANK_LEFT);
		setCursor(18,0);
		write(TANK_DOWN);
		setCursor(4,1);
		print("Tank Battle");
		break;
	case 1:
		setCursor(2,0);
		print("Start");

		setCursor(2,1);
		print("Settings");

		setCursor(2,2);
		print("About");


		switch(main_option)
		{
		case 1:
			setCursor(0,0);
			break;
		case 2:
			setCursor(0,1);
			break;
		case 3:
			setCursor(0,2);
			break;
		}

		print(">");
		break;

	case 2: // start
		if(_start_game)
		{
			_start_game = false;
			PWM_Change_Tone(0, 0);
			initial_game();
			HAL_RTC_GetTime(&hrtc, &myTime, RTC_FORMAT_BIN);
			HAL_RTC_GetDate(&hrtc, &myDate, RTC_FORMAT_BIN);
			sprintf(start_game_time, "%d:%d:%d", myTime.Hours, myTime.Minutes, myTime.Seconds);
		}
		else
		{
			if(change_dir_p1)
			{
				change_dir_p1 = false;
				game_positions[tank_p1.i][tank_p1.j] = change_tank_direction(game_positions[tank_p1.i][tank_p1.j]);
				setCursor(tank_p1.j, tank_p1.i);
				write(game_positions[tank_p1.i][tank_p1.j]);
			}
			if(change_dir_p2)
			{
				change_dir_p2 = false;
				game_positions[tank_p2.i][tank_p2.j] = change_tank_direction(game_positions[tank_p2.i][tank_p2.j]);
				setCursor(tank_p2.j, tank_p2.i);
				write(game_positions[tank_p2.i][tank_p2.j]);
			}
			if(move_p1)
			{
				move_p1 = false;
				move_tank(1, tank_p1.i, tank_p1.j);

			}
			if(move_p2)
			{
				move_p2 = false;
				move_tank(2, tank_p2.i, tank_p2.j);
			}
		}
		break;

	case 3: // settings

		setCursor(0,0);
		char health_str[15];
		sprintf(health_str, "Health:%d", health_p1);
		print(health_str);

		char missiles_str[15];
		sprintf(missiles_str, "Missile:%d", missile_p1);
		setCursor(10,0);
		print(missiles_str);

		setCursor(0,1);
		if(sound)
		{
			print("Sound:On");
		}
		else
		{
			print("Sound:Off");
		}

		char player1_name_str[20];
		sprintf(player1_name_str, "Player1:%s", player1);
		setCursor(0,2);
		print(player1_name_str);

		char player2_name_str[20];
		sprintf(player2_name_str, "Player2:%s", player2);
		setCursor(0,3);
		print(player2_name_str);

		break;

	case 4: // about

		setCursor(7,0);
		print("About");

		HAL_RTC_GetTime(&hrtc, &myTime, RTC_FORMAT_BIN);
		HAL_RTC_GetDate(&hrtc, &myDate, RTC_FORMAT_BIN);

		char datestr[10];
		setCursor(1,1);
		sprintf(datestr, "%2d-%2d-%2d", myDate.Year, myDate.Month, myDate.Date);
		print(datestr);

		char timestr[10];
		setCursor(10,1);
		sprintf(timestr, "%2d:%2d:%2d", myTime.Hours, myTime.Minutes, myTime.Seconds);
		print(timestr);

		setCursor(1,2);
		print("Nastaran");
		setCursor(1,3);
		print("Homa");

		break;
	}

}

int change_tank_direction(tank_direction) // clockwise
{
	switch(tank_direction)
	{
	case TANK_UP:
		return TANK_RIGHT;

	case TANK_RIGHT:
		return TANK_DOWN;

	case TANK_DOWN:
		return TANK_LEFT;

	case TANK_LEFT:
		return TANK_UP;

	}

}

void handle_keypad_button(uint8_t button_number)
{
	switch (button_number)
	{
	case 1:
		if(state == 1)
		{
			main_option = (main_option == 1) ? 3 : (main_option - 1);
			_option = true;
		}

		break;
	case 2:
		if(state == 0)
		{
			state = 1;
			_clear = true;
		}
		else if(state == 1)
		{
			switch(main_option)
			{
			case 1:
				_start_game = true;
				state = 2; // switch to start
				break;
			case 2:
				state = 3; // switch to settings
				break;
			case 3:
				state = 4; // switch to about
				break;
			}
			_clear = true;
		}
		else
		{
			state = 1;
			_clear = true;
		}

		break;
	case 3:
		if(state == 1)
		{
			main_option = (main_option == 3) ? 1 : (main_option + 1);
			_option = true;
		}
	break;

	case 5: // change direction player1
		change_dir_p1 = true;

		break;

	case 7: // change direction player2
		change_dir_p2 = true;

		break;

	case 9: // fire player1
		if(!_missile_p1)
		{
			if(sound)
			{
//				PWM_Change_Tone(180, volume);
				Change_Melody(fire, 3);
				active_buzzer = true;

			}
			missile_p1_direction = game_positions[tank_p1.i][tank_p1.j];
			missile_p1 = (missile_p1 == 0) ? 0 : missile_p1 - 1;
			missile_p1_.i = tank_p1.i;
			missile_p1_.j = tank_p1.j;
			_missile_p1 = true;
			update_players_info();
		}
		break;

	case 11: // fire player2
		if(!_missile_p2)
		{
			if(sound)
			{
//				PWM_Change_Tone(180, volume);
				Change_Melody(fire, 3);
				active_buzzer = true;

			}
			missile_p2_direction = game_positions[tank_p2.i][tank_p2.j];
			missile_p2 = (missile_p2 == 0) ? 0 : missile_p2 - 1;
			missile_p2_.i = tank_p2.i;
			missile_p2_.j = tank_p2.j;
			_missile_p2 = true;
			update_players_info();
		}
		break;

	case 13: // move player1
		move_p1 = true;
		break;

	case 15: // move player2
		move_p2 = true;
		break;
	}
}

void initialize_keypad()
{
	HAL_GPIO_WritePin(Column_ports[0], Column_pins[0], 1);
	HAL_GPIO_WritePin(Column_ports[1], Column_pins[1], 1);
	HAL_GPIO_WritePin(Column_ports[2], Column_pins[2], 1);
	HAL_GPIO_WritePin(Column_ports[3], Column_pins[3], 1);

}

void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
	if (last_gpio_exti + 200 > HAL_GetTick()) // Simple button debouncing
	{
		return;
	}

	last_gpio_exti = HAL_GetTick();

	int8_t row_number = -1;
	int8_t column_number = -1;

	for (uint8_t row = 0; row < 4; row++) // Loop through Rows
	{
		if (GPIO_Pin == Row_pins[row])
		{
		  row_number = row;
		}
	}

	HAL_GPIO_WritePin(Column_ports[0], Column_pins[0], 0);
	HAL_GPIO_WritePin(Column_ports[1], Column_pins[1], 0);
	HAL_GPIO_WritePin(Column_ports[2], Column_pins[2], 0);
	HAL_GPIO_WritePin(Column_ports[3], Column_pins[3], 0);

	for (uint8_t col = 0; col < 4; col++) // Loop through Columns
	{
	HAL_GPIO_WritePin(Column_ports[col], Column_pins[col], 1);
	if (HAL_GPIO_ReadPin(Row_ports[row_number], Row_pins[row_number]))
	{
	  column_number = col;
	}
	HAL_GPIO_WritePin(Column_ports[col], Column_pins[col], 0);
	}

	HAL_GPIO_WritePin(Column_ports[0], Column_pins[0], 1);
	HAL_GPIO_WritePin(Column_ports[1], Column_pins[1], 1);
	HAL_GPIO_WritePin(Column_ports[2], Column_pins[2], 1);
	HAL_GPIO_WritePin(Column_ports[3], Column_pins[3], 1);

	if (row_number == -1 || column_number == -1)
	{
	return; // Reject invalid scan
	}
	//   C0   C1   C2   C3
	// +----+----+----+----+
	// | 1  | 2  | 3  | 4  |  R0
	// +----+----+----+----+
	// | 5  | 6  | 7  | 8  |  R1
	// +----+----+----+----+
	// | 9  | 10 | 11 | 12 |  R2
	// +----+----+----+----+
	// | 13 | 14 | 15 | 16 |  R3
	// +----+----+----+----+
	const uint8_t button_number = row_number * 4 + column_number + 1;

	handle_keypad_button(button_number);
}

void initialize_game_positions()
{
	clear();

	for(int i = 0; i < 4; i++)
	{
		for(int j = 0; j < 20; j++)
		{
			game_positions[i][j] = -1;

		}
	}

	tank_p1.i = 2;
	tank_p1.j = 0;
	tank_p2.i = 1;
	tank_p2.j = 19;
	game_positions[2][0] = TANK_RIGHT;
	game_positions[1][19] = TANK_LEFT;
	game_positions[1][1] = WALL;
	game_positions[2][1] = WALL;
	game_positions[1][18] = WALL;
	game_positions[2][18] = WALL;

}

void update_players_info()
{
	seven_segment.digits[0] = health_p1;
	seven_segment.digits[1] = missile_p1;
	seven_segment.digits[2] = health_p2;
	seven_segment.digits[3] = missile_p2;
}

void send_history()
{
	char reward_p1_str[3], reward_p2_str[3], player1_str[11], player2_str[11], start_game_time_str[9], end_game_time[9];
	int len1 = sprintf(player1_str, "%s\n", player1);
	int len2 = sprintf(player2_str, "%s\n", player2);

	int len3 = sprintf(reward_p1_str, "%d\n", reward_p1);
	int len4 = sprintf(reward_p2_str, "%d\n", reward_p2);

	HAL_RTC_GetTime(&hrtc, &myTime, RTC_FORMAT_BIN);
	HAL_RTC_GetDate(&hrtc, &myDate, RTC_FORMAT_BIN);

	int len5 = sprintf(start_game_time_str, "%s\n", start_game_time);
	int len6 = sprintf(end_game_time, "%d:%d:%d\n", myTime.Hours, myTime.Minutes, myTime.Seconds);


	HAL_UART_Transmit(&huart4, player1_str, len1, HAL_MAX_DELAY);
	HAL_UART_Transmit(&huart4, player2_str, len2, HAL_MAX_DELAY);
	HAL_UART_Transmit(&huart4, reward_p1_str, len3, HAL_MAX_DELAY);
	HAL_UART_Transmit(&huart4, reward_p2_str, len4, HAL_MAX_DELAY);
	HAL_UART_Transmit(&huart4, start_game_time_str, len5, HAL_MAX_DELAY);
	HAL_UART_Transmit(&huart4, end_game_time, len6, HAL_MAX_DELAY);
}

void end_game_1()
{
	setCursor(0, 0);
	write(BURST);
	setCursor(1, 2);
	write(BURST);
	setCursor(19, 0);
	write(BURST);
	setCursor(18, 2);
	write(BURST);
}

void end_game_2()
{
	setCursor(0, 0);
	print(" ");
	setCursor(1, 2);
	print(" ");
	setCursor(19, 0);
	print(" ");
	setCursor(18, 2);
	print(" ");
}

void end_game()
{
	if(winner == 1)
	{
		setCursor(3,1);
		print("Congratulation");
		winner = -1;
		sprintf(winner_reward, "score:%d", reward_p1);

		char Info[20];
		int len = sprintf(Info, "%s : %d\n", player1, reward_p1);
		HAL_UART_Transmit(&huart4, Info, len, HAL_MAX_DELAY);

		setCursor(5,2);
		print(player1);

		setCursor(5,3);
		print(winner_reward);

	}
	else if(winner == 2)
	{
		setCursor(3,1);
		print("Congratulation");
		winner = -1;
		sprintf(winner_reward, "score:%d", reward_p2);

		char Info[20];
		int len = sprintf(Info, "%s : %d\n", player2, reward_p2);
		HAL_UART_Transmit(&huart4, Info, len, HAL_MAX_DELAY);

		setCursor(5,2);
		print(player2);

		setCursor(5,3);
		print(winner_reward);
	}
}




