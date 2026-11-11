# Gehäuse

Zwei Gehäuse für den Heltec Vision Master E290 zum 3D-Drucken. Beide
nutzen denselben Mittelrahmen und dieselben Tasten.

| Ordner | Gehäuse |
|---|---|
| [`slim/`](slim/) | **Slim** (neu): flach, Display 50° geneigt, Deckel mit L-Fuß, vertiefte Tastenbeschriftung „1 · 2 · R“ |
| [`klassisch/`](klassisch/) | **Klassisch** (bisheriges Gehäuse): größerer, geneigter Deckel |

Gemeinsame Teile (in diesem Ordner, für beide Gehäuse):

| Datei | Inhalt |
|---|---|
| `e290-middle-frame.stl` | Mittelrahmen, unverändert vom Original |
| `e290-buttons.stl` | Tasten (drei gleich lange Stößel), unverändert vom Original |

## Slim

| Datei | Inhalt |
|---|---|
| `slim/Case_Slim.stl` | Vorderteil, liegt in Druckausrichtung (Front nach unten) |
| `slim/Lid_Slim_50Grad.stl` | Deckel mit L-Fuß, liegt in Druckausrichtung (stehend) |
| `slim/Gehaeuse_Slim_Komplett.3mf` | Bambu-Studio-Projekt mit allen Teilen, eingestellt für den Bambu Lab A1 |

- Außenmaße ca. 94 × 50 mm (Front), 43 mm tief. Das Display steht 50° zur
  Tischfläche.
- Der Rand über dem Display ist nur im Bereich der Tasten hoch, rechts
  davon fällt er mit einem Schwung bis knapp über die Platine ab.
- Unter den Tasten ist „1 · 2 · R“ vertieft eingeprägt (linke Taste,
  mittlere Taste, Reset).
- Der Deckel steht auf einem flachen Fuß über die volle Breite. Von der
  Seite sieht er aus wie ein schräges L. Die unteren Ecken sind rund wie
  beim Vorderteil.
- Case und Deckel werden ohne Schrauben zusammengesteckt und rasten ein.

Drucken:
- Case mit der Front nach unten, Deckel stehend auf dem Fuß. Beide ohne
  Stützen und ohne Brim.
- Das 3MF-Projekt hat zwei Platten: Platte 1 Case und Deckel, Platte 2
  Mittelrahmen und Tasten. Eingestellt sind Bambu Lab A1 mit 0,4-mm-Düse,
  Profil „0.12mm High Quality @BBL A1“, PLA.

## Klassisch

| Datei | Inhalt |
|---|---|
| `klassisch/Case_45Grad_V1.stl` | Gehäuse, geändert (siehe unten) |
| `klassisch/Lid_45Grad_V1.stl` | Deckel, geändert (siehe unten) |
| `klassisch/Complete_with_Supports.3mf` | Bambu-Studio-Projekt mit allen Teilen und Stützen, eingestellt für den Bambu Lab A1 |

Trotz des Dateinamens steht das Display etwa 55° zur Tischfläche.

## Herkunft und Lizenz

Beide Gehäuse sind Remixe von **„Vision Master E290 V0.3.1 case for
Meshtastic“** von **HarukiToreda**:
[printables.com/model/974647](https://www.printables.com/model/974647),
lizenziert unter [Creative Commons Namensnennung 4.0 International (CC BY 4.0)](https://creativecommons.org/licenses/by/4.0/).

Änderungen gegenüber dem Original, **Klassisch**:
- Antennenanschluss entfernt
- Gehäuse steht geneigt auf dem Tisch
- Schrift- und Logovertiefungen entfernt

Änderungen gegenüber dem Original, **Slim**:
- Außenform neu gestaltet: flache Front, abgesenkter Rand rechts der
  Tasten, Deckel mit L-Fuß, Neigung 50°
- Tastenbeschriftung „1 · 2 · R“ ergänzt
- Antennenanschluss, Schrift- und Logovertiefungen entfernt
- Innenmaße, Rastverbindung, Mittelrahmen und Tasten vom Original
  übernommen

Diese Remixe stehen ebenfalls unter
[CC BY 4.0](https://creativecommons.org/licenses/by/4.0/). Bei
Weitergabe bitte den ursprünglichen Autor und dieses Projekt nennen.

Die GPL-Lizenz des Programmcodes (siehe `LICENSE` im Hauptordner) gilt
**nicht** für die Dateien in diesem Ordner.
