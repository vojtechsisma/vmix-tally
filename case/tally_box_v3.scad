// ===============================================================
//  vMix ESP32 Tally - krabicka v3 pro univerzalni desku 100 x 50 mm
//  + drzak 18650 nalepeny vedle ESP32, presahuje spodni dlouhou hranu desky.
//  ESP32 u leveho okraje desky (vysky stredem), USB konektor u leve steny,
//  2 LED (ohnute) skrz horni dlouhou stenu, diry 2/3 a 5/6 zleva.
//  Prava stena: otvor pro nabijeci micro USB (TP4056) a pro vypinac.
//  part = "box" | "lid" | "foot" | "assembly"
// ===============================================================
part = "assembly";
$fn = 48;
eps = 0.01;

// ---------- deska ----------
pcb_l = 100.0;          // X
pcb_w = 50.0;           // Y
pcb_t = 1.6;
pcb_gap = 0.4;          // vule kolem desky (na kazde strane)
pitch = 2.54;
pcb_cols = 38;          // otvory podel 100 mm
pcb_margin_x = (pcb_l - (pcb_cols - 1) * pitch) / 2;   // ~3.0 mm

// ---------- drzak 18650 (BHC-18650-1A, otevreny, dratove vyvody) ----------
hold_l = 78.0;          // delka drzaku (X)  - zmer a uprav
hold_w = 22.0;          // sirka drzaku (Y)
hold_h = 19.5;          // vyska drzaku i s clankem
esp_w  = 28.2;          // sirka ESP32 DevKit V1
esp_l  = 51.8;
hold_gap = 0.5;         // mezera mezi ESP32 a drzakem
hold_clear = 1.0;       // vule mezi drzakem a spodni stenou
// o kolik drzak presahuje spodni hranu desky (ESP je ve stredu sirky desky)
hold_overhang = hold_w + hold_gap + esp_w/2 - pcb_w/2;     // ~11,6 mm

// ---------- krabicka ----------
wall = 2.0;
floor_t = 2.0;
IL = pcb_l + 2 * pcb_gap;      // vnitrni X
pcb_y0 = hold_clear + hold_overhang + pcb_gap;   // spodni hrana desky (Y)
IW = pcb_y0 + pcb_w + pcb_gap; // vnitrni Y (deska u horni steny, pod ni misto pro presah drzaku)
IH = 29.5;                     // vnitrni vyska - lem vika musi byt nad drzakem 18650
standoff = 4.0;                // mezera pod deskou (spoje na spodni strane)
ledge_w = 2.0;                 // sirka opery pod okrajem desky
board_bot = floor_t + standoff;
board_top = board_bot + pcb_t;

// ---------- LED (horni dlouha stena = Y max) ----------
led_d = 5.3;                           // 5mm LED
led_above_board = 3.0;                 // stred LED nad horni stranou desky (ohnute, lezi na desce)
led_holes = [[2,3], [5,6]];            // dvojice der (nozicky) pocitano zleva od rohu u radku 1
function hole_x(n) = pcb_gap + pcb_margin_x + (n - 1) * pitch;
led_x = [ for (p = led_holes) (hole_x(p[0]) + hole_x(p[1])) / 2 ];
led_z = board_top + led_above_board;

// ---------- USB (leva kratka stena = X min, uprostred) ----------
usb_w = 12.0;  usb_h = 7.5;            // otvor pro koncovku kabelu
usb_conn_bottom = 13.0;                // spodni hrana USB konektoru nad horni stranou desky (zmereno)
usb_above_board = usb_conn_bottom + 1.4;   // stred konektoru (micro USB je vysoky ~2,8 mm)
usb_z = board_top + usb_above_board;
usb_y = pcb_y0 + pcb_w / 2;            // ESP32 ve stredu sirky desky, USB u leve steny

// ---------- prava kratka stena: nabijeni TP4056 + vypinac ----------
charge_cutout = true;                  // otvor pro micro USB na modulu TP4056
charge_w = 11.0;  charge_h = 7.0;
charge_y = pcb_y0 + pcb_w / 2;         // stred otvoru (Y) - posun podle umisteni modulu
charge_z = board_top + 2.5;            // modul lezi na desce, konektor ~2,5 mm nad ni
switch_hole = true;                    // posuvny prepinac 2 polohy, 6 pinu (typ S6P / SS-22 s prirubou)
sw_slot_l = 8.5;                       // vyrez pro pacicku: delka (pacicka ~3 mm + zdvih ~4 mm + vule)
sw_slot_w = 4.0;                       // vyrez: vyska
sw_screws = true;                      // 2 otvory pro sroubky prirub (montaz zevnitr)
sw_screw_d = 2.2;                      // M2 (u nekterych kusu M2.5 -> 2.7)
sw_screw_dist = 19.0;                  // roztec der v prirube - ZMER na svem kusu
sw_flange_l = 23.3;  sw_flange_h = 7.5; // priruba (jen pro nahled a kontrolu kolizi)
switch_y = pcb_y0 + 17;                // mezi drzakem 18650 a nabijecim USB (mimo drzak), vodorovne podel steny
switch_z = floor_t + IH - 9;

// ---------- sroubky vika: rohove bloky UVNITR, nad deskou ----------
cb = 7.0;                      // rozmer rohoveho bloku (x a y)
post_above_board = 9.0;        // plny blok zacina 9 mm nad deskou, pod nim zkoseni 45 st. k rohu
screw_hole = 2.6;  screw_clear = 3.4;  screw_head = 6.0;
screw_depth = 12;              // hloubka diry pro samorezny M3 (sroub M3 x 10-12)

// ---------- vicko ----------
lid_t = 2.0;  lip_h = 3.0;  lip_w = 1.6;  tol = 0.3;

// ---------- zachytky desky (na dlouhych stenach) ----------
snap_len = 8;  snap_depth = 0.7;  snap_h = 1.6;
snap_x = [IL * 0.3, IL * 0.7];

// ---------- cold shoe patka (samostatny dil, prisroubuje se zespodu) ----------
shoe_w = 18.4; shoe_l = 18.0; shoe_t = 1.8; neck_w = 11.5; neck_h = 2.5;
base_w = 24; base_t = 2.5; foot_hole_dx = 5;

// ===============================================================
corners = [[cb/2, cb/2], [IL - cb/2, cb/2], [cb/2, IW - cb/2], [IL - cb/2, IW - cb/2]];
post_z = board_top + post_above_board;

module rrect(l, w, h, r) { hull() for (x=[r, l-r], y=[r, w-r]) translate([x,y,0]) cylinder(r=r, h=h); }
module outline(h) { translate([-wall, -wall, 0]) rrect(IL + 2*wall, IW + 2*wall, h, 2); }
// rohovy blok: nahore plny, dole zkoseny 45 st. do rohu (tiskne se bez podpor)
module corner_block() {
    top = floor_t + IH;
    hull() {
        translate([0, 0, post_z]) cube([cb, cb, top - post_z]);
        translate([0, 0, post_z - cb]) cube([0.1, 0.1, 0.1]);
    }
}
// otvor pro LED jako zarez shora (deska s LED se vklada shora), vicko ho zaslepi jazyckem
module led_slot(x) {
    translate([x, IW - eps, led_z]) rotate([-90,0,0]) cylinder(d=led_d, h=wall + 2);
    translate([x - led_d/2, IW - eps, led_z]) cube([led_d, wall + 2, floor_t + IH - led_z + 1]);
}

module box() {
    difference() {
        outline(floor_t + IH);
        translate([0, 0, floor_t]) cube([IL, IW, IH + 1]);
        // USB
        translate([-wall - 1, usb_y, usb_z]) rotate([0,90,0])
            hull() for (d=[-1,1]) translate([0, d*(usb_w - usb_h)/2, 0]) cylinder(d=usb_h, h=wall + 2);
        // LED zarezy
        for (x = led_x) led_slot(x);
        // nabijeci micro USB (TP4056)
        if (charge_cutout) translate([IL - 1, charge_y, charge_z]) rotate([0,90,0])
            hull() for (d=[-1,1]) translate([0, d*(charge_w - charge_h)/2, 0]) cylinder(d=charge_h, h=wall + 2);
        // posuvny vypinac: obdelnikovy vyrez pro pacicku + otvory pro sroubky priruby
        if (switch_hole) {
            translate([IL - 1, switch_y - sw_slot_l/2, switch_z - sw_slot_w/2]) cube([wall + 2, sw_slot_l, sw_slot_w]);
            if (sw_screws) for (d = [-1, 1])
                translate([IL - 1, switch_y + d * sw_screw_dist/2, switch_z]) rotate([0,90,0]) cylinder(d=sw_screw_d, h=wall + 2);
        }
        // zahlubene M3 pro patku
        for (dx = [-foot_hole_dx, foot_hole_dx]) translate([IL/2 + dx, pcb_y0 + pcb_w/2, -1]) {
            cylinder(d=screw_clear, h=floor_t + 2);
            translate([0, 0, floor_t + 1 - 1.7]) cylinder(d1=screw_clear, d2=screw_head + 0.4, h=1.7 + eps);
        }
        // vetraci / popisek ve dne
        translate([IL/2, pcb_y0 + pcb_w/2 + 12, -eps]) linear_extrude(0.6) mirror([1,0,0])
            text("vMix TALLY", size=6, halign="center", valign="center", font="Liberation Sans:style=Bold");
    }
    // opera pod okrajem desky (rameno kolem desky)
    difference() {
        translate([0, pcb_y0 - pcb_gap, floor_t - eps]) cube([IL, IW - pcb_y0 + pcb_gap, standoff + eps]);
        translate([ledge_w, pcb_y0 - pcb_gap + ledge_w, floor_t - 1]) cube([IL - 2*ledge_w, IW - pcb_y0 + pcb_gap - 2*ledge_w, standoff + 2]);
    }
    // police pod presahujici casti drzaku 18650 - vyska = horni strana desky
    difference() {
        translate([0, 0, floor_t - eps]) cube([IL, pcb_y0 - pcb_gap, board_top - floor_t + eps]);
        // odlehceni zespodu (setri material, tiskne se bez podpor diky 45 st. stene)
        translate([cb + 3, 2, floor_t - 1]) cube([IL - 2*cb - 6, pcb_y0 - pcb_gap - 4, board_top - floor_t - 1.2]);
    }
    // zachytky (deska se zacvakne pod ne)
    for (x = snap_x, side = [1]) {          // jen u horni steny, dole drzi desku drzak
        y = side == 0 ? 0 : IW;
        translate([x - snap_len/2, y, board_top + 0.2])
            mirror([0, side, 0]) rotate([0, 90, 0]) linear_extrude(snap_len)
                polygon([[0, 0], [-snap_h, 0], [0, snap_depth]]);
    }
}

module box_with_posts() {
    difference() {
        union() {
            box();
            translate([0, 0, 0]) corner_block();
            translate([IL, 0, 0]) mirror([1,0,0]) corner_block();
            translate([0, IW, 0]) mirror([0,1,0]) corner_block();
            translate([IL, IW, 0]) mirror([1,0,0]) mirror([0,1,0]) corner_block();
        }
        for (c = corners) translate([c[0], c[1], floor_t + IH - screw_depth]) cylinder(d=screw_hole, h=screw_depth + 1);
    }
}

module lid() {
    difference() {
        union() {
            outline(lid_t);
            difference() {
                translate([tol, tol, lid_t - eps]) difference() {
                    cube([IL - 2*tol, IW - 2*tol, lip_h]);
                    translate([lip_w, lip_w, -1]) cube([IL - 2*tol - 2*lip_w, IW - 2*tol - 2*lip_w, lip_h + 2]);
                }
                // vynechat lem u rohovych bloku
                for (c = corners) translate([c[0] - cb/2 - 1, c[1] - cb/2 - 1, 0]) cube([cb + 2, cb + 2, lid_t + lip_h + 1]);
            }
            // jazycky do LED zarezu (vicko lezi obracene: z = 0 je vrch krabicky)
            for (x = led_x) {
                tongue = floor_t + IH - led_z;          // az ke stredu LED, s pulkruhovym vyrezem
                difference() {
                    translate([x - led_d/2 + 0.15, IW, lid_t - eps]) cube([led_d - 0.3, wall, tongue]);
                    translate([x, IW - 1, lid_t + tongue]) rotate([-90,0,0]) cylinder(d=led_d + 0.2, h=wall + 2);
                }
            }
        }
        for (c = corners) translate([c[0], c[1], -1]) {
            cylinder(d=screw_clear, h=lid_t + 2);
            cylinder(d1=screw_head + 0.4, d2=screw_clear, h=2.7);
        }
        translate([IL/2, pcb_y0 + pcb_w/2, -eps]) linear_extrude(0.6) mirror([1,0,0])
            text("TALLY", size=10, halign="center", valign="center", font="Liberation Sans:style=Bold");
    }
}

module foot() {
    difference() {
        union() {
            translate([-shoe_l/2, -shoe_w/2, 0]) rrect(shoe_l, shoe_w, shoe_t, 1.5);
            translate([-shoe_l/2, -neck_w/2, 0]) cube([shoe_l, neck_w, shoe_t + neck_h]);
            translate([-shoe_l/2, -base_w/2, shoe_t + neck_h - eps]) rrect(shoe_l, base_w, base_t, 2);
        }
        for (dx = [-foot_hole_dx, foot_hole_dx]) translate([dx, 0, -1]) cylinder(d=screw_hole, h=20);
    }
}

module dummies() {
    color("darkgreen", 0.85) translate([pcb_gap, pcb_y0, board_bot]) cube([pcb_l, pcb_w, pcb_t]);
    // ESP32 na listach u leveho okraje desky, USB proti otvoru v leve stene
    ex = pcb_gap + 0.5;  ey = usb_y - esp_w / 2;
    esp_pcb_z = board_top + usb_conn_bottom - 1.6;     // podle zmerene vysky USB konektoru
    color("black") translate([ex + 6, ey + 1, board_top]) cube([esp_l - 12, esp_w - 2, esp_pcb_z - board_top]);
    color("steelblue", 0.9) translate([ex, ey, esp_pcb_z]) cube([esp_l, esp_w, 1.6]);
    color("silver") translate([ex - 0.8, usb_y - 4.5, board_top + usb_conn_bottom]) cube([7.5, 9, 3.2]);
    // drzak 18650 + clanek
    hx = pcb_gap + pcb_l/2 - hold_l/2;  hy = ey - hold_gap - hold_w;
    color("dimgray") difference() {
        translate([hx, hy, board_top]) cube([hold_l, hold_w, hold_h - 6]);
        translate([hx + 2, hy + 1.5, board_top + 1.5]) cube([hold_l - 4, hold_w - 3, hold_h]);
    }
    if (switch_hole) color("orange") translate([IL - 13, switch_y - sw_flange_l/2, switch_z - sw_flange_h/2]) cube([13, sw_flange_l, sw_flange_h]);
    color("royalblue") translate([hx + 4, hy + hold_w/2, board_top + 1.5 + 9.25]) rotate([0,90,0]) cylinder(d=18.5, h=65);
    for (i = [0:1]) color(i == 0 ? "red" : "lime")
        translate([led_x[i], IW - 6, led_z]) rotate([-90,0,0]) cylinder(d=5, h=8);
}

if (part == "box") box_with_posts();
else if (part == "lid") lid();
else if (part == "foot") translate([0,0,shoe_l/2]) rotate([0,90,0]) foot();
else {
    box_with_posts(); dummies();
    color("lightgray", 0.45) translate([0, 0, floor_t + IH + 10 + lid_t]) mirror([0,0,1]) lid();
}
