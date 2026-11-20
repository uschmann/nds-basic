# NDS PHP

*[English](README.en.md) · Deutsch*

Ein kleiner, retro-inspirierter PHP-Interpreter für den Nintendo DS – programmiert wird wie früher
direkt auf dem Gerät, wie früher am Heimcomputer, aber in einem PHP-Dialekt.

* **Oberer Bildschirm:** Code-Editor mit Zeilennummern und Statuszeile bzw. die REPL-Konsole
  mit der Programmausgabe
* **Unterer Bildschirm:** Retro-Tastatur (Touch) mit LCD-Statusanzeige
* **Titelbildschirm:** Beim Start schwenkt oben ein Low-Poly-Heimcomputer (Tastatur, Röhrenmonitor,
  Diskettenlaufwerk), gerendert mit der 3D-Hardware des DS. Unten listet er alle gespeicherten Programme
  (12 pro Seite, mit ← → blättern): Antippen lädt das Programm direkt in den Editor. „NEUES PROGRAMM“
  bzw. „ZUM PROGRAMM“ oder eine beliebige Taste startet den Editor ohne zu laden.
  `EXIT` (in der REPL) kehrt zum Titelbildschirm zurück, Programm und Daten bleiben erhalten

## Bauen

Benötigt nur Docker (Image `devkitpro/devkitarm`):

```bash
./build.sh          # erzeugt nds-basic.nds
./build.sh clean
```

Interpreter-Tests auf dem PC (ohne DS):

```bash
make -C host test test-files test-examples
```

## Bedienung

Der obere Bildschirm ist entweder der **Code-Editor** oder die **REPL-Konsole**; `SCRN` wechselt
zwischen beiden.

| Aktion | Tastatur | Tasten |
|---|---|---|
| Programm starten / abbrechen | `RUN` / `STOP` | START |
| Zwischen Editor und REPL-Konsole wechseln | `SCRN` | – |
| Hilfeseite mit allen Befehlen (↑↓ scrollen, ←→ blättern) | – | SELECT |
| Cursor bewegen | `←` `→` `↑` `↓` | Steuerkreuz |
| Zeilenanfang / -ende, seitenweise blättern | – | L/R + ←→, L/R + ↑↓ |
| Neue Zeile (Einrückung wird übernommen) | `RET` | A |
| Zeichen links vom Cursor löschen (am Zeilenanfang: Zeilen verbinden) | `DEL` | B |
| Ganze Zeile löschen | `CLR` | – |
| Großbuchstaben und Zweitbelegung (2× tippen = Lock) | `SHFT` | L / R halten |
| Sprite-Editor | – | X |
| Sound-Editor | – | Y |

Die Tastatur tippt Kleinbuchstaben; mit Shift kommen Großbuchstaben und die für PHP nötigen Zeichen
`\ { } [ ] | : *` (jeweils oben links auf der Taste).

**Editor:** Der Code wird direkt bearbeitet – einfach den Cursor hinsetzen und tippen. Die
Zeilennummern am linken Rand dienen nur der Orientierung (und den Fehlermeldungen), eingeben muss
man sie nicht. Lange Zeilen werden umbrochen. Nach einer Zeile, die mit `{` endet, rückt `RET` zwei
Zeichen weiter ein, eine `}` auf einer sonst leeren Zeile rückt wieder aus. Bricht ein Programm mit
einem Fehler ab, steht der Cursor beim Zurückwechseln in der Fehlerzeile.

**REPL-Konsole:** Sie zeigt die Ausgabe der Programme und hat einen Prompt `>`, an dem man einzelne
PHP-Anweisungen sofort ausführt (`echo 2 + 3;`, `$x = strlen("abc");`) oder Befehle wie `RUN`,
`SAVE "NAME"` und `LOAD "NAME"` eingibt. ↑↓ holen frühere Eingaben zurück. `START` bzw. `RUN`
starten das Programm aus dem Editor heraus und schalten dafür auf die Konsole.

**Sprache:** Die Oberfläche (Titelbildschirm, Hinweise, Hilfeseite) richtet sich nach der Systemsprache des DS.
Ist der DS auf Deutsch eingestellt, erscheint sie auf Deutsch, bei jeder anderen Sprache auf Englisch.
Fehlermeldungen und Befehle sind immer englisch.

## Der PHP-Dialekt

`RUN` fügt alle Zeilen in ihrer Reihenfolge zu einem Programm zusammen, übersetzt es in Bytecode und
führt ihn aus. Blöcke mit `{ }` dürfen sich also über beliebig viele Zeilen erstrecken, `<?php` am
Anfang ist erlaubt, aber nicht nötig. Fehler werden mit der Zeilennummer gemeldet
(`Parse error in line 3: unexpected 'echo'`, `Error in line 2: Division by zero`).

```php
function fib($n) {
  return $n < 2 ? $n : fib($n - 1) + fib($n - 2);
}
for ($i = 0; $i < 10; $i++) echo fib($i), " ";
$name = input("NAME? ");
echo "\nHALLO $name!";
```

Unterstützt werden:

* **Werte:** `int` (32 Bit), `float`, Strings, `true`/`false`/`null`, Arrays als geordnete Maps mit
  Zahlen- oder Text-Schlüsseln (`[1, 2]`, `['a' => 1]`, `array(…)`, `$a[] = 3`), Arrays werden wie in
  PHP als Wert kopiert
* **Strings:** `'…'` und `"…"` mit `\n \t \\ \" \$` und Variablen (`"Hallo $name"`, `"$a[0]"`,
  `"{$a['x']['y']}"`)
* **Operatoren:** `+ - * / % **`, `.`, `== != === !== < > <= >= <=>`, `&& || ! and or xor`,
  `& | ^ ~ << >>`, `?:`, `??`, `= += -= *= /= .= %= **= &= |=`, `++ --`, Casts `(int) (float) (string) (bool) (array)`
* **Anweisungen:** `echo`, `print`, `if/elseif/else`, `while`, `do…while`, `for`, `foreach ($a as $k => $v)`,
  `switch/case/default`, `break n`, `continue n`, `function name($a, $b = 1) { … return …; }`,
  `global`, `unset`, `isset`, `empty`, `exit`/`die`, Kommentare `//`, `#`, `/* */`
* **Funktionen:** `strlen substr strpos str_repeat str_pad str_replace str_contains str_starts_with
  str_ends_with strtoupper strtolower ucfirst lcfirst trim ltrim rtrim strrev explode implode sprintf
  printf number_format chr ord intval floatval strval boolval is_numeric is_int is_float is_string
  is_bool is_array is_null gettype abs floor ceil round sqrt pow sin cos tan asin acos atan atan2
  exp log log10 pi fmod intdiv deg2rad rad2deg min max rand mt_rand srand count in_array
  array_search array_key_exists array_keys array_values array_sum array_reverse array_slice
  array_merge array_fill range array_push array_pop array_shift array_unshift sort rsort shuffle
  print_r var_dump`
* **Konstanten:** `PHP_EOL`, `PHP_INT_MAX`, `M_PI`, `STR_PAD_LEFT/RIGHT/BOTH`, `JOY_UP` … `JOY_R`

Nicht dabei sind u. a. Klassen, Closures, Referenzen (`&`), `static`, `match`, `include` und
Ausnahmen. Schlüsselwörter und Funktionsnamen sind wie in PHP unabhängig von Groß-/Kleinschreibung,
Variablen nicht. Zahlen mit Komma sind auf dem DS `float` mit einfacher Genauigkeit.

In der **REPL** bleiben die Variablen und Funktionen des letzten `RUN` erhalten
(`echo $punkte;`, `echo fib(12);`). `CLR` löscht alle Variablen.

### Text und Eingabe

```
cls()               Ausgabe löschen
locate(x, y)        Cursor auf Spalte x, Zeile y
color(c)            Textfarbe 0-15
input("TEXT")       zeigt TEXT und liest eine Zeile ein (Ergebnis ist ein String)
inkey()             zuletzt getippte Taste der Bildschirmtastatur oder ""
joy()               gehaltene DS-Tasten als Bitmaske
pause(n)            n Frames (1/60 s) warten, sleep(s) wartet Sekunden
```

`joy()` liefert 1 = hoch, 2 = runter, 4 = links, 8 = rechts, 16 = A, 32 = B, 64 = X, 128 = Y, 256 = L,
512 = R, dafür gibt es die Konstanten `JOY_UP` … `JOY_R` (z. B. `if (joy() & JOY_UP) $y -= 2;`).
START bleibt die Abbruchtaste.

## Grafik

Auf dem oberen Bildschirm liegt hinter dem Text eine Grafikebene mit 256×192 Punkten
(x 0–255, y 0–191). Farben sind 0–15 wie bei `color()`, `-1` löscht einen Punkt.
Farbe `c` und Füllung `f` sind optional (Standardfarbe über `gcolor()`).

```
gcls()                        Grafik löschen
gcolor(c)                     Standard-Zeichenfarbe setzen
plot(x, y[, c])               Punkt setzen
line(x1, y1, x2, y2[, c])     Linie
rect(x1, y1, x2, y2[, c[, f]])  Rechteck, f = 1/true gefüllt
circle(x, y, r[, c[, f]])     Kreis, f = 1/true gefüllt
paint(x, y[, c])              zusammenhängende Fläche füllen
point(x, y)                   Farbe eines Punktes (-1 = leer)
```

Beispiel:

```php
gcls(); cls();
for ($i = 0; $i < 16; $i++) circle(128, 96, 90 - $i * 5, $i, true);
line(0, 191, 255, 0, 2);
```

Die Grafik ist auf dem Ausgabebildschirm sichtbar, im Editor wird sie ausgeblendet.

## Sprites

Es gibt 256 Sprite-Slots (0–255) mit je 8×8 Punkten in den 16 Farben oder transparent.
Angezeigt werden sie als **Hardware-Sprites** des DS: 64 Sprite-Objekte (0–63), die über
Text und Grafik liegen und sich bewegen lassen, ohne den Hintergrund zu verändern.

```
sprite(o, n, x, y[, s])   Sprite-Objekt o mit Bild n an x,y zeigen bzw. bewegen, s = Größe 1-4
sproff([o])               Objekt o ausblenden, ohne Angabe alle
sprhit(a, b)              true, wenn sich die sichtbaren Objekte a und b überlappen
stamp(n, x, y[, s])       Bild n in die Grafikebene stempeln (s = 1-8), z. B. für Hintergründe
sprset(n, x, y, c)        einzelnen Punkt (x,y 0-7) eines Bildes setzen, c = -1 = transparent
SPRED [n]                 Sprite-Editor öffnen (REPL-Befehl, sonst Taste X)
```

Sprites sind nur auf dem Ausgabebildschirm sichtbar und dürfen teilweise außerhalb des
Bildschirms liegen. `RUN` und `NEW` blenden alle Objekte aus. Ändert man ein Bild (Editor oder
`sprset()`), aktualisieren sich alle Objekte, die es benutzen.

**Sprite-Editor** (Taste X oder `SPRED n`): unten ein 8×8-Raster zum Malen mit dem Stylus,
Farbpalette (Karo = transparent), Vorschau in 4×/2×/1×, Knöpfe für Slot wechseln, Kopieren,
Einfügen, Leeren und OK. Oben steht eine Übersicht aller 256 Slots; der aktuelle ist gelb
umrahmt. Steuerkreuz wechselt den Slot (←→ ±1, ↑↓ ±16), B/X/OK schließt den Editor.

## Sounds

Es gibt 10 Sound-Slots (0–9). Jeder Slot ist eine Melodie aus bis zu 64 Noten mit
Klang (Rechteck 12,5 %, 25 %, 50 % oder Rauschen) und Tempo.

```
sound(n)          Slot n im Hintergrund abspielen, das Programm läuft weiter
sound(n, true)    Slot n abspielen und warten, bis er fertig ist
sound(-1)         Wiedergabe stoppen
SNDED [n]         Sound-Editor öffnen (REPL-Befehl, sonst Taste Y)
```

**Sound-Editor** (Taste Y oder `SNDED n`): unten eine Klaviatur über zwei Oktaven. Ein Tipp
auf eine Taste spielt den Ton und hängt ihn mit der gewählten Länge (1/16 bis 1/1) an die
Melodie an. Weitere Knöpfe: Pause (`REST`), letzte Note löschen (`DEL`), Slot leeren (`CLR`),
Klang, Tempo, Oktave, Slot wechseln, `PLAY`/`STOP` und `OK`. Oben stehen die Noten des
Slots (z. B. `C#4  8` = Achtelnote) und eine Übersicht aller Slots; beim Abspielen wird die
aktuelle Note markiert. Steuerkreuz: ←→ Slot, ↑↓ Oktave; B/Y/OK schließt den Editor.

## Beispiele

Im Ordner [`examples/`](examples/) liegen fertige Programme. Zum Ausprobieren die `.PHP`-Dateien
auf die SD-Karte in den Ordner `/nds-basic` kopieren; sie erscheinen dann in der Liste auf dem
Titelbildschirm (oder `LOAD "PONG"`).

| Datei | Inhalt | Steuerung |
|---|---|---|
| [PONG.PHP](examples/PONG.PHP) | Pong gegen den Computer, bis 7 Punkte. Hardware-Sprites für Schläger und Ball, Sounds für Treffer | Steuerkreuz ↑↓ oder W/S |
| [SNAKE.PHP](examples/SNAKE.PHP) | Snake mit Äpfeln, wird mit jedem Apfel länger und schneller. Körper per `stamp()` in der Grafik, Kopf und Apfel als Sprites | Steuerkreuz oder W/A/S/D |

Beide Dateien enthalten ihre Sprites (`#SPR`) und Sounds (`#SND`) gleich mit.

## Speichern

Programme werden als PHP-Dateien auf der SD-Karte (Flashcart per DLDI oder DSi-SD) im Ordner
`/nds-basic` gespeichert:

```
SAVE "SPIEL"      speichert als /nds-basic/SPIEL.PHP (SAVE ohne Namen = letzter Name)
LOAD "SPIEL"      lädt das Programm (ersetzt das aktuelle)
DIR               listet alle .PHP-Dateien
DELETE "SPIEL"    löscht die Datei
```

Dateinamen: bis 8 Zeichen aus A–Z, 0–9, `_`, `-`. Die Datei beginnt mit `<?php` und enthält die
Programmzeilen genau wie im Editor (samt Einrückung und Leerzeilen), kann also am PC bearbeitet
werden. Benutzte Sprites und Sounds stehen am Ende als
PHP-Kommentare: `#SPR n` + 64 Zeichen (`.` = transparent, `0`–`F` = Farbe) und
`#SND n klang tempo note:länge …` (Note als MIDI-Nummer, 0 = Pause, Länge in 16teln).
Ohne SD-Karte meldet der Interpreter beim Start `NO SD CARD`.

Im Emulator melonDS: unter *Config → Emu settings → DLDI* die SD-Emulation aktivieren
(optional mit „Sync SD to folder“).

## Website

Die Landingpage liegt in [`site/`](site/), auf Englisch (`index.html`, `docs.html`) und Deutsch
(`index.de.html`, `docs.de.html`) mit Sprachumschalter im Kopf. Der Workflow
[`.github/workflows/pages.yml`](.github/workflows/pages.yml) baut bei jedem Push auf `main` die ROM im
devkitPro-Container, setzt die Seite mit `nds-basic.nds`, dieser README und der englischen (als Doku-Seiten) und den
Beispielen zusammen und veröffentlicht sie über GitHub Pages.

Einmalig im Repository einstellen: *Settings → Pages → Build and deployment → Source: GitHub Actions*.

Lokal ansehen:

```bash
./build.sh && sh site/build-site.sh && python3 -m http.server -d _site 8000
```
