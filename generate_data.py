#!/usr/bin/env python3
"""
Small utility to generate a fixed-width numeric column.
Writes raw little-endian uint32 or uint64 values.

Usage:
  python generate_data.py --n 1000000 --dtype u32 --dist uniform -o data.bin
"""

import argparse
import numpy as np


def make_dist(n: int, dist: str):
    if dist == "uniform":
        return np.random.rand(n)
    elif dist == "normal":
        return np.random.randn(n)
    elif dist == "beta":
        # moderately skewed beta(1,5)
        return np.random.beta(1, 5, size=n)
    else:
        raise ValueError(f"unknown dist: {dist}")


def main():
    p = argparse.ArgumentParser()
    p.add_argument("--n", type=int, required=True)
    p.add_argument("--dtype", choices=["u32", "u64"], required=True)
    p.add_argument("--dist", choices=["uniform", "normal", "beta"], default="uniform")
    p.add_argument("-o", "--out", required=True)
    args = p.parse_args()

    x = make_dist(args.n, args.dist)

    if args.dtype == "u32":
        # scale to full u32 range
        arr = (x * (2**32 - 1)).astype(np.uint32)
    else:
        arr = (x * (2**64 - 1)).astype(np.uint64)

    arr.tofile(args.out)
    print(f"wrote {args.n} elements to {args.out}")


if __name__ == "__main__":
    main()
