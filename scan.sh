#!/bin/bash
# Generate wayland protocols
for protocol in $(find wayland-protocols -name '*.xml'); do
    name=$(basename "$protocol" .xml)
    mkdir -p protocols/"$name"
    wayland-scanner private-code "$protocol" protocols/"$name"/"$name"-private.h
    wayland-scanner public-code "$protocol" protocols/"$name"/"$name"-public.h
    wayland-scanner client-header "$protocol" protocols/"$name"-protocol.h
done

# Generate wlroots protocols
WLR_LAYER_SHELL_XML="wlr-layer-shell-unstable-v1.xml"
WLR_LAYER_SHELL_URL="https://gitlab.freedesktop.org/wlroots/wlroots/-/raw/0.19.0/protocol/wlr-layer-shell-unstable-v1.xml"

if [ ! -f "$WLR_LAYER_SHELL_XML" ]; then
    curl -L "$WLR_LAYER_SHELL_URL" -o "$WLR_LAYER_SHELL_XML"
fi

wayland-scanner client-header "$WLR_LAYER_SHELL_XML" protocols/wlr-layer-shell-unstable-v1-protocol.h
wayland-scanner private-code "$WLR_LAYER_SHELL_XML" protocols/wlr-layer-shell-unstable-v1-private.h
wayland-scanner public-code "$WLR_LAYER_SHELL_XML" protocols/wlr-layer-shell-unstable-v1-public.h

# Generate wlr-layer-shell-unstable-v1.h
cat << EOF > protocols/wlr-layer-shell-unstable-v1.h
#ifndef WLR_LAYER_SHELL_UNSTABLE_V1_H
#define WLR_LAYER_SHELL_UNSTABLE_V1_H
#include "wlr-layer-shell-unstable-v1-protocol.h"
#endif
EOF