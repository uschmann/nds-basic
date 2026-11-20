<?php
// PAINT - MALEN MIT DEM STYLUS
// DIE AUSGABE WECHSELT AUF DEN TOUCHSCREEN
// FARBE OBEN ANTIPPEN ODER X, Y: LOESCHEN, B: ENDE
swap_screens(true);
cls(); gcls();
$colors = [0, 2, 8, 7, 5, 3, 6, 4];
$c = 1;
$last = null;

function palette(array $colors, int $c) {
  foreach ($colors as $i => $col) {
    rect(6 + $i * 18, 4, 20 + $i * 18, 16, $col, true);
    rect(4 + $i * 18, 2, 22 + $i * 18, 18, $i == $c ? 0 : 15);
  }
}

palette($colors, $c);
while (!(joy() & JOY_B)) {
  $j = joy();
  if ($j & JOY_X) {
    $c = ($c + 1) % count($colors);
    palette($colors, $c);
    while (joy() & JOY_X) pause(1);
  }
  if ($j & JOY_Y) { gcls(); palette($colors, $c); }
  $t = touch();
  if ($t && $t['y'] < 20) {
    /* tap on the palette */
    $i = intdiv($t['x'] - 4, 18);
    if ($i >= 0 && $i < count($colors)) { $c = $i; palette($colors, $c); }
    $last = null;
  } elseif ($t) {
    if ($last) line($last['x'], $last['y'], $t['x'], $t['y'], $colors[$c]);
    else plot($t['x'], $t['y'], $colors[$c]);
    $last = $t;
  } else $last = null;
  pause(1);
}
