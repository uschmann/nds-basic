<?php
// PUNKTE ANZEIGEN, NEUER BALL
function new_ball() {
  global $p1, $p2, $bx, $by, $dx, $dy;
  locate(11, 1); echo $p1;
  locate(17, 1); echo $p2;
  $bx = 124; $by = 92;
  $dx = rand(0, 1) ? 2.5 : -2.5;
  $dy = rand(-15, 15) / 10;
  sprite(6, 1, $bx, $by);
  pause(40);
}
