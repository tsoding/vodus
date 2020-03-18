with import <nixpkgs> {}; {
    vodusEnv = gcc8Stdenv.mkDerivation {
        name = "vodus-env";
        buildInputs = [ stdenv
                        gcc
                        gdb
                        pkgconfig
                        freetype
                        giflib
                        ffmpeg-full
                      ];
    };
}
