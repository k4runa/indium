#!/bin/bash

# 1. Eski build kalıntılarını temizle
if [ -d "build" ]; then
    echo "--- Eski build klasörü siliniyor... ---"
    rm -rf build
fi

# 2. Yeni build klasörünü oluştur ve içine gir
mkdir build && cd build

# 3. CMake yapılandırmasını çalıştır
echo "--- CMake yapılandırılıyor... ---"
cmake ..

# 4. Derle (İşlemci çekirdek sayısına göre hızlandırılmış)
echo "--- Derleniyor... ---"
make -j$(nproc)

# 5. Başarılıysa çalıştır
if [ $? -eq 0 ]; then
    echo "--- Çalıştırılıyor... ---"
    ./Indium
else
    echo "--- Hata: Derleme başarısız! ---"
    exit 1
fi
