#!/usr/bin/env python3
"""
Small utility to generate a fixed-width numeric column.
Writes raw little-endian uint32 or uint64 values.

Usage:
  python generate_data.py --n 1000000 --dtype u32 --dist uniform -o data.bin
"""

import argparse
import numpy as np


def make_dist(n: int, dist: str, beta_a: float, beta_b: float):
    rng = np.random.default_rng()

    if dist == "uniform":
        # Uniform in [0,1)
        return rng.random(n)
    elif dist == "normal":
        # Standard normal, then map to [0,1] via CDF-like transform
        x = rng.standard_normal(n)
        # Use CDF of N(0,1): 0.5 * (1 + erf(x / sqrt(2)))
        from math import erf, sqrt
        cdf = np.vectorize(lambda v: 0.5 * (1.0 + erf(v / sqrt(2.0))))
        return cdf(x)
    elif dist == "beta":
        # Parametric Beta(a,b) in [0,1]
        return rng.beta(beta_a, beta_b, size=n)
    else:
        raise ValueError(f"unknown dist: {dist}")


def main():
    p = argparse.ArgumentParser()
    p.add_argument("--n", type=int, required=True)
    p.add_argument("--dtype", choices=["u32", "u64"], required=True)
    p.add_argument("--dist", choices=["uniform", "normal", "beta"], default="uniform")
    p.add_argument("--beta-a", type=float, default=1.0,
                  help="alpha parameter for beta distribution (default 1.0)")
    p.add_argument("--beta-b", type=float, default=5.0,
                  help="beta parameter for beta distribution (default 5.0)")
    p.add_argument("-o", "--out", required=True)
    args = p.parse_args()

    x = make_dist(args.n, args.dist, args.beta_a, args.beta_b)

    if args.dtype == "u32":
        # scale to full u32 range
        arr = (x * (2**32 - 1)).astype(np.uint32)
    else:
        arr = (x * (2**64 - 1)).astype(np.uint64)

    arr.tofile(args.out)
    print(f"wrote {args.n} elements to {args.out}, dist={args.dist}, "
          f"beta_a={args.beta_a}, beta_b={args.beta_b}")


if __name__ == "__main__":
    main()
