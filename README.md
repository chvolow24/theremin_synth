# Theremin synth

## Install SDL
You can find more information online, but if you're on MacOS, you can probably use homebrew:
```console
$ brew install sdl2
```

On linux, try using apt:
```console
$ sudo apt-get install libsdl2-2.0-0 libsdl2-dev
```

### Compile the program

```console
$ gcc main.c $(sdl2-config --cflags --libs) -o run
```

### Run the program

```console
./run
```

Press *l* to play, *k* to pause.

