## Column Sketch — Reproduction Instructions

This README provides all commands required to fully reproduce the results from the project, including:  
- Building the project  
- Generating data  
- Producing 8-bit and 16-bit sketches  
- Running all benchmark experiments (uniform, beta-shaped, heavy-hitter, categorical)  
- Cleaning and resetting the workspace  

All commands assume the repository root as working directory.


### 0. Build Commands

```
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
```



### 1. How to Produce 8-bit vs 16-bit Sketches

```
python generate_data.py --n 1000000 --dtype u32 --dist uniform \
  -o data/u32.bin
```

#### 8-bit sketch (when total_codes <= 256)
```
./build/build_sketch \
  --in data/u32.bin --dtype u32 \
  --codes 256 --sample 50000 \
  --out data/u32_256

mkdir -p results
```
```
./build/benchmark \
  --base data/u32.bin \
  --sketch data/u32_256.sketch \
  --map data/u32_256.map.json \
  --dtype u32 \
  --op lt --v1 1000000000 \
  --csv results/bench_8bit_lt.csv
```

#### 16-bit sketch (when total_codes > 256)
```
./build/build_sketch \
  --in data/u32.bin --dtype u32 \
  --codes 4096 --sample 50000 \
  --out data/u32_4096
mkdir -p results

./build/benchmark \
  --base data/u32.bin \
  --sketch data/u32_4096.sketch \
  --map data/u32_4096.map.json \
  --dtype u32 \
  --op lt --v1 1000000000 \
  --csv results/bench_16bit_lt.csv
```


### 2. Uniform Baseline (1,000,000 rows)

```
python generate_data.py --n 1000000 --dtype u32 --dist uniform \
  -o data/u32_uniform.bin
```
``` 
./build/build_sketch \
  --in data/u32_uniform.bin --dtype u32 \
  --codes 256 --sample 50000 \
  --out data/u32_uniform_256
```
```
for v in 100000000 500000000 1000000000 2000000000; do
  ./build/benchmark \
    --base data/u32_uniform.bin \
    --sketch data/u32_uniform_256.sketch \
    --map data/u32_uniform_256.map.json \
    --dtype u32 \
    --op lt --v1 $v \
    --csv results/bench_shapes_lt_uniform.csv
done
```
```
./build/benchmark \
  --base data/u32_uniform.bin \
  --sketch data/u32_uniform_256.sketch \
  --map data/u32_uniform_256.map.json \
  --dtype u32 \
  --op eq --v1 0 \
  --csv results/bench_shapes_eq_uniform.csv

./build/benchmark \
  --base data/u32_uniform.bin \
  --sketch data/u32_uniform_256.sketch \
  --map data/u32_uniform_256.map.json \
  --dtype u32 \
  --op eq --v1 2000000000 \
  --csv results/bench_shapes_eq_uniform.csv
```

### 3. Beta(2,2) — Moderately Hump-Shaped Distribution

```
python generate_data.py --n 1000000 --dtype u32 --dist beta \
  --beta-a 2.0 --beta-b 2.0 \
  -o data/u32_beta_2_2.bin
```
```
./build/build_sketch \
  --in data/u32_beta_2_2.bin --dtype u32 \
  --codes 256 --sample 50000 \
  --out data/u32_beta_2_2_256
```

```
for v in 100000000 500000000 1000000000 2000000000; do
  ./build/benchmark \
    --base data/u32_beta_2_2.bin \
    --sketch data/u32_beta_2_2_256.sketch \
    --map data/u32_beta_2_2_256.map.json \
    --dtype u32 \
    --op lt --v1 $v \
    --csv results/bench_shapes_lt_beta_2_2.csv
done
```
```
./build/benchmark \
  --base data/u32_beta_2_2.bin \
  --sketch data/u32_beta_2_2_256.sketch \
  --map data/u32_beta_2_2_256.map.json \
  --dtype u32 \
  --op eq --v1 0 \
  --csv results/bench_shapes_eq_beta_2_2.csv

./build/benchmark \
  --base data/u32_beta_2_2.bin \
  --sketch data/u32_beta_2_2_256.sketch \
  --map data/u32_beta_2_2_256.map.json \
  --dtype u32 \
  --op eq --v1 2000000000 \
  --csv results/bench_shapes_eq_beta_2_2.csv
```

### 4. Beta(0.5,2) — Right-Skewed (many small values)

```
python generate_data.py --n 1000000 --dtype u32 --dist beta \
  --beta-a 0.5 --beta-b 2.0 \
  -o data/u32_beta_0.5_2.bin
```
```
./build/build_sketch \
  --in data/u32_beta_0.5_2.bin --dtype u32 \
  --codes 256 --sample 50000 \
  --out data/u32_beta_0.5_2_256
```
```
for v in 100000000 500000000 1000000000 2000000000; do
  ./build/benchmark \
    --base data/u32_beta_0.5_2.bin \
    --sketch data/u32_beta_0.5_2_256.sketch \
    --map data/u32_beta_0.5_2_256.map.json \
    --dtype u32 \
    --op lt --v1 $v \
    --csv results/bench_shapes_lt_beta_0.5_2.csv
done
```
```
./build/benchmark \
  --base data/u32_beta_0.5_2.bin \
  --sketch data/u32_beta_0.5_2_256.sketch \
  --map data/u32_beta_0.5_2_256.map.json \
  --dtype u32 \
  --op eq --v1 0 \
  --csv results/bench_shapes_eq_beta_0.5_2.csv

./build/benchmark \
  --base data/u32_beta_0.5_2.bin \
  --sketch data/u32_beta_0.5_2_256.sketch \
  --map data/u32_beta_0.5_2_256.map.json \
  --dtype u32 \
  --op eq --v1 2000000000 \
  --csv results/bench_shapes_eq_beta_0.5_2.csv
```


### 5. Beta(2,0.5) — Left-Skewed (many large values)

```
python generate_data.py --n 1000000 --dtype u32 --dist beta \
  --beta-a 2.0 --beta-b 0.5 \
  -o data/u32_beta_2_0.5.bin
```
```
./build/build_sketch \
  --in data/u32_beta_2_0.5.bin --dtype u32 \
  --codes 256 --sample 50000 \
  --out data/u32_beta_2_0.5_256
```
```
for v in 100000000 500000000 1000000000 2000000000; do
  ./build/benchmark \
    --base data/u32_beta_2_0.5.bin \
    --sketch data/u32_beta_2_0.5_256.sketch \
    --map data/u32_beta_2_0.5_256.map.json \
    --dtype u32 \
    --op lt --v1 $v \
    --csv results/bench_shapes_lt_beta_2_0.5.csv
done
```
```
./build/benchmark \
  --base data/u32_beta_2_0.5.bin \
  --sketch data/u32_beta_2_0.5_256.sketch \
  --map data/u32_beta_2_0.5_256.map.json \
  --dtype u32 \
  --op eq --v1 0 \
  --csv results/bench_shapes_eq_beta_2_0.5.csv

./build/benchmark \
  --base data/u32_beta_2_0.5.bin \
  --sketch data/u32_beta_2_0.5_256.sketch \
  --map data/u32_beta_2_0.5_256.map.json \
  --dtype u32 \
  --op eq --v1 2000000000 \
  --csv results/bench_shapes_eq_beta_2_0.5.csv
```

### 6. Heavy Hitter Construction
```
python generate_data.py --n 700000 --dtype u32 --dist uniform \
  -o data/noise.bin
```
```
python3 - << 'EOF'
import numpy as np
arr = np.full(300000, 42, dtype=np.uint32)
arr.tofile("data/hh_42.bin")
print("wrote 300000 heavy-hitter values")
EOF

cat data/hh_42.bin data/noise.bin > data/u32_heavyhitter.bin
```
```
./build/build_sketch \
  --in data/u32_heavyhitter.bin --dtype u32 \
  --codes 256 --sample 50000 \
  --unique-cutoff 100000 \
  --out data/u32_heavyhitter_256

mkdir -p results
```
```
./build/benchmark \
  --base data/u32_heavyhitter.bin \
  --sketch data/u32_heavyhitter_256.sketch \
  --map data/u32_heavyhitter_256.map.json \
  --dtype u32 \
  --op eq --v1 42 \
  --csv results/bench_heavyhitter_eq.csv

./build/benchmark \
  --base data/u32_heavyhitter.bin \
  --sketch data/u32_heavyhitter_256.sketch \
  --map data/u32_heavyhitter_256.map.json \
  --dtype u32 \
  --op eq --v1 123456789 \
  --csv results/bench_heavyhitter_eq.csv
```


### 7. Real-World Categorical Data (Spotify Artist Names)

```
python python/prepare_kaggle_column.py \
  --csv data/spotify_data.csv \
  --column artist_name \
  --out-bin data/kaggle_name.bin \
  --out-dict data/kaggle_name_dict.json
```
```
./build/build_sketch \
  --in data/kaggle_name.bin \
  --dtype u32 \
  --codes 256 --sample 50000 \
  --unique-cutoff 100 \
  --out data/kaggle_name_256
```

##### Unique-coded artist ID
```
./build/benchmark \
  --base data/kaggle_name.bin \
  --sketch data/kaggle_name_256.sketch \
  --map data/kaggle_name_256.map.json \
  --dtype u32 \
  --op eq --v1 2121 \
  --csv results/bench_kaggle_artist_eq.csv
```

##### Rare artist ID
```
./build/benchmark \
  --base data/kaggle_name.bin \
  --sketch data/kaggle_name_256.sketch \
  --map data/kaggle_name_256.map.json \
  --dtype u32 \
  --op eq --v1 2122 \
  --csv results/bench_kaggle_artist_eq.csv
```


#### 8. Cleaning 
(when replicating, it is probably easier to pull from main as to not lose the dataset)

```
rm -rf data
rm -rf results
mkdir data results
```


