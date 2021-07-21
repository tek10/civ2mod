This is a program to modify saved game files for Civilization II MGE.  It is intented to complement the cheat mode by allowing you to change things that cheat mode will not.  It is written in C so that it runs very fast, doesn't depend on interpreters like java.exe or node.exe, and because I wanted to write something in C again.  It uses the format information from hexedit.rtf

usage: civ2mod savefile [newsavefile]

No makefile.  It was created using cygwin gcc on Windows 10.  To compile:

cc civ2mod.c -o civ2mod



