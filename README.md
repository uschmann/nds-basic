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
| Zwischen Editor und Ausgabe wechseln | `SCRN` | SELECT |
| Zeile im Listing wählen / bearbeiten | – | ↑↓ / A |
| Cursor bewegen, löschen | `←` `→` `DEL` | ←→, B |
| Eingabezeile leeren | `CLR` | Y |
| Shift (2× tippen = Lock) | `SHFT` | L / R halten |

Zeilen mit Nummer werden gespeichert (`10 PRINT "HI"`), eine Nummer allein löscht die Zeile.
Ohne Nummer wird der Befehl direkt ausgeführt (`PRINT 2+3`, `RUN`, `NEW`, `LIST 100`).

## Sprachumfang

`PRINT`/`?` `LET` `INPUT` `IF…THEN…ELSE` `GOTO` `GOSUB`/`RETURN` `FOR…TO…STEP`/`NEXT`
`ON…GOTO/GOSUB` `DIM` `READ`/`DATA`/`RESTORE` `REM`/`'` `END` `STOP` `CLS` `PAUSE n` (Frames)
`LOCATE zeile,spalte` `COLOR 0-15` `RANDOMIZE` – Befehle `RUN` `LIST` `NEW` `CLR`

Funktionen: `ABS INT SQR SIN COS TAN ATN EXP LOG RND SGN LEN VAL ASC CHR$ STR$
LEFT$ RIGHT$ MID$ INSTR INKEY$ PI`, Operatoren `+ - * / ^ MOD = <> < > <= >= AND OR NOT`.

Schlüsselwörter brauchen Leerzeichen als Trenner (`FOR I=1 TO 10`). Speichern/Laden gibt es noch nicht.
