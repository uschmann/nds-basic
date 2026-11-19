# NDS BASIC

Ein kleiner, retro-inspirierter BASIC-Interpreter für den Nintendo DS.

* **Oberer Bildschirm:** Programm-Listing mit Zeilennummern, Statuszeile und Eingabezeile
  (bzw. der Ausgabebildschirm, während ein Programm läuft)
* **Unterer Bildschirm:** Retro-Tastatur (Touch) mit LCD-Statusanzeige

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
| Eingabezeile leeren | `CLR` | Y |
| Shift (2× tippen = Lock) | `SHFT` | L / R halten |

Zeilen mit Nummer werden gespeichert (`10 PRINT "HI"`), eine Nummer allein löscht die Zeile.
Ohne Nummer wird der Befehl direkt ausgeführt (`PRINT 2+3`, `RUN`, `NEW`, `LIST 100`).

## Sprachumfang

`PRINT`/`?` `LET` `INPUT` `IF…THEN…ELSE` `GOTO` `GOSUB`/`RETURN` `FOR…TO…STEP`/`NEXT`
`ON…GOTO/GOSUB` `DIM` `READ`/`DATA`/`RESTORE` `REM`/`'` `END` `STOP` `CLS` `PAUSE n` (Frames)
`LOCATE zeile,spalte` `COLOR 0-15` `RANDOMIZE` – Befehle `RUN` `LIST` `NEW` `CLR` `SAVE` `LOAD` `DIR` `DELETE`

Funktionen: `ABS INT SQR SIN COS TAN ATN EXP LOG RND SGN LEN VAL ASC CHR$ STR$
LEFT$ RIGHT$ MID$ INSTR INKEY$ PI`, Operatoren `+ - * / ^ MOD = <> < > <= >= AND OR NOT`.

Schlüsselwörter brauchen Leerzeichen als Trenner (`FOR I=1 TO 10`).

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
