#Useful commands:
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j

mkdir -p data
python generate_data.py --n 1000000 --dtype u32 --dist uniform -o data/u32.bin

./build/build_sketch --in data/u32.bin --dtype u32 --codes 256 --sample 200000 --out data/u32_uniform
