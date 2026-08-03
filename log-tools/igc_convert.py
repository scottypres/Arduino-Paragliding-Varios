#!/usr/bin/env python3
"""Convert an IGC flight log to GPX (for Insta360 Studio).

  ./igc_convert.py flight.IGC --gpx            -> flight.gpx     (baro altitude)
  ./igc_convert.py flight.IGC --gpx --gps-alt  -> flight.gpx     (GPS altitude)
  ./igc_convert.py --selfcheck

Gaggle imports FlySkyHy's IGC directly -- no conversion needed for that path.

Baro altitude is the point of the GPX path: Insta360 Studio only ever has GPS
altitude, and a FlySkyHy log flown with a BlueFlyVario has real pressure altitude.
"""
import sys, re, datetime, xml.sax.saxutils as xe

# B HHMMSS DDMMmmmN DDDMMmmmE A PPPPP GGGGG [extensions]
B = re.compile(r"^B(\d{2})(\d{2})(\d{2})"
               r"(\d{2})(\d{5})([NS])(\d{3})(\d{5})([EW])([AV])(-\d{4}|\d{5})(-\d{4}|\d{5})")


def parse(path):
    """-> (date, [(datetime, lat, lon, baro_m, gps_m), ...], raw_header_lines)"""
    date, fixes, header = None, [], []
    prev = None
    for raw in open(path, "r", errors="replace"):
        line = raw.rstrip("\r\n")
        if line.startswith("HFDTE"):
            d = re.search(r"(\d{6})", line).group(1)
            date = datetime.date(2000 + int(d[4:6]), int(d[2:4]), int(d[0:2]))
        if line[:1] in "AH":
            header.append(line)
        m = B.match(line)
        if not m:
            continue
        hh, mm, ss, latd, latm, ns, lond, lonm, ew, _fix, press, gnss = m.groups()
        t = datetime.time(int(hh), int(mm), int(ss))
        # ponytail: naive midnight rollover; fine for one flight, not for multi-day files
        if prev and t < prev:
            date += datetime.timedelta(days=1)
        prev = t
        lat = (int(latd) + int(latm) / 60000.0) * (-1 if ns == "S" else 1)
        lon = (int(lond) + int(lonm) / 60000.0) * (-1 if ew == "W" else 1)
        fixes.append((datetime.datetime.combine(date, t), lat, lon, int(press), int(gnss)))
    if not fixes:
        sys.exit(f"{path}: no B records found")
    return date, fixes, header


def to_gpx(fixes, header, gps_alt=False):
    name = next((h.split(":", 1)[1] for h in header if h.startswith("HFPLT") and ":" in h), "") or "IGC flight"
    out = ['<?xml version="1.0" encoding="UTF-8"?>',
           '<gpx version="1.1" creator="igc_convert.py" xmlns="http://www.topografix.com/GPX/1/1">',
           f'  <trk><name>{xe.escape(name.strip())}</name><trkseg>']
    for t, lat, lon, press, gnss in fixes:
        ele = gnss if gps_alt else (press or gnss)  # ponytail: fall back if baro column is all zeros
        out.append(f'    <trkpt lat="{lat:.6f}" lon="{lon:.6f}"><ele>{ele}</ele>'
                   f'<time>{t.strftime("%Y-%m-%dT%H:%M:%SZ")}</time></trkpt>')
    out += ['  </trkseg></trk>', '</gpx>', '']
    return "\n".join(out)


SAMPLE = """AXFH000
HFDTE020826
HFPLTPILOT:Test Pilot
I053638FXA3941VXA4244GSP4547CCO4850HDT
E102604PEVBackground
B1026044642840N00749431EA0158001581004003000071099
B1026054642840S00749431WA0158101582004003000071108
G0123456789
"""


def selfcheck():
    import tempfile, os
    p = os.path.join(tempfile.mkdtemp(), "s.igc")
    open(p, "w").write(SAMPLE)
    date, fixes, header = parse(p)
    assert date == datetime.date(2026, 8, 2), date
    assert len(fixes) == 2, fixes
    t, lat, lon, press, gnss = fixes[0]
    assert t == datetime.datetime(2026, 8, 2, 10, 26, 4), t
    assert abs(lat - 46.714000) < 1e-6, lat          # 4642840 -> 46 + 42840/60000
    assert abs(lon - 7.823850) < 1e-6, lon           # 00749431 -> 7 + 49431/60000
    assert (press, gnss) == (1580, 1581), (press, gnss)
    assert fixes[1][1] < 0 and fixes[1][2] < 0, "S/W hemisphere must go negative"

    gpx = to_gpx(fixes, header)
    assert '<ele>1580</ele>' in gpx and '2026-08-02T10:26:04Z' in gpx, gpx
    assert '<ele>1581</ele>' in to_gpx(fixes, header, gps_alt=True)

    print("selfcheck ok")


if __name__ == "__main__":
    a = sys.argv[1:]
    if "--selfcheck" in a:
        selfcheck(); sys.exit()
    if len(a) < 2:
        sys.exit(__doc__)
    src = a[0]
    _, fixes, header = parse(src)
    dst = re.sub(r"\.igc$", "", src, flags=re.I) + ".gpx"
    open(dst, "w").write(to_gpx(fixes, header, "--gps-alt" in a))
    print(dst)
