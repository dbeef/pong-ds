#include <nds.h>

#include <stdio.h>
#include <nds/arm9/input.h>
#include <nds/arm9/video.h>
#include <nds/arm9/background.h>
#include <nds/arm9/console.h>
#include <nds/arm9/sprite.h>

#define BAR_WIDTH 16
#define BAR_HEIGHT 32
#define BALL_WIDTH 8
#define BALL_HEIGHT 8

#define SCREEN_WIDTH 256
#define SCREEN_HEIGHT 192
#define MAP_OFFSET_UP 32
#define MAP_OFFSET_DOWN 20

#define COLORS_PER_PALETTE 16

int oam_index = 0;
int ball_x_speed = -1;
int ball_y_speed = -1;

#include "../build/dungeon_pong_upper.h"
#include "../build/dungeon_pong_lower.h"
#include "../build/bar.h"
#include "../build/ball.h"

typedef struct {
    int x; //pozycja X w układzie współrzędnych. 0 to lewa strona ekranu.
    int y; //pozycja Y w układzie współrzędnych. 0 to górna strona ekranu.
    int width; //szerokość sprajta
    int height; //wysokość sprajta
    u16 *sprite_gfx_mem; //wskaźnik na miejsce w pamięci vram zarezerwowane dla obrazka tego obiektu
    int oam_index; //id obiektu - inkrementowane za każdym nowym obiektem
} GfxObj;

void handle_input(GfxObj *bar_left, GfxObj *bar_right) {

    scanKeys();
    u16 keys_down, keys_pressed;
    keys_down = keysDown();
    keys_pressed = keysHeld();

    if (keys_down & KEY_DOWN || keys_pressed & KEY_DOWN)
        bar_left->y++;
    else if (keys_down & KEY_UP || keys_pressed & KEY_UP)
        bar_left->y--;

    if (keys_down & KEY_B || keys_pressed & KEY_B)
        bar_right->y++;
    else if (keys_down & KEY_X || keys_pressed & KEY_X)
        bar_right->y--;
}

//0,0 to lewy górny róg, rośnie w dół/w prawo
//Funkcja ograniczająca X,Y podanego obiektu do granic mapy, zwraca true jeśli przekroczono granice
bool limit_positions(GfxObj *o) {

    if (o->y < (0 + MAP_OFFSET_UP)) {
        o->y = (0 + MAP_OFFSET_UP);
        return true;
    }
    if (o->y > (SCREEN_HEIGHT - MAP_OFFSET_DOWN) - o->height) {
        o->y = (SCREEN_HEIGHT - MAP_OFFSET_DOWN) - o->height;
        return true;
    }

    return false;
}

//Inicjalizuje miejsce w pamięci dla grafik obiektu
void init_gfx(GfxObj *o, SpriteSize sprite_size, const unsigned int *tiles) {
    o->sprite_gfx_mem = oamAllocateGfx(&oamMain, sprite_size, SpriteColorFormat_16Color);
    dmaCopy(tiles, o->sprite_gfx_mem, o->width * o->height);
    o->oam_index = oam_index;
    oam_index++;
}

void wait_for_x_key() {
    while (true) {
        swiWaitForVBlank();
        scanKeys();
        if (keysDown() & KEY_X) break;
    }
}

//Sprawdza czy hitbox jednego obiektu nachodzi na drugi
bool check_collision(GfxObj *o1, GfxObj *o2) {
    return o1->x + o1->width > o2->x && o1->x < o2->x + o2->width &&
           o1->y + o1->height > o2->y && o1->y < o2->y + o2->height;
}

int main() {
    // set the mode for 2 text layers and two extended background layers
    //https://mtheall.com/vram.html#SUB=1&T0=2&NT0=864&MB0=0&TB0=1&S0=0&T3=5&MB3=3&S3=2 // obecny config
    videoSetMode(MODE_5_2D);
    videoSetModeSub(MODE_5_2D);
    vramSetBankA(VRAM_A_MAIN_BG_0x06000000);
    vramSetBankC(VRAM_C_SUB_BG_0x06200000);

    int bg_upper = bgInit(3, BgType_Bmp8, BgSize_B8_256x256, 0, 0);
    int bg_lower = bgInitSub(3, BgType_Bmp8, BgSize_B8_256x256, 3, 0);

    consoleInit(nullptr, OBJPRIORITY_0, BgType_Text4bpp, BgSize_T_256x256, 0, 1, false, true);

    dmaCopy(dungeon_pong_upperBitmap, bgGetGfxPtr(bg_upper), dungeon_pong_upperBitmapLen);
    dmaCopy(dungeon_pong_upperPal, BG_PALETTE, dungeon_pong_upperPalLen);

    dmaCopy(dungeon_pong_lowerBitmap, bgGetGfxPtr(bg_lower), dungeon_pong_lowerBitmapLen);
    dmaCopy(dungeon_pong_lowerPal, BG_PALETTE_SUB, dungeon_pong_lowerPalLen);

    printf("%sDUNGEON PONG", "\n\n\n\n\n\n\n\n\n\t\t\t  ");
    printf("%sPRESS X TO START", "\n\t\t\t");

    oamInit(&oamMain, SpriteMapping_1D_128, false);

    GfxObj bar_left = {0, SCREEN_HEIGHT / 4, BAR_WIDTH, BAR_HEIGHT};
    GfxObj bar_right = {SCREEN_WIDTH - BAR_WIDTH, SCREEN_HEIGHT / 2, BAR_WIDTH, BAR_HEIGHT};
    GfxObj ball = {100, 100, BALL_WIDTH, BALL_HEIGHT};

    init_gfx(&bar_left, SpriteSize_16x32, barTiles);
    init_gfx(&bar_right, SpriteSize_16x32, barTiles);
    init_gfx(&ball, SpriteSize_8x8, ballTiles);

    dmaCopy(barPal, &SPRITE_PALETTE[bar_left.oam_index * COLORS_PER_PALETTE], barPalLen);
    dmaCopy(barPal, &SPRITE_PALETTE[bar_right.oam_index * COLORS_PER_PALETTE], barPalLen);
    dmaCopy(ballPal, &SPRITE_PALETTE[ball.oam_index * COLORS_PER_PALETTE], ballPalLen);

    oamSetPalette(&oamMain, bar_left.oam_index, ball.oam_index);
    oamSetPalette(&oamMain, bar_right.oam_index, ball.oam_index);
    oamSetPalette(&oamMain, ball.oam_index, ball.oam_index);

    while (true) {

        wait_for_x_key();
        consoleClear();

        while (true) {

            swiWaitForVBlank();

            oamSet(&oamMain, bar_right.oam_index, bar_right.x, bar_right.y, 0, bar_right.oam_index, SpriteSize_16x32,
                   SpriteColorFormat_16Color,
                   bar_right.sprite_gfx_mem, -1, false, false, false, false, false);
            oamSet(&oamMain, bar_left.oam_index, bar_left.x, bar_left.y, 0, bar_left.oam_index, SpriteSize_16x32,
                   SpriteColorFormat_16Color,
                   bar_left.sprite_gfx_mem, -1, false, false, false, false, false);
            oamSet(&oamMain, ball.oam_index, ball.x, ball.y, 0, ball.oam_index, SpriteSize_8x8,
                   SpriteColorFormat_16Color,
                   ball.sprite_gfx_mem, -1, false, false, false, false, false);

            handle_input(&bar_left, &bar_right);

            oamUpdate(&oamMain);

            limit_positions(&bar_left);
            limit_positions(&bar_right);

            if (limit_positions(&ball))
                ball_y_speed *= -1;

            if (check_collision(&ball, &bar_left)) {
                ball_x_speed = 1;
                consoleClear();
                printf("%sLEFT BOING!", "\n\n\n\n\n\n\n\n\n\t\t\t\t");
            } else if (check_collision(&ball, &bar_right)) {
                ball_x_speed = -1;
                consoleClear();
                printf("%sRIGHT BOING!", "\n\n\n\n\n\n\n\n\n\t\t\t\t");
            }

            ball.x += ball_x_speed;
            ball.y += ball_y_speed;

            if (ball.x < 0) {
                consoleClear();
                printf("%sRIGHT SIDE WON", "\n\n\n\n\n\n\n\n\n\t\t\t");
                break;
            } else if (ball.x > SCREEN_WIDTH - BALL_WIDTH) {
                consoleClear();
                printf("%sLEFT SIDE WON", "\n\n\n\n\n\n\n\n\n\t\t\t ");
                break;
            }


        }

        wait_for_x_key();

        ball.x = 100;
        ball.y = 100;
    }

    return 0;
}
