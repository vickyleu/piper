#!/bin/bash
set -e

rm -rf libpiper.a libpiper_relocatable.a *.o libucd.a script.mri

echo "创建ar脚本..."
[ ! -f libpiper_phonemize.a ] && { echo "Error: libpiper_phonemize.a missing!"; exit 1; }

cat > script.mri << EOF
create libpiper.a
addlib libpiper_phonemize.a
EOF

# 修改为兼容 sh 的语法
for lib in *.a; do
    case "$lib" in
        libpiper.a|libpiper_relocatable.a)
            continue
            ;;
    esac
    if [ "$lib" != "libpiper_phonemize.a" ]; then
        echo "addlib $lib" >> script.mri
    fi
done

cat >> script.mri << EOF
save
end
EOF

echo "执行库合并..."
arm-linux-gnueabihf-ar -M < script.mri

echo "生成可重定位对象文件..."
arm-linux-gnueabihf-ld -z noexecstack -r  --whole-archive -o piper_full.o libpiper.a
echo "重建最终静态库..."
arm-linux-gnueabihf-ar crs libpiper_relocatable.a piper_full.o
mv -f libpiper_relocatable.a libpiper.a

echo "验证符号..."
if ! arm-linux-gnueabihf-nm -C libpiper.a | grep -q 'BuildKernelCreateInfo.*BitwiseOr.*int32_t'; then
    echo "Error: 关键符号缺失！"
    exit 1
fi

rm -f script.mri piper_full.o
echo "合并完成：libpiper.a"