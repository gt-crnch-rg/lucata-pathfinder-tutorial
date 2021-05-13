#!/usr/bin/env python3

import pandas as pd
import os
import sys
import json
import glob

if len(sys.argv) != 3:
    print("Usage: %s <glob> <output.csv>")
    sys.exit(1)

paths = sys.argv[1]
output = sys.argv[2]

rows = []

for path in glob.glob(paths):
    print(path, ":", end=' ')
    num_records = 0
    with open(path) as f:
        for line in f.readlines():
            try:
                rows.append(json.loads(line))
                num_records += 1
            except ValueError:
                pass
    print(num_records, "records")

if len(rows) == 0:
    print("No records found")
else:
    data = pd.DataFrame.from_records(rows)
    cols = [c for c in data.columns if c != "time_ms"]
    data[cols] = data[cols].fillna(method="ffill")
    data = data.dropna(subset=["time_ms"])
    data.to_csv(output)
    print("Saved to " + output)
