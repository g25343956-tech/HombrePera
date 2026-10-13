/*
 * HOMBRE PERA: LA GRAN AVENTURA DIMENSIONAL  -  Nintendo DS (libnds)
 * Fase 2/3: prototipo jugable + Nivel 1 completo (Pradera Frutal) con jefe.
 *
 * Pantalla superior: juego.  Pantalla inferior: HUD de texto (toca para pausar).
 * Controles: Cruceta mover | A saltar (doble salto en el aire) | B golpe
 *            X lanzar semilla explosiva (gasta energia) | L esquivar/embestida
 * Todo el arte se genera por codigo (cadenas de texto), asi es facil de reemplazar.
 */
#include <nds.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

#define LW 128          /* ancho del nivel en tiles de 8x8 */
#define LH 24           /* alto  del nivel en tiles        */
#define FX 256          /* punto fijo 24.8                 */

/* ---------------------------------------------------------------- */
/* Nivel y tiles                                                    */
/* ---------------------------------------------------------------- */
enum { T_AIR, T_DIRT, T_BRICK, T_GRASS, T_SPIKE, T_PLAT };

static u8  lvl[LH][LW];
static u16 *map;
static int bg, cam, lastcol = -999;

static const int GAPS[][2]  = {{20,3},{41,4},{66,3},{85,5}};
static const int PLATS[][3] = {{12,16,4},{25,15,3},{29,12,3},{46,16,4},{52,13,3},
                               {58,16,4},{72,16,3},{76,13,3},{90,15,4},{95,12,3}};
static const int COINS[][2] = {{13,15},{14,15},{15,15},{26,14},{27,14},{30,11},{31,11},
                               {47,15},{48,15},{53,12},{54,12},{60,15},{61,15},{73,15},
                               {77,12},{78,12},{91,14},{92,14},{96,11},{97,11},{35,15},{36,15}};
#define NCOINS ((int)(sizeof(COINS)/sizeof(COINS[0])))
static int coin_on[40];

static int tile_at(int x, int y)
{
    if (x < 0 || x >= LW * 8) return T_DIRT;
    if (y < 0 || y >= LH * 8) return T_AIR;
    return lvl[y >> 3][x >> 3];
}
static int is_solid(int t) { return t == T_DIRT || t == T_BRICK || t == T_GRASS; }

static void build_level(void)
{
    for (int c = 0; c < LW; c++) {
        lvl[20][c] = T_GRASS;
        for (int r = 21; r < LH; r++) lvl[r][c] = T_DIRT;
    }
    for (int g = 0; g < 4; g++)
        for (int c = GAPS[g][0]; c < GAPS[g][0] + GAPS[g][1]; c++)
            for (int r = 20; r < LH; r++) lvl[r][c] = T_AIR;
    for (int p = 0; p < 10; p++)
        for (int c = PLATS[p][0]; c < PLATS[p][0] + PLATS[p][2]; c++) lvl[PLATS[p][1]][c] = T_PLAT;
    for (int c = 34; c <= 36; c++) lvl[16][c] = T_BRICK;      /* ladrillos rompibles */
    for (int c = 62; c <= 63; c++) lvl[17][c] = T_BRICK;
    lvl[19][55] = lvl[18][55] = lvl[19][56] = lvl[18][56] = T_DIRT;   /* muro bajo */
    lvl[19][48] = lvl[19][49] = lvl[19][70] = lvl[19][71] = lvl[19][100] = lvl[19][101] = T_SPIKE;
}

/* streaming de columnas: anillo de 64 columnas (BG de 512x256) */
static void colw(int c)
{
    for (int r = 0; r < 32; r++) {
        u16 t = (c >= 0 && c < LW && r < LH) ? lvl[r][c] : 0;
        int cc = c & 63;
        map[cc < 32 ? r * 32 + cc : 1024 + r * 32 + (cc - 32)] = t;
    }
}
static void stream(void)
{
    int c0 = cam >> 3;
    if (lastcol == -999) { for (int c = c0; c < c0 + 34; c++) colw(c); }
    else if (c0 > lastcol) { for (int c = lastcol + 34; c <= c0 + 33; c++) colw(c); }
    else if (c0 < lastcol) { for (int c = c0; c < lastcol; c++) colw(c); }
    lastcol = c0;
}
static void put_tile(int c, int r, int t)
{
    lvl[r][c] = (u8)t;
    if (lastcol != -999 && c >= lastcol && c < lastcol + 34) colw(c);
}

/* ---------------------------------------------------------------- */
/* Arte                                                              */
/* ---------------------------------------------------------------- */
/* paleta sprites: 1 blanco 2 negro 3 verde 4 verde osc 5 verde claro 6 rojo
   7 amarillo 8 cafe 9 gris a naranja b morado c morado osc */
static const char *const A_PEAR0[16] = {
"......8.........",".....84444......","......3333......",".....553333.....",
".....533333.....","....55333333....","....5331133333..","....5331233333..",
"...55333333333..","...5333222333...","...53333333333..","...55333333333..",
"....533333333...","....44333333....","....88....88....","...2222..2222..."};
static const char *const A_PEAR1[16] = {
"......8.........",".....84444......","......3333......",".....553333.....",
".....533333.....","....55333333....","....5331133333..","....5331233333..",
"...55333333333..","...5333222333...","...53333333333..","...55333333333..",
"....533333333...","....44333333....","...88......88...","..2222....2222.."};
static const char *const A_PEAR2[16] = {
"......8.........",".....84444......","......3333......",".....553333.....",
".....533333.....","....55333333....","....5331133333..","....5331233333..",
"...55333333333..","...5333222333...","...53333333333..","...55333333333..",
"....533333333...","....44333333....","....88..88......","...222..222....."};
static const char *const A_WALK[16] = {
"................","................","................","................",
"................","................","................","......cccc......",
"....cbbbbbbc....","...cbbbbbbbbc...","..cbb1bbbb1bbc..","..cbb2bbbb2bbc..",
"..cbbbbbbbbbbc..","..cbbbbccbbbbc..","...cbbbbbbbbc...","....cccccccc...."};
static const char *const A_FLY[16] = {
"................","................","................","................",
"................","9..............9","99....aaaa....99",".99..aaaaaa..99.",
"..99aa1aa1aa99..","....aa2aa2aa....",".....aaaaaa.....","......aaaa......",
"................","................","................","................"};
static const char *const A_SEED[8]  = {"........","..8888..",".888888.",".889988.",".888888.",".888888.","..8888..","........"};
static const char *const A_COIN[8]  = {"........","..7777..",".777a77.",".77a777.",".777a77.",".777777.","..7777..","........"};
static const char *const A_SPARK[8] = {"...a....","a..7..a.",".a.7.a..","..a7a...","77777a77","..a7a...",".a.7.a..","a..7..a."};
static const char *const A_FLAG[8]  = {"88666...","8866666.","88666...","88......","88......","88......","88......","88......"};

static u16 *g_pear[3], *g_walk, *g_fly, *g_seed, *g_coin, *g_spark, *g_flag, *g_boss;

static void vcopy(const void *src, void *dst, int n) { DC_FlushRange(src, n); dmaCopy(src, dst, n); }

/* convierte arte de texto a bytes 8bpp en orden de tiles 8x8 (mapeo 1D) */
static void art_to_gfx(const char *const *art, int w, int h, u16 *dst)
{
    static u8 buf[1024];
    int tw = w / 8;
    for (int i = 0; i < w * h; i++) buf[i] = 0;
    for (int y = 0; y < h; y++)
        for (int x = 0; x < w; x++) {
            int len = 0; while (art[y][len]) len++;
            char ch = (x < len) ? art[y][x] : '.';
            int v = (ch >= '0' && ch <= '9') ? ch - '0' : (ch >= 'a' && ch <= 'f') ? ch - 'a' + 10 : 0;
            buf[((y >> 3) * tw + (x >> 3)) * 64 + (y & 7) * 8 + (x & 7)] = (u8)v;
        }
    vcopy(buf, dst, w * h);
}

static void make_boss(void)   /* manzana podrida 32x32 generada por codigo */
{
    static u8 buf[1024];
    for (int i = 0; i < 1024; i++) buf[i] = 0;
    for (int y = 0; y < 32; y++)
        for (int x = 0; x < 32; x++) {
            int dx = x - 16, dy = y - 18, d2 = dx * dx + dy * dy, v = 0;
            if (d2 <= 14 * 14) v = (d2 > 12 * 12) ? 12 : 6;
            if (d2 <= 12 * 12 && ((x * 5 + y * 3) % 13 == 0)) v = 11;     /* manchas podridas */
            if (y >= 2 && y <= 6 && x >= 15 && x <= 17) v = 8;              /* tallo */
            if (y >= 3 && y <= 6 && x >= 18 && x <= 23) v = 4;              /* hoja  */
            if (y >= 13 && y <= 17 && ((x >= 8 && x <= 13) || (x >= 18 && x <= 23))) v = 1; /* ojos */
            if (y >= 15 && y <= 17 && ((x >= 11 && x <= 13) || (x >= 18 && x <= 20))) v = 2;
            if (y >= 24 && y <= 26 && x >= 10 && x <= 22) v = 2;            /* boca */
            buf[((y >> 3) * 4 + (x >> 3)) * 64 + (y & 7) * 8 + (x & 7)] = (u8)v;
        }
    vcopy(buf, g_boss, 1024);
}

static void make_tiles(void)
{
    static u8 tb[6][64];
    for (int y = 0; y < 8; y++)
        for (int x = 0; x < 8; x++) {
            int i = y * 8 + x;
            tb[T_DIRT][i]  = ((x * 5 + y * 3) % 7 == 0) ? 9 : 8;
            tb[T_BRICK][i] = (y % 4 == 0 || (x + ((y / 4) & 1) * 4) % 8 == 0) ? 9 : 10;
            tb[T_GRASS][i] = (y < 3) ? 3 : (((x * 5 + y * 3) % 7 == 0) ? 9 : 8);
            tb[T_SPIKE][i] = (((x % 4 == 1 || x % 4 == 2) ? y >= 2 : y >= 5)) ? 7 : 0;
            tb[T_PLAT][i]  = (y < 3) ? 11 : 0;
        }
    vcopy(tb, bgGetGfxPtr(bg), sizeof(tb));
    BG_PALETTE[0] = RGB15(10, 20, 31);  BG_PALETTE[3] = RGB15(8, 24, 6);
    BG_PALETTE[7] = RGB15(24, 24, 26);  BG_PALETTE[8] = RGB15(18, 10, 4);
    BG_PALETTE[9] = RGB15(12, 6, 2);    BG_PALETTE[10] = RGB15(26, 12, 6);
    BG_PALETTE[11] = RGB15(22, 16, 6);
}

/* ---------------------------------------------------------------- */
/* Fisica                                                             */
/* ---------------------------------------------------------------- */
typedef struct { int x, y, vx, vy, w, h; } Body;       /* x,y en 1/256 px */

static int hits(int x, int y, int w, int h)
{
    for (int ty = y >> 3; ty <= (y + h - 1) >> 3; ty++)
        for (int tx = x >> 3; tx <= (x + w - 1) >> 3; tx++)
            if (is_solid(tile_at(tx << 3, ty << 3))) return 1;
    return 0;
}
static int on_plat(int x, int y, int w, int h)
{
    int b = y + h - 1;
    if (b & 7) return 0;
    for (int tx = x >> 3; tx <= (x + w - 1) >> 3; tx++)
        if (tile_at(tx << 3, b) == T_PLAT) return 1;
    return 0;
}
static void mvx(Body *b, int *blk)
{
    int s = b->vx > 0 ? 1 : -1, n = b->vx > 0 ? b->vx : -b->vx;
    *blk = 0;
    while (n > 0) {
        int st = n > FX ? FX : n; n -= st;
        int nx = b->x + s * st;
        if (hits(nx >> 8, b->y >> 8, b->w, b->h)) { *blk = 1; b->vx = 0; return; }
        b->x = nx;
    }
}
static void mvy(Body *b, int *ground)
{
    int s = b->vy > 0 ? 1 : -1, n = b->vy > 0 ? b->vy : -b->vy;
    *ground = 0;
    while (n > 0) {
        int st = n > FX ? FX : n; n -= st;
        int ny = b->y + s * st;
        if (hits(b->x >> 8, ny >> 8, b->w, b->h) ||
            (s > 0 && on_plat(b->x >> 8, ny >> 8, b->w, b->h))) {
            if (s > 0) *ground = 1;
            b->vy = 0; return;
        }
        b->y = ny;
    }
}
static int overlap(int ax, int ay, int aw, int ah, int bx, int by, int bw, int bh)
{ return ax < bx + bw && bx < ax + aw && ay < by + bh && by < ay + ah; }

/* ---------------------------------------------------------------- */
/* Entidades                                                          */
/* ---------------------------------------------------------------- */
typedef struct { Body b; int type, hp, on, dir, t, fl, ox, by, swing, state, st; } En;
#define NEN 12
static En en[NEN];
static En *boss;

typedef struct { int x, y, vx, vy, on; } Seed;
static Seed seeds[6];
typedef struct { int x, y, t; } Spark;
static Spark sparks[8];

static Body P;
static int pdir = 1, pjumps, pcoy, pinv, patk, pdash, pcd, php = 3, penergy = 10, pdead;
static int swing, coins, kills, cpx, cpy, boss_on, won, paused, frame;

static const signed char SIN32[32] = {0,3,6,9,11,14,16,17,18,17,16,14,11,9,6,3,0,-3,-6,-9,-11,-14,-16,-17,-18,-17,-16,-14,-11,-9,-6,-3};

static void spark(int x, int y)
{ for (int i = 0; i < 8; i++) if (!sparks[i].t) { sparks[i].x = x; sparks[i].y = y; sparks[i].t = 14; return; } }

static void add_en(int col, int type)
{
    for (int i = 0; i < NEN - 1; i++) if (!en[i].on) {
        En *e = &en[i]; *e = (En){0};
        e->on = 1; e->type = type; e->dir = -1; e->hp = type ? 1 : 2;
        e->b.w = 12; e->b.h = 10; e->b.x = col * 8 * FX;
        if (type == 0) e->b.y = (160 - 10) * FX;
        else { e->by = (11 - (col % 3)) * 8; e->b.y = e->by * FX; e->ox = col * 8; }
        return;
    }
}

static void init_level(void)
{
    for (int i = 0; i < NEN; i++) en[i].on = 0;
    static const int W[] = {15,30,38,50,60,80,92};
    for (int i = 0; i < 7; i++) add_en(W[i], 0);
    add_en(45, 1); add_en(70, 1); add_en(88, 1);
    boss = &en[NEN - 1]; *boss = (En){0};
    boss->on = 1; boss->type = 2; boss->hp = 12; boss->b.w = 26; boss->b.h = 26;
    boss->b.x = 118 * 8 * FX; boss->b.y = (160 - 26) * FX; boss->dir = -1;
    boss_on = 0; won = 0;
    for (int i = 0; i < NCOINS; i++) coin_on[i] = 1;
    coins = kills = 0;
}

static void respawn(void)
{
    P.x = cpx * FX; P.y = cpy * FX; P.vx = P.vy = 0;
    php = 3; pinv = 90; pdead = 0; pdash = 0; patk = 0;
    for (int i = 0; i < 6; i++) seeds[i].on = 0;
    if (boss->hp > 0 && boss_on) { boss_on = 0; boss->hp = 12; boss->b.x = 118 * 8 * FX; boss->state = 0; for (int r = 16; r < 20; r++) put_tile(97, r, T_AIR); }
    lastcol = -999;
}

static void hurt(int from_dir)
{
    if (pinv > 0 || pdash > 0 || pdead) return;
    php--; pinv = 90;
    P.vy = -900; P.vx = from_dir * 500;
    spark((P.x >> 8) + 2, (P.y >> 8) + 2);
    if (php <= 0) pdead = 60;
}

static void kill_en(En *e)
{
    e->on = 0; kills++; spark(e->b.x >> 8, e->b.y >> 8); spark((e->b.x >> 8) + 8, (e->b.y >> 8) + 4);
    if (penergy < 10) penergy++;
}

static void damage_en(En *e, int d)
{
    if (e->type == 2 && boss->state != 3) { e->fl = 4; return; }   /* solo vulnerable aturdido */
    e->hp -= d; e->fl = 8;
    if (e->type == 2) { spark(e->b.x >> 8, e->b.y >> 8); if (e->hp <= 0) { e->on = 0; won = 1; for (int k = 0; k < 8; k++) spark((e->b.x >> 8) + k * 3, (e->b.y >> 8) + (k & 3) * 6); } }
    else if (e->hp <= 0) kill_en(e);
}

static void explode(int x, int y)
{
    for (int i = 0; i < 4; i++) spark(x - 6 + i * 4, y - 4 + (i & 1) * 6);
    for (int i = 0; i < NEN; i++) {
        En *e = &en[i];
        if (!e->on && !(e == boss && boss_on && boss->hp > 0)) continue;
        if (e == boss && !boss_on) continue;
        if (overlap(x - 20, y - 20, 40, 40, e->b.x >> 8, e->b.y >> 8, e->b.w, e->b.h)) damage_en(e, 2);
    }
    for (int ty = (y - 16) >> 3; ty <= (y + 16) >> 3; ty++)
        for (int tx = (x - 16) >> 3; tx <= (x + 16) >> 3; tx++)
            if (tx >= 0 && tx < LW && ty >= 0 && ty < LH && lvl[ty][tx] == T_BRICK) { put_tile(tx, ty, T_AIR); coins++; }
}

/* ---------------------------------------------------------------- */
/* Actualizacion                                                      */
/* ---------------------------------------------------------------- */
static void update_player(int held, int down)
{
    if (pdead) {
        if (--pdead == 0) respawn();
        return;
    }
    if (pinv > 0) pinv--;
    if (pcd > 0) pcd--;
    int ground, blk;

    if ((down & KEY_L) && pcd == 0 && pdash == 0) { pdash = 9; pcd = 40; }
    if (pdash > 0) { pdash--; P.vx = pdir * 1100; P.vy = 0; }
    else {
        if (held & KEY_LEFT)       { P.vx -= 45; pdir = -1; }
        else if (held & KEY_RIGHT) { P.vx += 45; pdir = 1; }
        else P.vx = P.vx * 7 / 8;
        if (P.vx > 560) P.vx = 560;
        if (P.vx < -560) P.vx = -560;
        P.vy += 85; if (P.vy > 1300) P.vy = 1300;
    }

    if (down & KEY_A) {
        if (pcoy > 0) { P.vy = -1500; pcoy = 0; pjumps = 1; }
        else if (pjumps < 2) { P.vy = -1350; pjumps++; spark((P.x >> 8) + 4, (P.y >> 8) + 12); }
    }
    if (!(held & KEY_A) && P.vy < -500) P.vy = -500;

    if ((down & KEY_B) && patk == 0) { patk = 14; swing++; }
    if ((down & KEY_X) && penergy >= 2) {
        for (int i = 0; i < 6; i++) if (!seeds[i].on) {
            seeds[i].on = 1; seeds[i].x = (P.x >> 8) + 4; seeds[i].y = (P.y >> 8) + 4;
            seeds[i].vx = pdir * 900; seeds[i].vy = -450; penergy -= 2; break;
        }
    }
    if (patk > 0) patk--;

    mvx(&P, &blk);
    mvy(&P, &ground);
    if (ground) { pcoy = 6; pjumps = 0; } else if (pcoy > 0) pcoy--;
    if ((P.y >> 8) > LH * 8 + 16) { php = 0; pdead = 40; }
    if (tile_at((P.x >> 8) + 5, (P.y >> 8) + P.h - 2) == T_SPIKE) hurt(-pdir);
    if ((P.x >> 8) > cpx && (P.x >> 8) > 64 * 8 && cpx < 64 * 8) { cpx = 64 * 8; cpy = 146; spark(cpx, 150); }
    if (frame % 150 == 0 && penergy < 10) penergy++;

    int px = P.x >> 8, py = P.y >> 8;
    for (int i = 0; i < NCOINS; i++)
        if (coin_on[i] && overlap(px, py, P.w, P.h, COINS[i][0] * 8, COINS[i][1] * 8, 8, 8)) {
            coin_on[i] = 0; coins++; if (penergy < 10) penergy++;
        }

    /* hitbox del golpe: activa en los frames centrales */
    if (patk >= 4 && patk <= 11) {
        int hx = pdir > 0 ? px + 10 : px - 14;
        for (int ty = (py + 1) >> 3; ty <= (py + 13) >> 3; ty++)
            for (int tx = hx >> 3; tx <= (hx + 13) >> 3; tx++)
                if (tx >= 0 && tx < LW && ty >= 0 && ty < LH && lvl[ty][tx] == T_BRICK) { put_tile(tx, ty, T_AIR); coins++; spark(tx * 8, ty * 8); }
        for (int i = 0; i < NEN; i++) {
            En *e = &en[i];
            if (!e->on || e->swing == swing) continue;
            if (e == boss && !boss_on) continue;
            if (overlap(hx, py + 1, 14, 12, e->b.x >> 8, e->b.y >> 8, e->b.w, e->b.h)) {
                e->swing = swing; damage_en(e, 1);
                if (e != boss) { e->b.vx = pdir * 600; }
            }
        }
    }
}

static void update_seeds(void)
{
    for (int i = 0; i < 6; i++) {
        Seed *s = &seeds[i]; if (!s->on) continue;
        s->vy += 55; s->x += s->vx >> 8; s->y += s->vy >> 8;
        int hit = hits(s->x - 2, s->y - 2, 4, 4);
        for (int k = 0; k < NEN && !hit; k++) {
            En *e = &en[k];
            if (!e->on || (e == boss && !boss_on)) continue;
            if (overlap(s->x - 3, s->y - 3, 6, 6, e->b.x >> 8, e->b.y >> 8, e->b.w, e->b.h)) hit = 1;
        }
        if (hit || s->y > LH * 8 + 16) { if (hit) explode(s->x, s->y); s->on = 0; }
    }
}

static void update_enemies(void)
{
    int px = P.x >> 8, py = P.y >> 8;
    for (int i = 0; i < NEN; i++) {
        En *e = &en[i];
        if (!e->on) continue;
        if (e->fl > 0) e->fl--;
        int blk, gr;
        if (e->type == 0) {
            e->b.vy += 85; if (e->b.vy > 1200) e->b.vy = 1200;
            int kn = e->b.vx;                              /* retroceso por golpe */
            e->b.vx = (kn > 120 || kn < -120) ? kn * 3 / 4 : e->dir * 150;
            mvx(&e->b, &blk); mvy(&e->b, &gr);
            int fx = (e->dir > 0 ? (e->b.x >> 8) + e->b.w + 1 : (e->b.x >> 8) - 2);
            int below = tile_at(fx, (e->b.y >> 8) + e->b.h + 2);
            if (blk || (gr && !is_solid(below) && below != T_PLAT)) e->dir = -e->dir;
            if ((e->b.y >> 8) > LH * 8) e->on = 0;
        } else if (e->type == 1) {
            e->t++;
            e->b.x += e->dir * 150;
            if ((e->b.x >> 8) < e->ox - 64) e->dir = 1;
            if ((e->b.x >> 8) > e->ox + 64) e->dir = -1;
            e->b.y = (e->by + SIN32[(e->t >> 1) & 31]) * FX;
        } else if (e->type == 2) {
            if (!boss_on) { if (px > 104 * 8) { boss_on = 1; for (int r = 16; r < 20; r++) put_tile(97, r, T_DIRT); } continue; }
            e->t++;
            e->b.vy += 85; if (e->b.vy > 1200) e->b.vy = 1200;
            int fast = e->hp <= 6;
            if (e->state == 0) {                           /* camina hacia el jugador */
                e->dir = (px < (e->b.x >> 8)) ? -1 : 1;
                e->b.vx = e->dir * (fast ? 260 : 200);
                if (e->t > (fast ? 70 : 110)) { e->state = 1; e->t = 0; }
            } else if (e->state == 1) {                    /* aviso antes de embestir */
                e->b.vx = 0; e->fl = (e->t & 4) ? 1 : 0;
                if (e->t > 30) { e->state = 2; e->t = 0; }
            } else if (e->state == 2) {                    /* embestida */
                e->b.vx = e->dir * (fast ? 1000 : 800);
                if (e->t > 50) { e->state = 0; e->t = 0; }
            } else {                                       /* aturdido: ventana para atacar */
                e->b.vx = 0; e->fl = 0;
                if (e->t > 100) { e->state = 0; e->t = 0; }
            }
            mvx(&e->b, &blk); mvy(&e->b, &gr);
            if (blk && e->state == 2) { e->state = 3; e->t = 0; e->b.vx = 0; spark(e->b.x >> 8, (e->b.y >> 8) + 10); }
        }
        /* contacto con el jugador */
        if (!pdead && overlap(px + 2, py + 1, P.w - 4, P.h - 1, e->b.x >> 8, e->b.y >> 8, e->b.w, e->b.h)) {
            if (e->type != 2 && P.vy > 300 && py + P.h - 4 < (e->b.y >> 8) + 6) {
                P.vy = -1000; pjumps = 1; damage_en(e, 1);
            } else hurt(px < (e->b.x >> 8) ? -1 : 1);
        }
    }
}

/* ---------------------------------------------------------------- */
/* Dibujo                                                             */
/* ---------------------------------------------------------------- */
static void put(int id, int x, int y, u16 *g, SpriteSize sz, int flip)
{
    x -= cam;
    int vis = !(x < -32 || x > 256 || y < -32 || y > 192);
    oamSet(&oamMain, id, x, y, 0, 0, sz, SpriteColorFormat_256Color, g, -1, false, !vis, flip, false, false);
}
static void hide(int id) { oamSetHidden(&oamMain, id, true); }

static void draw(void)
{
    int px = P.x >> 8, py = P.y >> 8;
    if (pdead || (pinv > 0 && (frame & 4))) hide(0);
    else {
        u16 *g = (P.vy < -100 || P.vy > 200 ? g_pear[2] : ((P.vx > 60 || P.vx < -60) && ((frame >> 3) & 1) ? g_pear[1] : g_pear[0]));
        put(0, px - 3, py + (patk ? 0 : 0), g, SpriteSize_16x16, pdir < 0);
    }
    /* golpe: destello delante del personaje */
    if (patk >= 4 && patk <= 11) put(61, (pdir > 0 ? px + 12 : px - 6), py + 3, g_spark, SpriteSize_8x8, 0); else hide(61);
    for (int i = 0; i < NEN - 1; i++) {
        En *e = &en[i];
        if (!e->on || (e->fl & 1)) { hide(1 + i); continue; }
        put(1 + i, (e->b.x >> 8) - 2, (e->b.y >> 8) - (e->type ? 5 : 6), e->type ? g_fly : g_walk, SpriteSize_16x16, e->dir > 0);
    }
    if (boss->on && boss_on && !(boss->fl & 1)) put(60, (boss->b.x >> 8) - 3, (boss->b.y >> 8) - 6, g_boss, SpriteSize_32x32, boss->dir > 0);
    else hide(60);
    for (int i = 0; i < 6; i++) { if (seeds[i].on) put(13 + i, seeds[i].x - 4, seeds[i].y - 4, g_seed, SpriteSize_8x8, 0); else hide(13 + i); }
    for (int i = 0; i < 8; i++) {
        if (sparks[i].t) { sparks[i].t--; put(19 + i, sparks[i].x, sparks[i].y - (14 - sparks[i].t) / 3, g_spark, SpriteSize_8x8, 0); }
        else hide(19 + i);
    }
    for (int i = 0; i < NCOINS; i++) { if (coin_on[i]) put(27 + i, COINS[i][0] * 8, COINS[i][1] * 8 + ((frame >> 4) & 1), g_coin, SpriteSize_8x8, 0); else hide(27 + i); }
    if (cpx >= 64 * 8) put(59, 64 * 8, 152, g_flag, SpriteSize_8x8, 0); else put(59, 64 * 8, 152, g_flag, SpriteSize_8x8, 1);
}

static void hud(void)
{
    consoleClear();
    iprintf("\n  HOMBRE PERA  -  Pradera Frutal\n\n");
    iprintf("  VIDA     %d/3\n", php < 0 ? 0 : php);
    iprintf("  ENERGIA  [");
    for (int i = 0; i < 10; i++) iprintf(i < penergy ? "#" : "-");
    iprintf("]\n  MONEDAS  %d\n  ENEMIGOS %d\n\n", coins, kills);
    int pr = (P.x >> 8) * 100 / (LW * 8);
    iprintf("  AVANCE   %d%%\n", pr > 100 ? 100 : pr);
    if (boss_on && !won) { iprintf("  JEFE     ["); for (int i = 0; i < 12; i++) iprintf(i < boss->hp ? "#" : "-"); iprintf("]\n  (golpealo cuando se aturde)\n"); }
    iprintf("\n");
    if (won) iprintf("  *** NIVEL COMPLETO! ***\n");
    else if (paused) iprintf("  PAUSA - toca para seguir\n");
    else iprintf("  Toca la pantalla: pausa\n");
    iprintf("\n  A salto/doble  B golpe\n  X semilla      L esquivar\n");
}

static void vsync(void) { while (REG_VCOUNT >= 192) { } while (REG_VCOUNT < 192) { } }

/* ---------------------------------------------------------------- */
int main(void)
{
    videoSetMode(MODE_0_2D | DISPLAY_SPR_ACTIVE | DISPLAY_BG0_ACTIVE | DISPLAY_SPR_1D_LAYOUT);
    videoSetModeSub(MODE_0_2D);
    vramSetBankA(VRAM_A_MAIN_BG);
    vramSetBankB(VRAM_B_MAIN_SPRITE);
    lcdMainOnTop();
    BG_PALETTE[0] = RGB15(31, 0, 0);          /* ROJO: el programa arranco */
    consoleDemoInit();
    BG_PALETTE[0] = RGB15(31, 16, 0);        /* NARANJA: consola lista */
    iprintf("\n  HOMBRE PERA\n  v0.2 arrancando...\n");

    bg = bgInit(0, BgType_Text8bpp, BgSize_T_512x256, 0, 1);
    map = (u16 *)bgGetMapPtr(bg);
    build_level();
    make_tiles();
    iprintf("  1 fondo ok\n");
    BG_PALETTE[0] = RGB15(31, 28, 0);        /* AMARILLO: fondo listo */

    oamInit(&oamMain, SpriteMapping_1D_32, false);
    u16 pal[] = {0, RGB15(31,31,31), RGB15(1,1,2), RGB15(8,24,6), RGB15(3,13,3), RGB15(18,30,12), RGB15(31,6,6),
                 RGB15(31,28,4), RGB15(18,10,4), RGB15(20,20,22), RGB15(31,16,3), RGB15(20,6,26), RGB15(10,2,14)};
    for (int i = 0; i < 13; i++) SPRITE_PALETTE[i] = pal[i];
    for (int i = 0; i < 3; i++) g_pear[i] = oamAllocateGfx(&oamMain, SpriteSize_16x16, SpriteColorFormat_256Color);
    g_walk = oamAllocateGfx(&oamMain, SpriteSize_16x16, SpriteColorFormat_256Color);
    g_fly  = oamAllocateGfx(&oamMain, SpriteSize_16x16, SpriteColorFormat_256Color);
    g_seed = oamAllocateGfx(&oamMain, SpriteSize_8x8, SpriteColorFormat_256Color);
    g_coin = oamAllocateGfx(&oamMain, SpriteSize_8x8, SpriteColorFormat_256Color);
    g_spark = oamAllocateGfx(&oamMain, SpriteSize_8x8, SpriteColorFormat_256Color);
    g_flag = oamAllocateGfx(&oamMain, SpriteSize_8x8, SpriteColorFormat_256Color);
    g_boss = oamAllocateGfx(&oamMain, SpriteSize_32x32, SpriteColorFormat_256Color);
    art_to_gfx(A_PEAR0, 16, 16, g_pear[0]); art_to_gfx(A_PEAR1, 16, 16, g_pear[1]); art_to_gfx(A_PEAR2, 16, 16, g_pear[2]);
    art_to_gfx(A_WALK, 16, 16, g_walk); art_to_gfx(A_FLY, 16, 16, g_fly);
    art_to_gfx(A_SEED, 8, 8, g_seed); art_to_gfx(A_COIN, 8, 8, g_coin);
    art_to_gfx(A_SPARK, 8, 8, g_spark); art_to_gfx(A_FLAG, 8, 8, g_flag);
    make_boss();
    iprintf("  2 sprites ok\n");
    BG_PALETTE[0] = RGB15(0, 26, 0);         /* VERDE: sprites listos */

    P.w = 10; P.h = 14; cpx = 24; cpy = 146;
    init_level();
    respawn();
    cam = 0;
    iprintf("  3 nivel ok\n");
    BG_PALETTE[0] = RGB15(10, 20, 31);       /* CELESTE: todo listo */

    for (;;) {
        scanKeys();
        int held = keysHeld(), down = keysDown();
        frame++;
        if (down & KEY_TOUCH) paused = !paused;
        if (!paused) {
            update_player(held, down);
            update_seeds();
            update_enemies();
        }
        int target = (P.x >> 8) - 110;
        if (target < 0) target = 0;
        if (target > LW * 8 - 256) target = LW * 8 - 256;
        if (lastcol == -999) cam = target; else cam += (target - cam) / 6;
        stream();
        bgSetScroll(bg, cam, 0);
        bgUpdate();
        draw();
        if (frame % 6 == 0) hud();
        vsync();
        oamUpdate(&oamMain);
    }
    return 0;
}
