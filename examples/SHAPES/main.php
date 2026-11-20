<?php
// SHAPES - KLASSEN, INTERFACES UND TRAITS
// A: NEUE FORMEN, B: ENDE
require "shapes.php";

function random_shape(): Shape {
  $x = rand(30, 226); $y = rand(40, 170);
  switch (rand(0, 2)) {
    case 0: return new Circle($x, $y, rand(6, 24));
    case 1: return new Box($x, $y, rand(10, 40), rand(10, 30));
    default: return new Square($x, $y, rand(8, 30));
  }
}

while (!(joy() & JOY_B)) {
  cls(); gcls();
  $shapes = [];
  for ($i = 0; $i < 6; $i++) $shapes[] = random_shape();
  foreach ($shapes as $s) {
    $s->draw();
    echo $s, PHP_EOL;
  }
  echo PHP_EOL, Figure::$count, " FORMEN, A = NEU";
  while (!(joy() & (JOY_A | JOY_B))) pause(1);
  while (joy() & JOY_A) pause(1);
}
