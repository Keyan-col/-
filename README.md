# -

windres -i lottery.rc -o lottery.res -O coff --include-dir=.
gcc lottery.c lottery.res -o lottery.exe -mwindows -lcomdlg32

Then you get lottery.exe to use. Fast and easy.
