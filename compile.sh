WHAT="$1"

if [ -z "$WHAT" ]; then
    echo "Usage: $0 <what>"
    exit 1
fi

compile_server() {
    g++ -o server src/server.cpp -lraylib -lenet -fsanitize=thread -g -O1
}

compile_client() {
    g++ -o client src/client.cpp -lraylib -lenet
}

compile_windows() {
    RAYLIB_PATH="$1"
    x86_64-w64-mingw32-g++ -o game.exe src/client.cpp \
  -I$RAYLIB_PATH/src \
  $RAYLIB_PATH/libraylib.a \
  -lopengl32 -lgdi32 -lwinmm -lws2_32 -lenet
}

if [ "$WHAT" = "all" ]; then
    compile_server
    compile_client
elif [ "$WHAT" = "server" ]; then
    compile_server
elif [ "$WHAT" = "client" ]; then
    compile_client
elif [ "$WHAT" = "windows" ]; then
    if [ -z "$2" ]; then
        echo "Usage: $0 windows <path-to-raylib>"
        exit 1
    fi
    compile_windows "$2"
fi

