<?php
interface Shape {
  public function area(): float;
  public function draw();
}

trait Colorful {
  private static $palette = [2, 3, 4, 5, 6, 8, 10, 14];
  protected int $color = 2;
  public function pickColor() {
    $this->color = self::$palette[rand(0, count(self::$palette) - 1)];
  }
}

abstract class Figure implements Shape {
  use Colorful;
  public static int $count = 0;
  public function __construct(protected int $x, protected int $y) {
    static::$count++;
    $this->pickColor();
  }
  abstract public function name(): string;
  public function __toString(): string {
    return sprintf("%-7s %5.0f PX", $this->name(), $this->area());
  }
}

class Circle extends Figure {
  public function __construct(int $x, int $y, private int $r) { parent::__construct($x, $y); }
  public function name(): string { return "KREIS"; }
  public function area(): float { return M_PI * $this->r * $this->r; }
  public function draw() { circle($this->x, $this->y, $this->r, $this->color, true); }
}

class Box extends Figure {
  public function __construct(int $x, int $y, protected int $w, protected int $h) { parent::__construct($x, $y); }
  public function name(): string { return "RECHTECK"; }
  public function area(): float { return $this->w * $this->h; }
  public function draw() {
    rect($this->x - intdiv($this->w, 2), $this->y - intdiv($this->h, 2),
         $this->x + intdiv($this->w, 2), $this->y + intdiv($this->h, 2), $this->color, true);
  }
}

class Square extends Box {
  public function __construct(int $x, int $y, int $a) { parent::__construct($x, $y, $a, $a); }
  public function name(): string { return "QUADRAT"; }
}
