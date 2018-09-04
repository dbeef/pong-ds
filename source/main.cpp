#include <nds.h>
#include <stdio.h>
#include <nds/arm9/input.h>
#include <nds/arm9/video.h>
#include <nds/arm9/background.h>
#include <nds/arm9/console.h>
#include <nds/arm9/sprite.h>
#include <dswifi9.h>
//Pliki wygenerowane przez grit
#include "../build/dungeon_pong_upper.h"
#include "../build/dungeon_pong_lower.h"
#include "../build/bar.h"
#include "../build/ball.h"

#define BAR_WIDTH 16 //Odpowiada faktycznym rozmiarom obrazków (w pikselach)
#define BAR_HEIGHT 32
#define BALL_WIDTH 8
#define BALL_HEIGHT 8

#define SCREEN_WIDTH 256 //Odpowiada faktycznej rozdzielczości ekranu NDS (w pikselach)
#define SCREEN_HEIGHT 192

#define MAP_OFFSET_UP 32 //Górna granica mapy to 32 piksele w dół od górnej krawędzi ekranu
#define MAP_OFFSET_DOWN 20 //Dolna granica mapy to 20 pikseli w górę od dolnej krawędzi ekranu

#define COLORS_PER_SPRITE_PALETTE 16

int oam_index = 0;
int ball_x_speed = -1; //Startowy zwrot wektora prędkości na oś X to lewo
int ball_y_speed = -1; //Startowy zwrot wektora prędkości na oś Y to góra

typedef struct {
    int x; //pozycja X w układzie współrzędnych. 0 to lewa strona ekranu.
    int y; //pozycja Y w układzie współrzędnych. 0 to górna strona ekranu.
    int width; //szerokość sprajta
    int height; //wysokość sprajta
    u16 *sprite_gfx_mem; //wskaźnik na miejsce w pamięci vram zarezerwowane dla obrazka tego obiektu (paletki/kulki)
    int oam_index; //id obiektu - inkrementowane za każdym nowym obiektem
} GfxObj;

void handle_input(GfxObj *bar_left, GfxObj *bar_right) {

    scanKeys();
    u16 keys_down, keys_pressed;
    keys_down = keysDown(); //Pobierz klawisze niedawno wciśnięte
    keys_pressed = keysHeld(); //Pobierz klawisze wciśnięte cały czas

    if (keys_down & KEY_DOWN || keys_pressed & KEY_DOWN)
        bar_left->y++; //Paletka gracza po lewej jedzie w dół
    else if (keys_down & KEY_UP || keys_pressed & KEY_UP)
        bar_left->y--; //Paletka gracza po lewej jedzie w górę

    if (keys_down & KEY_B || keys_pressed & KEY_B)
        bar_right->y++; //Paletka gracza po prawej jedzie w dół
    else if (keys_down & KEY_X || keys_pressed & KEY_X)
        bar_right->y--; //Paletka gracza po prawej jedzie w górę
}

//Jeśli przekroczono granicę mapy, cofa na granicę i zwraca true
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
        swiWaitForVBlank(); //Czekaj do następnej klatki
        scanKeys(); //Aktualizuj stan wciśniętych klawiszy
        if (keysDown() & KEY_START) break; //Przerwij, jeśli wciśnięto START
    }
}

//Sprawdza czy hitbox jednego obiektu nachodzi na drugi
bool check_collision(GfxObj *o1, GfxObj *o2) {
    return o1->x + o1->width > o2->x && o1->x < o2->x + o2->width &&
           o1->y + o1->height > o2->y && o1->y < o2->y + o2->height;
}

int main() {
    // Obecna konfiguracja banków VRAM:
    //https://mtheall.com/vram.html#SUB=1&T0=2&NT0=864&MB0=0&TB0=1&S0=0&T3=5&MB3=3&S3=2
    videoSetMode(MODE_5_2D);
    videoSetModeSub(MODE_5_2D);
    vramSetBankA(VRAM_A_MAIN_BG_0x06000000);
    vramSetBankC(VRAM_C_SUB_BG_0x06200000);

    //Inicjalizacja tła
    int bg_upper = bgInit(3, BgType_Bmp8, BgSize_B8_256x256, 0, 0);
    int bg_lower = bgInitSub(3, BgType_Bmp8, BgSize_B8_256x256, 3, 0);
    //Inicjalizacja konsoli - ustawiamy defaultowy font oraz rozmiar okna
    consoleInit(nullptr, OBJPRIORITY_0, BgType_Text4bpp, BgSize_T_256x256, 0, 1, false, true);

    //Ekran górny
    dmaCopy(dungeon_pong_upperBitmap, bgGetGfxPtr(bg_upper), dungeon_pong_upperBitmapLen);
    dmaCopy(dungeon_pong_upperPal, BG_PALETTE, dungeon_pong_upperPalLen);
    //Ekran dolny
    dmaCopy(dungeon_pong_lowerBitmap, bgGetGfxPtr(bg_lower), dungeon_pong_lowerBitmapLen);
    dmaCopy(dungeon_pong_lowerPal, BG_PALETTE_SUB, dungeon_pong_lowerPalLen);

    printf("%sDUNGEON PONG", "\n\n\n\n\n\n\n\n\n\t\t\t");
    printf("%sPRESS START", "\n\t\t\t    ");

    oamInit(&oamMain, SpriteMapping_1D_128, false); //Inicjalizacja silnika sprajtów

    GfxObj bar_left = {0, SCREEN_HEIGHT / 4, BAR_WIDTH, BAR_HEIGHT};
    GfxObj bar_right = {SCREEN_WIDTH - BAR_WIDTH, SCREEN_HEIGHT / 2, BAR_WIDTH, BAR_HEIGHT};
    GfxObj ball = {100, 100, BALL_WIDTH, BALL_HEIGHT};

    init_gfx(&bar_left, SpriteSize_16x32, barTiles);
    init_gfx(&bar_right, SpriteSize_16x32, barTiles);
    init_gfx(&ball, SpriteSize_8x8, ballTiles);

    dmaCopy(barPal, &SPRITE_PALETTE[bar_left.oam_index * COLORS_PER_SPRITE_PALETTE], barPalLen);
    dmaCopy(barPal, &SPRITE_PALETTE[bar_right.oam_index * COLORS_PER_SPRITE_PALETTE], barPalLen);
    dmaCopy(ballPal, &SPRITE_PALETTE[ball.oam_index * COLORS_PER_SPRITE_PALETTE], ballPalLen);

    oamSetPalette(&oamMain, bar_left.oam_index, bar_left.oam_index);
    oamSetPalette(&oamMain, bar_right.oam_index, bar_right.oam_index);
    oamSetPalette(&oamMain, ball.oam_index, ball.oam_index);

    while (true) {

        wait_for_x_key();
        consoleClear();

        while (true) {

            //Czekamy do następnej klatki
            swiWaitForVBlank();
            //Aktualizujemy pozycje sprajtów
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

            //Jeśli piłka zderzyła się z mapą, to odwróć zwrot wektora prędkości na oś pionową
            if (limit_positions(&ball))
                ball_y_speed *= -1;

            if (check_collision(&ball, &bar_left)) {
                //Jeśli piłka zderzyła się z paletką po lewej, ustaw zwrot wektora prędkości w prawo
                ball_x_speed = 1;
                consoleClear();
                printf("%sLEWE UDERZENIE!", "\n\n\n\n\n\n\n\n\n\t\t\t");
            } else if (check_collision(&ball, &bar_right)) {
                //Jeśli piłka zderzyła się z paletką po prawej, ustaw zwrot wektora prędkości na lewo
                ball_x_speed = -1;
                consoleClear();
                printf("%sPRAWE UDERZENIE!", "\n\n\n\n\n\n\n\n\n\t\t\t");
            }

            ball.x += ball_x_speed;
            ball.y += ball_y_speed;

            if (ball.x < 0) {
                consoleClear();
                printf("%sPRAWA STRONA WYGRYWA", "\n\n\n\n\n\n\n\n\n\t\t ");
                break;
            } else if (ball.x > SCREEN_WIDTH - BALL_WIDTH) {
                consoleClear();
                printf("%sLEWA STRONA WYGRYWA", "\n\n\n\n\n\n\n\n\n\t\t ");
                break;
            }


        }

        wait_for_x_key();

        ball.x = 100;
        ball.y = 100;
    }

    return 0;
}
