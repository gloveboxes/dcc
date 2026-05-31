# dcc
C89 compiler targeting CP/M 2.2 on a Z80

## What dcc is
dcc implements most C89 syntax. It takes a .c file and generates a .MAC assembly file that can be assembled by M80 and linked by L80 to produce CP/M .COM files.

A separate app dccpeep.c is a peephole optimizer that rewrites portions of .MAC files so apps run faster. It's not necessary to use dccpeep; apps will work just fine without it. But if you need your app to be both smaller and faster it's worth running.

DCCRTL.MAC is the dcc C Runtime Library. It's written in Z80 assembly for size and performance. It has the entrypoint start for apps that initializes the heap (for malloc/free) and command-line arguments so main's argc and argv work. It implements a small subset of the C89 C runtime including floating point.

dccrtlstrip.c is an app that examines the code of your .c file and strips portions of the DCCRTL.MAC C runtime so only the parts needed are linked into the .COM file. It's not necessary to run this program for your app to work. But the resulting .COM file may be smaller if you do.

The 3 compiler apps dcc, dccpeep, and dccrtlstrip all build and run on Windows, Linux, and MacOS. They are too big to run on CP/M. Use m.bat or m.sh to build these apps using msvc and gcc respectively.

## How to build test apps and your apps

ma.bat and ma.sh are scripts to build your app. Run "ma foo" (or "ma.sh foo" on Linux/MacOS) to compile foo.c, optimize it, strip the DCCRTL.MAC runtime so unused code isn't included, assemble the generated FOO.MAC file, and link to FOO.COM. Use the "nopeep" argument like "ma foo nopeep" to not run the dccpeep peephole optimizer.

runall.bat and runall.sh compile and run all 90+ test cases both optimized and unoptimized. The output of that run is compared with baseline_test_dcc.txt to check for regressions.

Apps must all fit in one .C file for now. That file can include .H files. You can also include .C files from your main .C file to help keep code more organized if you want.

The test apps validate compiler correctness and performance. Some test apps are small and exercise a single compiler feature. Others are larger; tchess.c plays chess (not very well) and with the -c argument can play against itself. 

Linux typically is configured to have case-sensitive filenames. CP/M files are uppercase. The convention used is that source .c files have lowercase names since only dcc works with them. Assembly files (.MAC) are all uppercase, as are output files from m80.com and l80.com including .COM, .PRN, and .REL.

## Emulators

I use my [ntvcm](https://github.com/davidly/ntvcm) CP/M 2.2 emulator to run m80.com, l80.com, and apps built with dcc. The widely-used CPM emulator works equally well; all tests build and pass with that emulator. I haven't run other emulators but I suspect they'll all just work. The compiler and runtime don't push emulator compatibility limits.

## M80 and L80

m80.com and l80.com are part of the M80 Assembler product from Microsoft. I didn't write them. They are included in this repo to ease development, but they can be found in dozens of locations on the internet.

## C89+ language 

The compiler accepts some syntax from later C standards including declaring variables where you like and initializing them with complex expressions. Only 4-byte floats are supported; 8-byte doubles are not. I'm almost certain more arcane C expressions/features aren't implemented (yet), but the test cases have pretty good coverage. Only a small subset of the C runtime is implemented in DCCRTL.MAC, but the samples implement a bunch more that you can copy/paste where needed.

## Memory layout

Memory layout is what you would expect; CP/M loads .COM files in just one way. BSS begins just after the loaded image. The app assumes sp is set to the highest free byte by the loader. DCCRTL.MAC defines DEFAULT_STACK as 512. The heap used by malloc() uses RAM between the end of BSS and the bottom of the stack. If you need to adjust the heap and stack sizes you can change that one constant to slide the barrier. There are no runtime checks that prevent the stack from smashing the heap; you have to be careful that DEFAULT_STACK works for your app.

## Benchmarks

I ran a subset of the test apps to measure performance of the compiler relative to other compilers. Most of the compilers in the table below are era-appropriate, from the 1970's and 1980's. The ZCC/Z88DK compilers are from 2025. I chose the best CP/M compilers I could find for the comparison. My repo [cpm_compilers](https://github.com/davidly/cpm_compilers) has a more complete set along with runtimes for some of these performance benchmarks.

Generally, dcc compares very well with all other compilers that target CP/M, especially when the dccpeep optimizer is used. Even when the optimizer isn't used dcc only loses a few benchmarks. My assembly implementations of some of the benchmarks (asmsieve.mac, asme.mac, asmttt.mac) still beat all compilers. Binary size is also competitive with other compilers. It's not always best but it's always close. ZCC is very good at code generation but occasionally hard to work with.

### The benchmarks:

  - sieve.c: This is the classic from BYTE magazine in 1983. It measures loop and array performance.
  - e.c: This computes the first 192 digits of e. It measures integer division and mod operations as well as loop and array performance.
  - tm.c: Test Malloc. This is C-only and measures performance of the allocator as well as memset. Many of the C compilers for CP/M can't run it because they don't have an allocator or don't implement free().
  - ttt.c: Proves you can't win at tic-tac-toe if the opponent is competent. Tests function call performance as well as loop and array performance. Always remember it took WOPR 72 seconds to solve this problem in the 1983 movie War Games. A 2Mhz 8080 in 1974 could solve this in less than 3 seconds. Movie magic.
  - pihex.c: Computes PI in base 16. This is C-only and some of the compilers can't build or run it due to a variety of bugs. It measures unsigned long mod and floating point performance. I spent 90 minutes trying to get the two forms of ZCC to build and run it, ran into many compiler and C runtime bugs, and gave up. HiSoft v4.11 has a C runtime bug where if you cast 3.963512 to an int it gives you 4. After I worked around that and other bugs code from that compiler ran really well -- faster than dcc.
  - mm.c: Another BYTE magazine classic from October 1982. Measures floating point initialization, addition, and multiplication performance.

Benchmark times are in milliseconds on a 4Mhz Z80. CP/M file sizes are rounded up to the next multiple of 128 bytes due to how the file system works.

<img width="3455" height="1182" alt="table" src="https://github.com/user-attachments/assets/c6834233-14c1-487a-b03d-16d5a9087c54" />

## Notes

I built the compiler using AI. I wanted to use Claude and ChatGPT on something reasonably complicated. I used each about equally and found them both to be extemely helpful and infurriating at the same time. They are lazy, forgetful, brilliant, fast, insightful, and seemingly willfully ignorant. They remind me of the hundreds of people I worked with over the years. They wrote about half the test cases, Google wrote a few, and I had the rest from other projects. It took me hundreds of prompts to get the compiler this far along. I had to drive the architecture. I also had to do a bunch of the debugging when they got stuck. asme.mac, asmsieve.mac, and asmttt.mac are my assembly versions of the benchmarks. They proved useful in prompts to get the AIs to optimize their generated code.

Why dcc? All compilers from that era were K&R since the first ANSI C standard was C89 (1989). I wanted a compiler with modern syntax for CP/M. I was also curious how hard it would be to generate better code than the older compilers. It's  easier than ever to code for old machines, and I think that's pretty cool.
