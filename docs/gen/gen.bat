@echo off
cd E:\projects\luaserver\bin
luas doc.lua -menu E:\projects\reeme\docs\menu.lua -in E:\projects\reeme\docs\ -out E:\projects\reeme\docs\gen\out -template E:\projects\reeme\docs\gen\template\index.html
@echo on

@echo please press any key to exit...
pause