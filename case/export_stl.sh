#!/usr/bin/env bash
cd "$(dirname "$0")"
OPENSCAD=/Applications/OpenSCAD-2021.01.app/Contents/MacOS/OpenSCAD

for p in box lid foot; do
  echo "Exporting $p..."
  "$OPENSCAD" -o "tally3_$p.stl" -D "part=\"$p\"" tally_box_v3.scad
done
