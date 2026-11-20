<?php
// PONG - DU SPIELST LINKS GEGEN DEN COMPUTER
// STEUERUNG: STEUERKREUZ HOCH/RUNTER ODER W/S
// WER ZUERST 7 PUNKTE HAT, GEWINNT
cls(); gcls(); sproff();
rect(0, 0, 255, 191, 1);
for ($y = 6; $y <= 186; $y += 12) line(128, $y, 128, $y + 5, 11);
$py = 80; $cy = 80; $p1 = 0; $p2 = 0;
require "ball.php";   // new_ball()

new_ball();
while ($p1 < 7 && $p2 < 7) {
  $j = joy();
  $k = strtoupper(inkey());
  if ($j & JOY_UP || $k == "W") $py -= 4;
  if ($j & JOY_DOWN || $k == "S") $py += 4;
  $py = max(2, min(166, $py));
  // COMPUTER FOLGT DEM BALL
  if ($cy + 12 < $by) $cy += 3;
  if ($cy + 12 > $by + 4) $cy -= 3;
  $cy = max(2, min(166, $cy));
  $bx += $dx; $by += $dy;
  if ($by < 2) { $by = 2; $dy = -$dy; sound(1); }
  if ($by > 182) { $by = 182; $dy = -$dy; sound(1); }
  if ($dx < 0 && $bx <= 16 && $bx >= 4 && $by + 8 >= $py && $by <= $py + 24) {
    $dx = -$dx + 0.2; $dy = ($by - $py - 8) / 4; sound(0);
  }
  if ($dx > 0 && $bx >= 232 && $bx <= 244 && $by + 8 >= $cy && $by <= $cy + 24) {
    $dx = -$dx - 0.2; $dy = ($by - $cy - 8) / 4; sound(0);
  }
  if ($bx < -8) { $p2++; sound(2); new_ball(); }
  if ($bx > 256) { $p1++; sound(2); new_ball(); }
  for ($i = 0; $i < 3; $i++) {
    sprite($i, 0, 8, $py + $i * 8);
    sprite(3 + $i, 0, 240, $cy + $i * 8);
  }
  sprite(6, 1, $bx, $by);
  pause(1);
}
sproff(6);
if ($p1 == 7) { locate(10, 11); echo "DU GEWINNST!"; }
else { locate(8, 11); echo "COMPUTER GEWINNT"; }
#SPR 0 1111111111111111111111111111111111111111111111111111111111111111
#SPR 1 ..7777...777777.77777777777777777777777777777777.777777...7777..
#SND 0 2 240 84:1
#SND 1 2 240 72:1
#SND 2 1 200 72:2 67:2 60:4
