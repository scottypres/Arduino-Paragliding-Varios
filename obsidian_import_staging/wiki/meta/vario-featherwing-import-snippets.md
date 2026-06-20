---
type: meta
title: "Vario FeatherWing Import Snippets"
created: 2026-06-20
updated: 2026-06-20
tags:
  - obsidian
  - import
status: staging
related:
  - "[[Vario Adafruit FeatherWing]]"
  - "[[Feather Vario Buzzer PCB]]"
  - "[[Vario FeatherWing PCB Layout Notes]]"
---

# Vario FeatherWing Import Snippets

These snippets are staged because the current Codex sandbox cannot write directly to `/Users/scottpresbrey/Documents/GitHub/obsidian-vault`.

## Add near the top of `wiki/index.md` Sources

```markdown
- [[vario-adafruit-featherwing-repo-2026-06-20|Vario Adafruit FeatherWing Repo Snapshot]] - 2026-06-20 | local hardware repo for Feather vario CAD, Adafruit EagleCAD imports, and custom KiCad buzzer/power PCB | 4 pages staged
```

## Add near the top of `wiki/index.md` Entities

```markdown
- [[Vario Adafruit FeatherWing]] - local hardware repo for Feather vario CAD, KiCad, and custom buzzer board integration (status: active)
- [[Feather Vario Buzzer PCB]] - custom power-switch and passive-buzzer driver board for the Feather vario build (status: active)
```

## Add near the top of `wiki/index.md` Concepts

```markdown
- [[Hand-Drilled KiCad PCB Constraints]] - drill-size, trace-width, and formed-lead footprint practices for hand-fabricated KiCad boards (status: developing)
```

## Add near the top of `wiki/index.md` Questions

```markdown
- [[Vario FeatherWing PCB Layout Notes]] - saved PCB layout decisions: drill sizes, D13 location, boost footprint, Q1 formed-lead footprint, SW1 meaning, capacitor labels, and trace-width guidance (status: developing)
```

## Add at the top of `wiki/log.md`

```markdown
## [2026-06-20] ingest | Vario Adafruit FeatherWing Repo Snapshot
- Type: source
- Location: wiki/sources/vario-adafruit-featherwing-repo-2026-06-20.md
- From: local repo `/Users/scottpresbrey/Documents/GitHub/Vario_Adafruit_Featherwing`. Captures Adafruit CAD/schematic retrieval, custom KiCad buzzer PCB state, hand-drill constraints, D13 fixed location, boost footprint, Q1 formed-lead footprint, and routing guidance. Pages staged: [[vario-adafruit-featherwing-repo-2026-06-20]], [[Vario Adafruit FeatherWing]], [[Feather Vario Buzzer PCB]], [[Hand-Drilled KiCad PCB Constraints]], [[Vario FeatherWing PCB Layout Notes]].
```

## Replacement `wiki/hot.md` Recent Context Entry

```markdown
## Last Updated
2026-06-20: staged ingest for [[Vario Adafruit FeatherWing]] and saved [[Vario FeatherWing PCB Layout Notes]]. The custom [[Feather Vario Buzzer PCB]] is a battery switch and passive-buzzer driver board under the Feather stack. Key locked details: D13/J4 restored to `(85.87, 92.84)`; allowed drill export uses `0.8`, `1.0`, `1.2`, `3.0 mm`; trace presets are `1`, `2`, `3 mm` and `1 mm` traces are acceptable everywhere; the boost converter footprint is `11 x 11 mm` with `1.2 mm` holes at `2.54 mm` pitch and hole centers `1.35 mm` from the edge; Q1 uses a widened formed-lead TO-92 footprint at `2.54 mm` adjacent pad pitch, requiring the S8050 outer legs to be bent outward. `SW1` is the inline positive battery switch. `C2` is the small `BOOST_5V` to `GND` decoupling capacitor. KiCad CLI DRC crashed with exit `134`, so drill/position exports and file inspection were used instead.
```

## Manifest Fragment

```json
{
  "sources": {
    ".raw/repos/vario-adafruit-featherwing-2026-06-20.md": {
      "hash": "3df1f6da502c700dd865b13b261963f3",
      "ingested_at": "2026-06-20",
      "pages_created": [
        "wiki/sources/vario-adafruit-featherwing-repo-2026-06-20.md",
        "wiki/entities/Vario Adafruit FeatherWing.md",
        "wiki/entities/Feather Vario Buzzer PCB.md",
        "wiki/concepts/Hand-Drilled KiCad PCB Constraints.md",
        "wiki/questions/Vario FeatherWing PCB Layout Notes.md"
      ],
      "pages_updated": [
        "wiki/index.md",
        "wiki/log.md",
        "wiki/hot.md"
      ]
    }
  }
}
```
