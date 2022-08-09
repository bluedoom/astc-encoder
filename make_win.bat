
mkdir build
pushd build
cmake -G "Visual Studio 17"  -T ClangCL -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=..\  -DISA_AVX2=ON -DISA_SSE41=ON -DISA_SSE2=ON ..