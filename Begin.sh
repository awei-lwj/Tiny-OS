echo "Install the compilation tools and dependency libraries"
sudo apt install make                -y
sudo apt-get install build-essential -y
sudo apt-get install xorg-dev        -y
sudo apt-get install libgtk2.0-dev   -y
sudo apt-get install bison           -y
sudo apt-get install gcc             -y
sudo apt-get install nasm            -y


echo "Unpack the bochs-2.6.11.tar.gz"
cd tool
tar vxaf bochs-2.6.11.tar.gz

echo "cp bochsrc,disk.img and disk80.img to bochs-2.6.11"
cp bochsrc    bochs-2.6.11
cp disk80.img bochs-2.6.11

echo "Install the bochs"
cd bochs-2.6.11
./configure --enable-debugger --enable-disasm
make
sudo make install

bximage -mode=create -hd=60 -imgmode="flat" -q disk.img
cd ..
cd ..


echo "Now begin to compile our tiny-OS"


echo "                                                            "
echo "完成MBR磁盘主引导程序的编译"
echo "nasm -I boot/include/ -o bin/mbr.bin boot/mbr.S"
nasm -I boot/include/ -o bin/mbr.bin boot/mbr.S
echo "dd if=bin/mbr.bin of=tool/bochs-2.6.11/disk.img bs=512 count=1 conv=notrunc"
dd if=bin/mbr.bin of=tool/bochs-2.6.11/disk.img bs=512 count=1 conv=notrunc



echo "                                                 "
echo "完成内存加载器的编译 "
echo "nasm -I boot/include/ -o bin/loader.bin boot/loader.S"
nasm -I boot/include/ -o bin/loader.bin boot/loader.S
echo "dd if=bin/loader.bin of=tool/bochs-2.6.11/disk.img bs=512 count=4 seek=2 conv=notrunc"
dd if=bin/loader.bin of=tool/bochs-2.6.11/disk.img bs=512 count=4 seek=2 conv=notrunc



echo "                                            "
echo "完成printf的编译，用于测试 "
echo "nasm -f elf -o build/print.o lib/kernel/print.S"
nasm -f elf -o build/print.o lib/kernel/print.S



echo "                                                            "
echo "完成中断部分的程序的编译 "
echo "nasm -f elf -o build/kernel.o kernel/kernel.S"
echo "gcc -m32 -I lib/kernel/ -I lib/ -I kernel/ -c -fno-builtin -o build/interrupt.o kernel/interrupt.c -fno-stack-protector
gcc -m32 -I lib/kernel/ -I lib/ -I kernel/ -c -fno-builtin -o build/init.o kernel/init.c -fno-stack-protector
gcc -m32 -I lib/kernel/ -I lib/ -I kernel/ -c -fno-builtin -o build/timer.o device/timer.c -fno-stack-protector"

nasm -f elf -o build/kernel.o kernel/kernel.S 
gcc -m32 -I lib/kernel/ -I lib/ -I kernel/ -c -fno-builtin -o build/interrupt.o kernel/interrupt.c -fno-stack-protector
gcc -m32 -I lib/kernel/ -I lib/ -I kernel/ -c -fno-builtin -o build/init.o kernel/init.c -fno-stack-protector
gcc -m32 -I lib/kernel/ -I lib/ -I kernel/ -c -fno-builtin -o build/timer.o device/timer.c -fno-stack-protector
gcc -m32 -I lib/kernel/ -I lib/ -I kernel/ -c -fno-builtin -o build/debug.o kernel/debug.c -fno-stack-protector



echo "                              "
echo "开始管理内存系统部分编译"
echo "gcc -m32 -I lib/kernel/ -I lib/ -I kernel/ -c -fno-builtin -o build/memory.o kernel/memory.c -fno-stack-protector
gcc -m32 -I lib/kernel/ -I lib/ -I kernel/ -c -fno-builtin -o build/bitmap.o lib/kernel/bitmap.c -fno-stack-protector"

gcc -m32 -I lib/kernel/ -I lib/ -I kernel/ -c -fno-builtin -o build/string.o lib/string.c -fno-stack-protector
gcc -m32 -I lib/kernel/ -I lib/ -I kernel/ -c -fno-builtin -o build/memory.o kernel/memory.c -fno-stack-protector
gcc -m32 -I lib/kernel/ -I lib/ -I kernel/ -c -fno-builtin -o build/bitmap.o lib/kernel/bitmap.c -fno-stack-protector



echo "                                     "
echo "开始线程部分的编译"
echo "gcc -m32 -I lib/kernel/ -I lib/ -I kernel/ -c -fno-builtin -o build/thread.o thread/thread.c -fno-stack-protector
gcc -m32 -I lib/kernel/ -I lib/ -I kernel/ -c -fno-builtin -o build/list.o lib/kernel/list.c -fno-stack-protector
nasm -f elf -o build/switch.o thread/switch.S"

gcc -m32 -I lib/kernel/ -I lib/ -I kernel/ -c -fno-builtin -o build/thread.o thread/thread.c -fno-stack-protector
gcc -m32 -I lib/kernel/ -I lib/ -I kernel/ -c -fno-builtin -o build/list.o lib/kernel/list.c -fno-stack-protector
nasm -f elf -o build/switch.o thread/switch.S



echo "              "
echo "开始锁的实现"
echo "gcc -m32 -I lib/kernel/ -I lib/ -I kernel/ -c -fno-builtin -o build/sync.o thread/sync.c -fno-stack-protector
gcc -m32 -I lib/kernel/ -I lib/ -I kernel/ -c -fno-builtin -o build/console.o device/console.c -fno-stack-protector"

gcc -m32 -I lib/kernel/ -I lib/ -I kernel/ -c -fno-builtin -o build/sync.o thread/sync.c -fno-stack-protector
gcc -m32 -I lib/kernel/ -I lib/ -I kernel/ -c -fno-builtin -o build/console.o device/console.c -fno-stack-protector



echo "           "
echo "实现键盘驱动"
echo "gcc -m32 -I lib/kernel/ -I lib/ -I kernel/ -c -fno-builtin -o build/keyboard.o device/keyboard.c -fno-stack-protector
gcc -m32 -I lib/kernel/ -I lib/ -I kernel/ -c -fno-builtin -o build/ioqueue.o device/ioqueue.c -fno-stack-protector"

gcc -m32 -I lib/kernel/ -I lib/ -I kernel/ -c -fno-builtin -o build/keyboard.o device/keyboard.c -fno-stack-protector
gcc -m32 -I lib/kernel/ -I lib/ -I kernel/ -c -fno-builtin -o build/ioqueue.o device/ioqueue.c -fno-stack-protector



echo "           "
echo "用户进程的实现"
echo "gcc -m32 -I lib/kernel/ -I lib/ -I kernel/ -c -fno-builtin -o build/tss.o userprogress/tss.c -fno-stack-protector
gcc -m32 -I lib/kernel/ -I lib/ -I kernel/ -c -fno-builtin -o build/process.o userprog/process.c -fno-stack-protector"
gcc -m32 -I lib/kernel/ -I lib/ -I kernel/ -c -fno-builtin -o build/tss.o userprog/tss.c -fno-stack-protector
gcc -m32 -I lib/kernel/ -I lib/ -I kernel/ -c -fno-builtin -o build/process.o userprog/process.c -fno-stack-protector



echo "          "
echo "系统调用的实现"
echo "gcc -m32 -I lib/kernel/ -I lib/ -I kernel/ -c -fno-builtin -o build/syscall_init.o userprog/syscall_init.c -fno-stack-protector
gcc -m32 -I lib/kernel/ -I lib/ -I kernel/ -c -fno-builtin -o build/syscall.o lib/user/syscall.c -fno-stack-protector
gcc -m32 -I lib/kernel/ -I lib/ -I kernel/ -c -fno-builtin -o build/stdio.o lib/stdio.c -fno-stack-protector"

gcc -m32 -I lib/kernel/ -I lib/ -I kernel/ -c -fno-builtin -o build/syscall-init.o userprog/syscall-init.c -fno-stack-protector
gcc -m32 -I lib/kernel/ -I lib/ -I kernel/ -c -fno-builtin -o build/syscall.o lib/user/syscall.c -fno-stack-protector
gcc -m32 -I lib/kernel/ -I lib/ -I kernel/ -c -fno-builtin -o build/stdio.o lib/stdio.c -fno-stack-protector



echo "          "
echo "硬盘驱动"
echo "gcc -m32 -I lib/kernel/ -I lib/ -I kernel/ -c -fno-builtin -o build/ide.o device/ide.c -fno-stack-protector
gcc -m32 -I lib/kernel/ -I lib/ -I kernel/ -c -fno-builtin -o build/stdio-kernel.o lib/kernel/stdio-kernel.c -fno-stack-protector"
gcc -m32 -I lib/kernel/ -I lib/ -I kernel/ -c -fno-builtin -o build/ide.o device/ide.c -fno-stack-protector
gcc -m32 -I lib/kernel/ -I lib/ -I kernel/ -c -fno-builtin -o build/stdio-kernel.o lib/kernel/stdio-kernel.c -fno-stack-protector



echo "           "
echo "文件系统的创建"
echo "gcc -m32 -I lib/kernel/ -I lib/ -I kernel/ -c -fno-builtin -o build/fs.o fs/fs.c -fno-stack-protector
gcc -m32 -I lib/kernel/ -I lib/ -I kernel/ -c -fno-builtin -o build/file.o fs/file.c -fno-stack-protector
gcc -m32 -I lib/kernel/ -I lib/ -I kernel/ -c -fno-builtin -o build/inode.o fs/inode.c -fno-stack-protector"

gcc -m32 -I lib/kernel/ -I lib/ -I kernel/ -c -fno-builtin -o build/fs.o fs/fs.c -fno-stack-protector
gcc -m32 -I lib/kernel/ -I lib/ -I kernel/ -c -fno-builtin -o build/file.o fs/file.c -fno-stack-protector
gcc -m32 -I lib/kernel/ -I lib/ -I kernel/ -c -fno-builtin -o build/inode.o fs/inode.c -fno-stack-protector
gcc -m32 -I lib/kernel/ -I lib/ -I kernel/ -c -fno-builtin -o build/dir.o fs/dir.c -fno-stack-protector



echo " "
echo "系统交互"
echo "gcc -m32 -I lib/kernel/ -I lib/ -I kernel/ -c -fno-builtin -o build/fork.o userprog/fork.c -fno-stack-protector
gcc -m32 -I lib/kernel/ -I lib/ -I kernel/ -c -fno-builtin -o build/assert.o lib/user/assert.c -fno-stack-protector
gcc -m32 -I lib/kernel/ -I lib/ -I kernel/ -c -fno-builtin -o build/shell.o shell/shell.c -fno-stack-protector
gcc -m32 -I lib/kernel/ -I lib/ -I kernel/ -c -fno-builtin -o build/buildin-cmd.o shell/buildin-cmd.c -fno-stack-protector
gcc -m32 -I lib/kernel/ -I lib/ -I kernel/ -c -fno-builtin -o build/exec.o userprog/exec.c -fno-stack-protector
gcc -m32 -I lib/kernel/ -I lib/ -I kernel/ -c -fno-builtin -o build/wait_exit.o userprog/wait_exit.c -fno-stack-protector
gcc -m32 -I lib/kernel/ -I lib/ -I kernel/ -c -fno-builtin -o build/pipe.o shell/pipe.c -fno-stack-protector
"
gcc -m32 -I lib/kernel/ -I lib/ -I kernel/ -c -fno-builtin -o build/fork.o userprog/fork.c -fno-stack-protector
gcc -m32 -I lib/kernel/ -I lib/ -I kernel/ -c -fno-builtin -o build/assert.o lib/user/assert.c -fno-stack-protector
gcc -m32 -I lib/kernel/ -I lib/ -I kernel/ -c -fno-builtin -o build/shell.o shell/shell.c -fno-stack-protector
gcc -m32 -I lib/kernel/ -I lib/ -I kernel/ -c -fno-builtin -o build/buildin-cmd.o shell/buildin-cmd.c -fno-stack-protector
gcc -m32 -I lib/kernel/ -I lib/ -I kernel/ -c -fno-builtin -o build/exec.o userprog/exec.c -fno-stack-protector
gcc -m32 -I lib/kernel/ -I lib/ -I kernel/ -c -fno-builtin -o build/wait_exit.o userprog/wait_exit.c -fno-stack-protector
gcc -m32 -I lib/kernel/ -I lib/ -I kernel/ -c -fno-builtin -o build/pipe.o shell/pipe.c -fno-stack-protector


echo "              "
echo "开始主函数的编译"
echo "gcc -m32 -I lib/kernel/ -I lib/ -I kernel/ -c -fno-builtin -o build/main.o kernel/main.c -fno-stack-protector"
gcc -m32 -I lib/kernel/ -I lib/ -I kernel/ -c -fno-builtin -o build/main.o kernel/main.c -fno-stack-protector


echo "                                                           "
echo "ld build/*.o to bin/kernel.bin"
ld -m elf_i386 -Ttext 0xc0001500 -e main -o bin/kernel.bin \
build/main.o    build/init.o    build/interrupt.o    build/print.o    build/kernel.o  build/timer.o \
build/debug.o   build/bitmap.o  build/memory.o       build/string.o   build/thread.o  build/list.o  \
build/switch.o  build/sync.o    build/console.o      build/keyboard.o build/ioqueue.o build/tss.o   \
build/process.o build/syscall.o build/syscall-init.o build/stdio.o    build/ide.o     build/stdio-kernel.o \
build/fs.o      build/file.o    build/inode.o        build/dir.o      build/fork.o    build/shell.o \
build/assert.o  build/buildin-cmd.o build/exec.o     build/wait_exit.o build/pipe.o



echo "                                                           "
echo "write bin/kernel.bin to the disk.img"
echo "dd if=bin/kernel.bin of=tool/bochs-2.6.11/disk.img bs=512 count=200 seek=9 conv=notrunc"
dd if=bin/kernel.bin of=tool/bochs-2.6.11/disk.img bs=512 count=200 seek=9 conv=notrunc


#echo " "
#echo "command"
#gcc -m32 -I lib/kernel/ -I lib/ -I kernel/ -c -fno-builtin -o command/prog_arg.o command/prog_arg.c -fno-stack-protector
#nasm -f elf -o command/start.bin command/start.S 
#dd if=command/start.bin of=/home/awei/bochs-2.6.11/disk.img bs=512 count=200 seek=300 conv=notrunc

echo "                                                            "
echo "cd tool/bochs-2.6.11/ and have your fun"
cd tool/bochs-2.6.11/
bochs -f bochsrc
