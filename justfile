make:
    gcc main.c ipc_server.c transform_matrix.c gl_shaders.c -o dist/icm -lwlroots-0.20 -lwayland-server -lm -lEGL -lGL -ldl -lxkbcommon -I/usr/include/wlroots-0.20 -I/usr/include/wayland-server -I/usr/include/wayland-server-core -I/usr/include/wayland-util -Iprotocols/ -I/usr/include/GL -I/usr/include/EGL -lX11 -lX11-xcb -lxcb -lxcb-render -lxcb-shape -lxcb-xfixes -lXrandr -lXcursor -lXinerama -lXcomposite -lXdamage -lXext -lXfixes -lXrender -lXv -lXxf86vm -lXrandr -DWLR_USE_UNSTABLE -I/usr/include/pixman-1 -I/usr/include/xcb -I/usr/include/xcb/render -I/usr/include/xcb/shape -I/usr/include/xcb/xfixes -I/usr/include/X11 -I/usr/include/X11/extensions -I/usr/include/X11/extensions/Xrandr -I/usr/include/X11/extensions/Xcursor -I/usr/include/X11/extensions/Xinerama -I/usr/include/X11/extensions/Xcomposite -I/usr/include/X11/extensions/Xdamage -I/usr/include/X11/extensions/Xext -I/usr/include/X11/extensions/Xfixes -I/usr/include/X11/extensions/Xrender -I/usr/include/X11/extensions/Xres -I/usr/include/X11/extensions/Xv -I/usr/include/X11/extensions/Xvmc -I/usr/include/X11/extensions/xf86vm -I/usr/include/GL -I/usr/include/EGL -Iprotocols/ -lfreetype -I/usr/include/freetype2 -I/usr/include/freetype2/freetype -I/usr/include/freetype2/ft2build -lfontconfig -I/usr/include/fontconfig $(pkg-config --cflags pangocairo) $(pkg-config --libs pangocairo)
    gcc icmi.c -o dist/icmi

scan:
    ./scan.sh

run: make
    ./dist/icm