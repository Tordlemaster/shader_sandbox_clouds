main: $(wildcard ./src/*)
	g++ -std=c++20 -fdiagnostics-color=always -g ./src/main.cpp ./src/glad.c -o ./main.exe -I./include -L./lib -lopengl32 -lglfw3 -lgdi32