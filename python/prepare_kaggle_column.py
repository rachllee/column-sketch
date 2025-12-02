#!/usr/bin/env python3
import argparse
import pandas as pd
import numpy as np
import json
from pathlib import Path

def main():
    p = argparse.ArgumentParser()
    p.add_argument("--csv", required=True, help="Input Kaggle CSV file")
    p.add_argument("--column", required=True, help="Categorical column name")
    p.add_argument("--out-bin", required=True, help="Output binary file (u32)")
    p.add_argument("--out-dict", required=True, help="Output JSON mapping")
    args = p.parse_args()

    df = pd.read_csv(args.csv)
    col = df[args.column].dropna().astype(str)

    # Factorize: each distinct category -> integer ID
    codes, uniques = pd.factorize(col, sort=True)
    codes = codes.astype(np.uint32)

    Path(args.out_bin).parent.mkdir(parents=True, exist_ok=True)
    codes.tofile(args.out_bin)

    # Save dictionary for interpretation later
    mapping = {int(i): cat for i, cat in enumerate(uniques)}
    with open(args.out_dict, "w") as f:
        json.dump(mapping, f, indent=2)

    print(f"wrote {len(codes)} rows to {args.out_bin}")
    print(f"distinct categories: {len(uniques)} (mapping -> {args.out_dict})")

if __name__ == "__main__":
    main()
