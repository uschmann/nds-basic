# NDS BASIC

Ein kleiner, retro-inspirierter BASIC-Interpreter für den Nintendo DS.

* **Oberer Bildschirm:** Programm-Listing mit Zeilennummern, Statuszeile und Eingabezeile
  (bzw. der Ausgabebildschirm, während ein Programm läuft)
* **Unterer Bildschirm:** Retro-Tastatur (Touch) mit LCD-Statusanzeige
* **Titelbildschirm:** Beim Start schwenkt oben ein Low-Poly-Heimcomputer (Tastatur, Röhrenmonitor,
  Diskettenlaufwerk), gerendert mit der 3D-Hardware des DS. Unten listet er alle gespeicherten Programme
  (12 pro Seite, mit ← → blättern): Antippen lädt das Programm direkt in den Editor. „NEUES PROGRAMM“
  bzw. „ZUM PROGRAMM“ oder eine beliebige Taste startet BASIC ohne zu laden.
  `EXIT` (im Direktmodus oder im Programm) kehrt zum Titelbildschirm zurück, Programm und Daten bleiben erhalten

## Bauen

Benötigt nur Docker (Image `devkitpro/devkitarm`):

```bash
./build.sh          # erzeugt nds-basic.nds
./build.sh clean
```

Interpreter-Tests auf dem PC (ohne DS):

```bash
make -C host test
```

## Bedienung

| Aktion | Tastatur | Tasten |
|---|---|---|
| Programm starten / abbrechen | `RUN` / `STOP` | START |
| Hilfeseite mit allen Befehlen (↑↓ scrollen, ←→ blättern) | – | SELECT |
| Zwischen Editor und Ausgabe wechseln | `SCRN` | – |
| Zeile im Listing wählen / bearbeiten | – | ↑↓ / A |
| Cursor bewegen, löschen | `←` `→` `DEL` | ←→, B |
| Eingabezeile leeren | `CLR` | – |
| Shift (2× tippen = Lock) | `SHFT` | L / R halten |
| Sprite-Editor | – | X |
| Sound-Editor | – | Y |

Zeilen mit Nummer werden gespeichert (`10 PRINT "HI"`), eine Nummer allein löscht die Zeile.
Ohne Nummer wird der Befehl direkt ausgeführt (`PRINT 2+3`, `RUN`, `NEW`, `LIST 100`).

**Sprache:** Die Oberfläche (Titelbildschirm, Hinweise, Hilfeseite) richtet sich nach der Systemsprache des DS.
Ist der DS auf Deutsch eingestellt, erscheint sie auf Deutsch, bei jeder anderen Sprache auf Englisch.
Die klassischen BASIC-Meldungen (`READY.`, `?SYNTAX ERROR` …) und die Befehle sind immer englisch.

## Sprachumfang

`PRINT`/`?` `LET` `INPUT` `IF…THEN…ELSE` `GOTO` `GOSUB`/`RETURN` `FOR…TO…STEP`/`NEXT`
`ON…GOTO/GOSUB` `DIM` `READ`/`DATA`/`RESTORE` `REM`/`'` `END` `STOP` `CLS` `PAUSE n` (Frames)
`LOCATE zeile,spalte` `COLOR 0-15` `RANDOMIZE` – Befehle `RUN` `LIST` `NEW` `CLR` `EXIT` `SAVE` `LOAD` `DIR` `DELETE` `SPRITE` `SPROFF` `STAMP` `SPRSET` `SPRED` `SOUND` `SNDED`

Funktionen: `ABS INT SQR SIN COS TAN ATN EXP LOG RND SGN LEN VAL ASC CHR$ STR$
LEFT$ RIGHT$ MID$ INSTR INKEY$ JOY PI`, Operatoren `+ - * / ^ MOD = <> < > <= >= AND OR NOT`.

Schlüsselwörter brauchen Leerzeichen als Trenner (`FOR I=1 TO 10`).

`JOY` liefert die gerade gehaltenen DS-Tasten als Bitmaske, ideal für Spiele:
1 = hoch, 2 = runter, 4 = links, 8 = rechts, 16 = A, 32 = B, 64 = X, 128 = Y, 256 = L, 512 = R
(z. B. `IF JOY AND 1 THEN Y=Y-2`). START bleibt die Abbruchtaste.

## Grafik

Auf dem oberen Bildschirm liegt hinter dem Text eine Grafikebene mit 256×192 Punkten
(x 0–255, y 0–191). Farben sind 0–15 wie bei `COLOR`, `-1` löscht einen Punkt.
Farbe `c` und Füllung `f` sind optional (Standardfarbe über `GCOLOR`).

```
GCLS                          Grafik löschen
GCOLOR c                      Standard-Zeichenfarbe setzen
PLOT x,y[,c]                  Punkt setzen
LINE x1,y1,x2,y2[,c]          Linie
RECT x1,y1,x2,y2[,c[,f]]      Rechteck, f=1 gefüllt
CIRCLE x,y,r[,c[,f]]          Kreis, f=1 gefüllt
PAINT x,y[,c]                 zusammenhängende Fläche füllen
POINT(x,y)                    Farbe eines Punktes (-1 = leer)
```

Beispiel:

```
10 GCLS: CLS
20 FOR I=0 TO 15: CIRCLE 128,96,90-I*5,I,1: NEXT
30 LINE 0,191,255,0,2
```

Die Grafik ist auf dem Ausgabebildschirm sichtbar, im Editor wird sie ausgeblendet.

## Sprites

Es gibt 256 Sprite-Slots (0–255) mit je 8×8 Punkten in den 16 Farben oder transparent.
Angezeigt werden sie als **Hardware-Sprites** des DS: 64 Sprite-Objekte (0–63), die über
Text und Grafik liegen und sich bewegen lassen, ohne den Hintergrund zu verändern.

```
SPRITE o,n,x,y[,s]   Sprite-Objekt o mit Bild n an x,y zeigen bzw. bewegen, s = Größe 1-4
SPROFF [o]           Objekt o ausblenden, ohne Angabe alle
SPRHIT(a,b)          -1, wenn sich die sichtbaren Objekte a und b überlappen, sonst 0
STAMP n,x,y[,s]      Bild n in die Grafikebene stempeln (s = 1-8), z. B. für Hintergründe
SPRSET n,x,y,c       einzelnen Punkt (x,y 0-7) eines Bildes setzen, c=-1 = transparent
SPRED [n]            Sprite-Editor öffnen (im Editor auch Taste X)
```

Sprites sind nur auf dem Ausgabebildschirm sichtbar und dürfen teilweise außerhalb des
Bildschirms liegen. `RUN` und `NEW` blenden alle Objekte aus. Ändert man ein Bild (Editor oder
`SPRSET`), aktualisieren sich alle Objekte, die es benutzen.

Beispiel:

```
10 CLS
20 FOR X=0 TO 240 STEP 2
30 SPRITE 0,0,X,80,2: SPRITE 1,1,240-X,80,2
40 IF SPRHIT(0,1) THEN SOUND 0
50 PAUSE 1: NEXT
```

**Sprite-Editor** (Taste X oder `SPRED n`): unten ein 8×8-Raster zum Malen mit dem Stylus,
Farbpalette (Karo = transparent), Vorschau in 4×/2×/1×, Knöpfe für Slot wechseln, Kopieren,
Einfügen, Leeren und OK. Oben steht eine Übersicht aller 256 Slots; der aktuelle ist gelb
umrahmt. Steuerkreuz wechselt den Slot (←→ ±1, ↑↓ ±16), B/X/OK schließt den Editor.

`SAVE` schreibt alle benutzten Sprites mit in die `.BAS`-Datei (`#SPR n` + 64 Zeichen,
`.` = transparent, `0`–`F` = Farbe). `LOAD` ersetzt die Sprites, wenn die Datei welche enthält.

## Sounds

Es gibt 10 Sound-Slots (0–9). Jeder Slot ist eine Melodie aus bis zu 64 Noten mit
Klang (Rechteck 12,5 %, 25 %, 50 % oder Rauschen) und Tempo.

```
SOUND n        Slot n im Hintergrund abspielen, das Programm läuft weiter
SOUND n,1      Slot n abspielen und warten, bis er fertig ist
SOUND -1       Wiedergabe stoppen
SNDED [n]      Sound-Editor öffnen (im Editor auch Taste Y)
```

**Sound-Editor** (Taste Y oder `SNDED n`): unten eine Klaviatur über zwei Oktaven. Ein Tipp
auf eine Taste spielt den Ton und hängt ihn mit der gewählten Länge (1/16 bis 1/1) an die
Melodie an. Weitere Knöpfe: Pause (`REST`), letzte Note löschen (`DEL`), Slot leeren (`CLR`),
Klang, Tempo, Oktave, Slot wechseln, `PLAY`/`STOP` und `OK`. Oben stehen die Noten des
Slots (z. B. `C#4  8` = Achtelnote) und eine Übersicht aller Slots; beim Abspielen wird die
aktuelle Note markiert. Steuerkreuz: ←→ Slot, ↑↓ Oktave; B/Y/OK schließt den Editor.

`SAVE` speichert benutzte Slots mit (`#SND n klang tempo note:länge …`, Note als
MIDI-Nummer, 0 = Pause, Länge in 16teln); `LOAD` stellt sie wieder her.

## Beispiele

Im Ordner [`examples/`](examples/) liegen fertige Programme. Zum Ausprobieren die `.BAS`-Dateien
auf die SD-Karte in den Ordner `/nds-basic` kopieren; sie erscheinen dann in der Liste auf dem
Titelbildschirm (oder `LOAD "PONG"`).

| Datei | Inhalt | Steuerung |
|---|---|---|
| [PONG.BAS](examples/PONG.BAS) | Pong gegen den Computer, bis 7 Punkte. Hardware-Sprites für Schläger und Ball, Sounds für Treffer | Steuerkreuz ↑↓ oder W/S |
| [SNAKE.BAS](examples/SNAKE.BAS) | Snake mit Äpfeln, wird mit jedem Apfel länger und schneller. Körper per `STAMP` in der Grafik, Kopf und Apfel als Sprites | Steuerkreuz oder W/A/S/D |

Beide Dateien enthalten ihre Sprites (`#SPR`) und Sounds (`#SND`) gleich mit.

## Speichern

Programme werden als Textdateien auf der SD-Karte (Flashcart per DLDI oder DSi-SD) im Ordner
`/nds-basic` gespeichert:

```
SAVE "SPIEL"      speichert als /nds-basic/SPIEL.BAS (SAVE ohne Namen = letzter Name)
LOAD "SPIEL"      lädt das Programm (ersetzt das aktuelle)
DIR               listet alle .BAS-Dateien
DELETE "SPIEL"    löscht die Datei
```

Dateinamen: bis 8 Zeichen aus A–Z, 0–9, `_`, `-`. Die Dateien sind normale Textdateien
(`10 PRINT "HI"` pro Zeile) und können auch am PC bearbeitet werden. Ohne SD-Karte meldet
der Interpreter beim Start `NO SD CARD` und die Befehle liefern `?NO STORAGE ERROR`.

Im Emulator melonDS: unter *Config → Emu settings → DLDI* die SD-Emulation aktivieren
(optional mit „Sync SD to folder“).

## Website

Die Landingpage liegt in [`site/`](site/). Der Workflow
[`.github/workflows/pages.yml`](.github/workflows/pages.yml) baut bei jedem Push auf `main` die ROM im
devkitPro-Container, setzt die Seite mit `nds-basic.nds`, dieser README (als Doku-Seite) und den
Beispielen zusammen und veröffentlicht sie über GitHub Pages.

Einmalig im Repository einstellen: *Settings → Pages → Build and deployment → Source: GitHub Actions*.

Lokal ansehen:

```bash
./build.sh && sh site/build-site.sh && python3 -m http.server -d _site 8000
```
